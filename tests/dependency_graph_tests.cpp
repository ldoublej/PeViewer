#include <dependency_graph.h>
#include <pe_file_data_source.h>

#include <QTemporaryDir>
#include <memory>

#include "dependencies_test_support.h"

using namespace DependencyTests;

namespace {
const DependencyEdge& dependency(const ModuleNode& module, const QString& name) {
    for (const auto& entry : module.dependencies()) {
        if (entry.dllName == name) {
            return entry;
        }
    }
    throw std::runtime_error("Dependency edge was not recorded");
}

const ModuleNode& resolved(const ModuleNode& module, const QString& name) {
    const auto& entry = dependency(module, name);
    require(entry.isResolved() && !entry.error, "Expected resolved dependency");
    return *entry.module;
}

void fileGraph() {
    QTemporaryDir temporary;
    require(temporary.isValid(), "Temporary directory failed");
    const QDir dir(temporary.path());
    writePe(dir, QStringLiteral("app.exe"),
            {QStringLiteral("a.dll"), QStringLiteral("b.dll"), QStringLiteral("missing.dll"),
             QStringLiteral("bad.dll")});
    writePe(dir, QStringLiteral("a.dll"),
            {QStringLiteral("shared.dll"), QStringLiteral("APP.EXE")});
    writePe(dir, QStringLiteral("b.dll"), {QStringLiteral("SHARED.DLL")});
    writePe(dir, QStringLiteral("shared.dll"), {QStringLiteral("a.dll")});
    writeFile(dir.filePath(QStringLiteral("bad.dll")), QByteArray("invalid PE"));
    PeFileDataSource source(dir.filePath(QStringLiteral("app.exe")));
    PeFile root(source);
    root.parse();
    PeFileStore files;
    DependencyGraph graph(root, files, FileLocatorOptions{});
    const auto* rootNode = &graph.root();
    require(&rootNode->pe_file() == &root && rootNode->dependencies().empty(),
            "Root is not borrowed before build");
    graph.build();
    const auto& a = resolved(graph.root(), QStringLiteral("a.dll"));
    const auto& b = resolved(graph.root(), QStringLiteral("b.dll"));
    const auto& shared = resolved(a, QStringLiteral("shared.dll"));
    require(&resolved(b, QStringLiteral("SHARED.DLL")) == &shared,
            "Shared dependency was duplicated");
    require(&resolved(a, QStringLiteral("APP.EXE")) == rootNode, "Root cycle did not reuse root");
    require(&resolved(shared, QStringLiteral("a.dll")) == &a, "Child cycle was not merged");
    require(shared.dependents() == std::vector<const ModuleNode*>{&a, &b},
            "Reverse edges are incorrect");
    require(a.dependents() == std::vector<const ModuleNode*>{rootNode, &shared},
            "Cycle reverse edges are incorrect");
    require(graph.moduleCount() == 4, "Unexpected module count");
    require(graph.files().size() == 3, "Store did not retain parsed dependency files");
    const auto& edges = graph.root().dependencies();
    require(edges.size() == 4 && edges[0].dllName == QStringLiteral("a.dll") &&
                edges[1].dllName == QStringLiteral("b.dll"),
            "Import order was lost");
    require(!edges[2].module && edges[2].error->code == DependencyError::Code::NotFound,
            "Missing module error was not retained");
    require(!edges[3].module && edges[3].error->code == DependencyError::Code::ParseError,
            "Invalid PE error was not retained");
    require(edges[3].error->exception != nullptr, "Original parse exception was discarded");

    // Root identity is stable; rebuilding reopens children and refreshes failed edges.
    writePe(dir, QStringLiteral("missing.dll"));
    writePe(dir, QStringLiteral("a.dll"), {QStringLiteral("missing.dll")});
    graph.build();
    require(&graph.root() == rootNode && &graph.root().pe_file() == &root,
            "Rebuild replaced the root");
    const auto& missing = resolved(graph.root(), QStringLiteral("missing.dll"));
    const auto& rebuiltA = resolved(graph.root(), QStringLiteral("a.dll"));
    require(&resolved(rebuiltA, QStringLiteral("missing.dll")) == &missing,
            "Rebuild reused old results");
    require(graph.root().dependents().empty(), "Rebuild retained obsolete reverse edges");
    require(graph.files().size() == 4, "Rebuild did not refresh the store");
    require(root.imports().size() == 4, "Graph modified borrowed root imports");
}

struct SourceState {
    QByteArray bytes;
    int reads = 0;
    int alive = 0;
    bool failReads = false;
};

class CountingSource final : public PeDataSource {
public:
    explicit CountingSource(std::shared_ptr<SourceState> state) : m_state(std::move(state)) {
        ++m_state->alive;
    }
    ~CountingSource() override { --m_state->alive; }
    quint64 length() const override { return m_state->bytes.size(); }
    bool isFileAligned() const override { return true; }
    quint64 imageBase() const override { return 0; }
    QString url() const override { return QStringLiteral("test://module"); }
    void readExact(quint64 offset, char* buffer, std::size_t count) const override {
        ++m_state->reads;
        if (m_state->failReads) {
            throw PeDataSourceError(PeDataSourceError::Code::Io,
                                    QStringLiteral("Injected read error"), offset, count);
        }
        if (offset > length() || count > length() - offset) {
            throw PeDataSourceError(PeDataSourceError::Code::OutOfBounds,
                                    QStringLiteral("Fixture bounds"), offset, count);
        }
        std::memcpy(buffer, m_state->bytes.constData() + offset, count);
    }

private:
    std::shared_ptr<SourceState> m_state;
};

class TestLocator final : public ModuleLocator {
public:
    QHash<QString, std::pair<QString, std::shared_ptr<SourceState>>> modules;
    std::optional<LocatedModule> findModule(const QString& name) const override {
        if (name == QStringLiteral("search_error.dll")) {
            throw PeDataSourceError(PeDataSourceError::Code::Io,
                                    QStringLiteral("Injected open error"));
        }
        const auto found = modules.constFind(name.toLower());
        if (found == modules.cend()) {
            return std::nullopt;
        }
        return LocatedModule{found->first, std::make_unique<CountingSource>(found->second)};
    }
};

void customSources() {
    auto rootState = std::make_shared<SourceState>();
    rootState->bytes =
        makePe({QStringLiteral("child.dll"), QStringLiteral("alias.dll"), QStringLiteral("bad.dll"),
                QStringLiteral("bad_alias.dll"), QStringLiteral("wide.dll"),
                QStringLiteral("wide_alias.dll"), QStringLiteral("read_error.dll"),
                QStringLiteral("search_error.dll")});
    auto child = std::make_shared<SourceState>();
    child->bytes = makePe({QStringLiteral("root.dll")});
    auto bad = std::make_shared<SourceState>();
    bad->bytes = QByteArray(64, '\0');
    auto wide = std::make_shared<SourceState>();
    wide->bytes = makePe({}, true);
    auto unreadable = std::make_shared<SourceState>();
    unreadable->bytes = makePe();
    unreadable->failReads = true;
    CountingSource source(rootState);
    PeFile root(source);
    root.parse();
    const auto rootReads = rootState->reads;
    auto locator = std::make_unique<TestLocator>();
    locator->modules = {
        {QStringLiteral("root.dll"), {QStringLiteral("root"), rootState}},
        {QStringLiteral("child.dll"), {QStringLiteral("child"), child}},
        {QStringLiteral("alias.dll"), {QStringLiteral("child"), child}},
        {QStringLiteral("bad.dll"), {QStringLiteral("bad"), bad}},
        {QStringLiteral("bad_alias.dll"), {QStringLiteral("bad"), bad}},
        {QStringLiteral("wide.dll"), {QStringLiteral("wide"), wide}},
        {QStringLiteral("wide_alias.dll"), {QStringLiteral("wide"), wide}},
        {QStringLiteral("read_error.dll"), {QStringLiteral("read_error"), unreadable}}};
    {
        PeFileStore files;
        DependencyGraph graph(root, files, QStringLiteral("root"), std::move(locator));
        graph.build();
        const auto& childNode = resolved(graph.root(), QStringLiteral("child.dll"));
        require(&resolved(graph.root(), QStringLiteral("alias.dll")) == &childNode,
                "Custom identity aliases were not merged");
        require(&resolved(childNode, QStringLiteral("root.dll")) == &graph.root(),
                "Custom source root cycle failed");
        require(childNode.dependents().size() == 1, "Reverse edges contain duplicate parent");
        require(rootState->reads == rootReads, "Borrowed root was parsed again");
        require(bad->reads == 1, "Failed module was parsed again through alias");
        require(graph.moduleCount() == 2 && child->alive == 1, "Child ownership is incorrect");
        require(bad->alive == 0 && unreadable->alive == 0,
                "Failed dependency leaked its data source");
        require(wide->alive == 1, "Architecture mismatch was not retained by the store");
        require(graph.files().size() == 2 && graph.files().contains(QStringLiteral("child")) &&
                    graph.files().contains(QStringLiteral("wide")) &&
                    graph.files().find(QStringLiteral("root")) == nullptr,
                "Store contents do not match parsed dependencies");
        const auto& mismatch = dependency(graph.root(), QStringLiteral("wide.dll"));
        require(!mismatch.module &&
                    mismatch.error->code == DependencyError::Code::ArchitectureMismatch &&
                    !mismatch.error->expectedPe32Plus && mismatch.error->actualPe32Plus,
                "Architecture mismatch was not retained");
        const auto wideReads = wide->reads;
        CountingSource wideSource(wide);
        PeFile wideFile(wideSource);
        wideFile.parse();
        require(wide->reads == wideReads * 2, "Architecture failure was not cached by identity");
        for (const auto& name :
             {QStringLiteral("read_error.dll"), QStringLiteral("search_error.dll")}) {
            const auto& edge = dependency(graph.root(), name);
            require(!edge.module && edge.error->code == DependencyError::Code::ReadError &&
                        edge.error->exception,
                    "Read error was not retained");
            expectException<PeDataSourceError>(
                [&] { std::rethrow_exception(edge.error->exception); });
        }
        const auto childReads = child->reads;
        graph.build();
        require(child->alive == 1 && child->reads == childReads * 2,
                "Rebuild did not release/reparse owned children");
        require(rootState->reads == rootReads && rootState->alive == 1,
                "Graph took ownership of root data source");
    }
    require(child->alive == 0 && wide->alive == 0 && rootState->alive == 1,
            "Graph destruction violated source ownership");
}

void configuredPathsAndValidation() {
    QTemporaryDir temporary;
    require(temporary.isValid(), "Temporary directory failed");
    const QDir dir(temporary.path());
    writePe(dir, QStringLiteral("app/app.exe"),
            {QStringLiteral("extra.dll"), QStringLiteral("work.dll"), QStringLiteral("local.dll")});
    writePe(dir, QStringLiteral("app/local.dll"));
    writePe(dir, QStringLiteral("extra/extra.dll"));
    writePe(dir, QStringLiteral("work/work.dll"));
    PeFileDataSource source(dir.filePath(QStringLiteral("app/app.exe")));
    PeFile root(source);
    PeFileStore files;
    expectException<std::invalid_argument>([&] {
        DependencyGraph graph(root, files, QStringLiteral("root"), std::make_unique<TestLocator>());
    });
    root.parse();
    expectException<std::invalid_argument>(
        [&] { DependencyGraph graph(root, files, QStringLiteral("root"), nullptr); });
    expectException<std::invalid_argument>(
        [&] { DependencyGraph graph(root, files, QString{}, std::make_unique<TestLocator>()); });
    FileLocatorOptions options;
    options.workDirectory = dir.filePath(QStringLiteral("work"));
    options.fallbackDirectories = QStringList{dir.filePath(QStringLiteral("extra"))};
    options.appDirectory = dir.filePath(QStringLiteral("incorrect_app"));
    options.isPe32Plus = true;  // The root's 32-bit format overrides this hint.
    DependencyGraph graph(root, files, options);
    graph.build();
    require(graph.moduleCount() == 4,
            "Configured application/fallback/work directories were not searched");
    for (const auto& edge : graph.root().dependencies()) {
        require(edge.isResolved(),
                "An unrelated failure prevented other dependencies from resolving");
    }
}

void longChain() {
    auto rootState = std::make_shared<SourceState>();
    rootState->bytes = makePe({QStringLiteral("0.dll")});
    CountingSource source(rootState);
    PeFile root(source);
    root.parse();
    auto locator = std::make_unique<TestLocator>();
    constexpr int Count = 512;
    std::vector<std::shared_ptr<SourceState>> states;
    for (int i = 0; i < Count; ++i) {
        auto state = std::make_shared<SourceState>();
        const auto next =
            i + 1 == Count ? QStringLiteral("root.dll") : QStringLiteral("%1.dll").arg(i + 1);
        state->bytes = makePe({next});
        const auto name = QStringLiteral("%1.dll").arg(i);
        locator->modules.insert(name, {name, state});
        states.push_back(std::move(state));
    }
    locator->modules.insert(QStringLiteral("root.dll"), {QStringLiteral("root"), rootState});
    {
        PeFileStore files;
        DependencyGraph graph(root, files, QStringLiteral("root"), std::move(locator));
        graph.build();
        require(graph.moduleCount() == Count + 1, "Long dependency chain was truncated");
        const auto* node = &graph.root();
        for (int i = 0; i <= Count; ++i) {
            require(node->dependencies().size() == 1 && node->dependencies()[0].isResolved(),
                    "Long dependency chain contains an unresolved edge");
            node = node->dependencies()[0].module;
        }
        require(node == &graph.root(), "Long dependency cycle failed to close");
    }
    for (const auto& state : states) {
        require(state->alive == 0, "Long dependency graph leaked an owned source");
    }
}

void tests() {
    fileGraph();
    customSources();
    configuredPathsAndValidation();
    longChain();
}
}  // namespace

int main() { return run(tests); }
