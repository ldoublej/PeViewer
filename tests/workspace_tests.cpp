#include <pe_data_source.h>
#include <pe_file.h>
#include <workspace.h>
#include <workspace_manager.h>

#include <QDir>
#include <QFileInfo>
#include <QTemporaryDir>
#include <stdexcept>

#include "dependencies_test_support.h"
#include "dependency_graph.h"

using namespace DependencyTests;

namespace {
QString writeFixture(const QDir& dir, const QString& name,
                     const QStringList& imports = QStringList{}) {
    writePe(dir, name, imports);
    return dir.filePath(name);
}

void opensSession() {
    QTemporaryDir temporary;
    require(temporary.isValid(), "Temporary directory failed");
    const QDir dir(temporary.path());
    const QString path =
        writeFixture(dir, QStringLiteral("app.exe"), {QStringLiteral("a.dll")});
    writeFixture(dir, QStringLiteral("a.dll"));

    Workspace workspace(path, FileLocatorOptions{});
    require(workspace.path() == QFileInfo(path).absoluteFilePath(),
            "Workspace did not record the absolute path");
    require(workspace.peFile().isParsed(), "Workspace did not parse the root file");
    require(&workspace.graph().root().pe_file() == &workspace.peFile(),
            "Graph does not borrow the workspace root");
    require(workspace.graph().moduleCount() == 2, "Dependency graph was not built");
    require(workspace.files().size() == 1 && workspace.graph().files().size() == 1,
            "Workspace does not expose its dependency store");
}

void managerTracksList() {
    QTemporaryDir temporary;
    require(temporary.isValid(), "Temporary directory failed");
    const QDir dir(temporary.path());
    const QString firstPath =
        writeFixture(dir, QStringLiteral("first.exe"), {QStringLiteral("a.dll")});
    writeFixture(dir, QStringLiteral("a.dll"));
    const QString secondPath =
        writeFixture(dir, QStringLiteral("second.exe"), {QStringLiteral("b.dll")});
    writeFixture(dir, QStringLiteral("b.dll"));

    WorkspaceManager manager;
    require(manager.current() == nullptr && manager.empty() && manager.size() == 0,
            "Manager is not empty");

    Workspace* first = manager.open(firstPath);
    require(first != nullptr && manager.current() == first && manager.size() == 1,
            "Manager did not open a workspace");
    require(first->graph().moduleCount() == 2, "First workspace graph is incomplete");

    Workspace* second = manager.open(secondPath);
    require(second != nullptr && second != first && manager.current() == second,
            "Manager did not activate the new workspace");
    require(manager.size() == 2 && manager.workspaces()[0].get() == first,
            "Manager did not keep the previous workspace");

    // Reopening an already-open path activates it instead of duplicating it.
    require(manager.open(firstPath) == first && manager.current() == first &&
                manager.size() == 2,
            "Reopening a path duplicated the workspace");
    require(manager.find(secondPath) == second, "find() did not locate the workspace");

    // A failed open must keep the list and the current session intact.
    expectException<PeDataSourceError>(
        [&] { manager.open(dir.filePath(QStringLiteral("missing.exe"))); });
    require(manager.current() == first && manager.size() == 2,
            "Failed open changed the workspace list");

    writeFile(dir.filePath(QStringLiteral("bad.exe")), QByteArray("invalid PE"));
    expectException<ParseError>(
        [&] { manager.open(dir.filePath(QStringLiteral("bad.exe"))); });
    require(manager.current() == first && manager.size() == 2,
            "Failed parse changed the workspace list");

    require(manager.activate(second) && manager.current() == second,
            "activate() did not change the current workspace");
    require(!manager.activate(nullptr), "activate() accepted an unknown workspace");

    require(manager.close(second) && manager.size() == 1 && manager.current() == first,
            "Closing the current workspace did not activate another one");
    require(!manager.close(second), "close() accepted a removed workspace");

    manager.closeAll();
    require(manager.empty() && manager.current() == nullptr, "closeAll() kept workspaces");
}

void tests() {
    opensSession();
    managerTracksList();
}
}  // namespace

int main() { return run(tests); }
