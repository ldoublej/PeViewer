#include <dependency_graph.h>
#include <pe_file_data_source.h>

#include <QCoreApplication>
#include <QSet>
#include <QTemporaryDir>

#include "components/dependency_tree_model.h"
#include "dependencies_test_support.h"

using namespace DependencyTests;

namespace {
using Status = DependencyTreeModel::Status;

Status status(const DependencyTreeModel& model, const QModelIndex& index) {
    return static_cast<Status>(model.data(index, DependencyTreeModel::StatusRole).toInt());
}

void apiSetHostReferences() {
    QTemporaryDir temporary;
    require(temporary.isValid(), "Temporary directory failed");
    const QDir dir(temporary.path());
    const auto processes = QStringLiteral("api-ms-win-core-processthreads-l1-1-0.dll");
    const auto io = QStringLiteral("api-ms-win-core-io-l1-1-0.dll");
    const auto eventing = QStringLiteral("api-ms-win-eventing-provider-l1-1-0.dll");
    writePe(dir, QStringLiteral("app.exe"), {QStringLiteral("kernel32.dll"), processes});
    writePe(dir, QStringLiteral("kernel32.dll"), {processes, io});
    writePe(dir, QStringLiteral("kernelbase.dll"), {eventing});
    FileLocatorOptions options;
    options.apiSet.addMapping(processes, QStringLiteral("kernel32.dll"));
    options.apiSet.addMapping(processes, QStringLiteral("kernelbase.dll"),
                             QStringLiteral("kernel32.dll"));
    options.apiSet.addMapping(io, QStringLiteral("kernel32.dll"));
    options.apiSet.addMapping(io, QStringLiteral("kernelbase.dll"), QStringLiteral("kernel32.dll"));
    options.apiSet.addMapping(eventing, QStringLiteral("kernelbase.dll"));
    PeFileDataSource source(dir.filePath(QStringLiteral("app.exe")));
    PeFile root(source);
    root.parse();
    PeFileStore files;
    DependencyGraph graph(root, files, options);
    graph.build();

    const auto& rootEdges = graph.root().dependencies();
    const auto* kernel32 = rootEdges[0].module;
    require(kernel32 && rootEdges[1].module == kernel32 && graph.moduleCount() == 3,
            "API Set aliases duplicated the host module");
    require(rootEdges[0].apiSetHost.isEmpty() &&
                rootEdges[1].apiSetHost == QStringLiteral("kernel32.dll"),
            "Cached host resolution leaked API Set metadata between import edges");
    const auto* kernelbase = kernel32->dependencies()[0].module;
    require(kernelbase && kernelbase != kernel32 &&
                QFileInfo(kernelbase->identity()).fileName() == QStringLiteral("kernelbase.dll"),
            "Kernel32 importer override did not select kernelbase");
    require(kernel32->dependencies()[1].module == kernelbase &&
                kernelbase->dependencies()[0].module == kernelbase,
            "Host sharing or legitimate API Set self-reference was lost");

    DependencyTreeModel model;
    model.setGraph(&graph);
    const auto rootIndex = model.index(0, 0);
    model.fetchMore(rootIndex);
    const auto kernelIndex = model.index(0, 0, rootIndex);
    const auto sharedAlias = model.index(1, 0, rootIndex);
    require(status(model, kernelIndex) == Status::Resolved &&
                status(model, sharedAlias) == Status::ApiSet && model.canFetchMore(sharedAlias),
            "An API Set pointing to a shared host was classified as a cycle");
    model.fetchMore(kernelIndex);
    const auto processesIndex = model.index(0, 0, kernelIndex);
    require(status(model, processesIndex) == Status::ApiSet && model.canFetchMore(processesIndex),
            "Kernel32's redirected import was reported as circular");
    model.fetchMore(processesIndex);
    const auto selfReference = model.index(0, 0, processesIndex);
    require(status(model, selfReference) == Status::ApiSet && !model.hasChildren(selfReference) &&
                !model.canFetchMore(selfReference),
            "API Set host self-reference was treated as a normal cycle or expanded infinitely");
    require(model.data(model.index(0, DependencyTreeModel::DetailColumn, processesIndex))
                .toString().contains(kernelbase->identity()),
            "API Set self-reference did not show its resolved host path");
    model.fetchMore(sharedAlias);
    require(status(model, model.index(0, 0, sharedAlias)) == Status::ApiSet,
            "A shared host was treated as globally visited instead of branch-local");
}

void realCyclesRemainVisible() {
    QTemporaryDir temporary;
    require(temporary.isValid(), "Temporary directory failed");
    const QDir dir(temporary.path());
    const auto contract = QStringLiteral("api-test-root-l1-1-0.dll");
    const auto unmapped = QStringLiteral("api-local-l1-1-0.dll");
    writePe(dir, QStringLiteral("app.exe"), {QStringLiteral("a.dll"), unmapped});
    writePe(dir, QStringLiteral("a.dll"), {QStringLiteral("b.dll"), QStringLiteral("a.dll")});
    writePe(dir, QStringLiteral("b.dll"), {contract});
    writePe(dir, unmapped, {unmapped});
    FileLocatorOptions options;
    options.apiSet.addMapping(contract, QStringLiteral("app.exe"));
    PeFileDataSource source(dir.filePath(QStringLiteral("app.exe")));
    PeFile root(source);
    root.parse();
    PeFileStore files;
    DependencyGraph graph(root, files, options);
    graph.build();
    DependencyTreeModel model;
    model.setGraph(&graph);
    const auto rootIndex = model.index(0, 0);
    model.fetchMore(rootIndex);
    const auto a = model.index(0, 0, rootIndex);
    model.fetchMore(a);
    const auto b = model.index(0, 0, a);
    const auto ordinarySelf = model.index(1, 0, a);
    require(status(model, ordinarySelf) == Status::Cycle && !model.canFetchMore(ordinarySelf),
            "Ordinary DLL self-import was hidden by the API Set fix");
    model.fetchMore(b);
    const auto realCycle = model.index(0, 0, b);
    require(status(model, realCycle) == Status::Cycle && !model.hasChildren(realCycle),
            "A real multi-module cycle through an API Set was hidden");
    const auto localApi = model.index(1, 0, rootIndex);
    model.fetchMore(localApi);
    require(status(model, model.index(0, 0, localApi)) == Status::Cycle,
            "An unmapped api-prefixed filename was incorrectly treated as a virtual contract");
}

void tests() {
    apiSetHostReferences();
    realCyclesRemainVisible();
}

// Optional integration check against a real executable and the local schema.
void inspectFile(const QString& path) {
    PeFileDataSource source(path);
    PeFile root(source);
    root.parse();
    PeFileStore files;
    DependencyGraph graph(root, files);
    graph.build();
    DependencyTreeModel model;
    model.setGraph(&graph);
    struct Pending {
        const ModuleNode* module;
        QModelIndex index;
    };
    std::vector<Pending> pending{{&graph.root(), model.index(0, 0)}};
    QSet<const ModuleNode*> visited;
    int apiSetEdges = 0;
    int selfReferences = 0;
    for (std::size_t i = 0; i < pending.size(); ++i) {
        const auto current = pending[i];
        if (visited.contains(current.module)) {
            continue;
        }
        visited.insert(current.module);
        model.fetchMore(current.index);
        const auto& edges = current.module->dependencies();
        for (std::size_t j = 0; j < edges.size(); ++j) {
            const auto& edge = edges[j];
            const auto child = model.index(static_cast<int>(j), 0, current.index);
            if (!edge.apiSetHost.isEmpty() && edge.module) {
                ++apiSetEdges;
                if (edge.module == current.module) {
                    ++selfReferences;
                    require(status(model, child) == Status::ApiSet && !model.hasChildren(child),
                            "Real API Set self-reference is still reported as circular");
                }
                if (QFileInfo(current.module->identity()).fileName() ==
                        QStringLiteral("kernel32.dll") &&
                    edge.dllName.startsWith(QStringLiteral("api-ms-win-core-processthreads-"))) {
                    require(edge.module != current.module && status(model, child) == Status::ApiSet,
                            "Real kernel32 processthreads import still resolves to itself");
                    std::cout << "kernel32.dll -> " << edge.dllName.toStdString() << " -> "
                              << edge.apiSetHost.toStdString() << '\n';
                }
            }
            if (edge.module) {
                pending.push_back({edge.module, child});
            }
        }
    }
    std::cout << "Inspected " << graph.moduleCount() << " modules, " << apiSetEdges
              << " API Set edges, " << selfReferences << " host self-references\n";
}
}  // namespace

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    const int result = run(tests);
    if (result != 0 || app.arguments().size() < 2) {
        return result;
    }
    try {
        inspectFile(app.arguments()[1]);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
