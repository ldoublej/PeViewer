#include <pe_data_source.h>
#include <pe_file.h>
#include <workspace.h>
#include <workspace_manager.h>

#include <QDir>
#include <QFileInfo>
#include <QTemporaryDir>
#include <stdexcept>
#include <type_traits>
#include <vector>

#include "dependencies_test_support.h"
#include "dependency_graph.h"

using namespace DependencyTests;

namespace {
static_assert(!std::is_default_constructible_v<WorkspaceManager>);
static_assert(!std::is_copy_constructible_v<WorkspaceManager>);
static_assert(!std::is_move_constructible_v<WorkspaceManager>);

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

    auto* manager = WorkspaceManager::instance();
    require(manager == WorkspaceManager::instance(), "Manager is not a singleton");
    require(manager->current() == nullptr && manager->empty() && manager->size() == 0,
            "Manager is not empty");

    Workspace* first = manager->open(firstPath);
    require(first != nullptr && manager->current() == first && manager->size() == 1,
            "Manager did not open a workspace");
    require(WorkspaceManager::instance()->find(firstPath) == first,
            "Singleton access did not share the workspace list");
    require(first->graph().moduleCount() == 2, "First workspace graph is incomplete");

    Workspace* second = manager->open(secondPath);
    require(second != nullptr && second != first && manager->current() == second,
            "Manager did not activate the new workspace");
    require(manager->size() == 2 && manager->workspaces()[0].get() == first,
            "Manager did not keep the previous workspace");

    // Reopening an already-open path activates it instead of duplicating it.
    require(manager->open(firstPath) == first && manager->current() == first &&
                manager->size() == 2,
            "Reopening a path duplicated the workspace");
    require(manager->find(secondPath) == second, "find() did not locate the workspace");

    // A failed open must keep the list and the current session intact.
    expectException<PeDataSourceError>(
        [&] { manager->open(dir.filePath(QStringLiteral("missing.exe"))); });
    require(manager->current() == first && manager->size() == 2,
            "Failed open changed the workspace list");

    writeFile(dir.filePath(QStringLiteral("bad.exe")), QByteArray("invalid PE"));
    expectException<ParseError>(
        [&] { manager->open(dir.filePath(QStringLiteral("bad.exe"))); });
    require(manager->current() == first && manager->size() == 2,
            "Failed parse changed the workspace list");

    require(manager->activate(second) && manager->current() == second,
            "activate() did not change the current workspace");
    require(!manager->activate(nullptr), "activate() accepted an unknown workspace");

    require(manager->close(second) && manager->size() == 1 && manager->current() == first,
            "Closing the current workspace did not activate another one");
    require(!manager->close(second), "close() accepted a removed workspace");

    manager->closeAll();
    require(manager->empty() && manager->current() == nullptr, "closeAll() kept workspaces");
}

void managerSignalsAndOpenModes() {
    QTemporaryDir temporary;
    require(temporary.isValid(), "Temporary directory failed");
    const QDir dir(temporary.path());
    const QString firstPath = writeFixture(dir, QStringLiteral("first.exe"));
    const QString secondPath = writeFixture(dir, QStringLiteral("second.exe"));

    auto* manager = WorkspaceManager::instance();
    using OpenMode = WorkspaceManager::OpenMode;
    std::vector<QString> events;
    QObject context;
    QObject::connect(manager, &WorkspaceManager::workspaceOpened, &context,
                     [&](Workspace* workspace) {
                         require(manager->find(workspace->path()) == workspace,
                                 "Opened signal preceded insertion");
                         events.push_back(QStringLiteral("opened:") +
                                          QFileInfo(workspace->path()).fileName());
                     });
    QObject::connect(manager, &WorkspaceManager::workspaceActivated, &context,
                     [&](Workspace* workspace) {
                         require(manager->current() == workspace,
                                 "Activated signal preceded current change");
                         events.push_back(QStringLiteral("activated:") +
                                          (workspace ? QFileInfo(workspace->path()).fileName()
                                                     : QStringLiteral("none")));
                     });
    QObject::connect(manager, &WorkspaceManager::workspaceClosed, &context,
                     [&](Workspace* workspace) {
                         require(manager->find(workspace->path()) == nullptr &&
                                     manager->current() != workspace,
                                 "Closed signal preceded removal");
                         require(workspace->peFile().isParsed(),
                                 "Closed workspace is not readable during signal delivery");
                         events.push_back(QStringLiteral("closed:") +
                                          QFileInfo(workspace->path()).fileName());
                     });
    const auto expectEvents = [&](const std::vector<QString>& expected) {
        require(events == expected, "Unexpected workspace signals or signal order");
        events.clear();
    };

    Workspace* first = manager->open(firstPath, OpenMode::OpenOnly);
    require(manager->current() == nullptr, "OpenOnly activated the first workspace");
    expectEvents({QStringLiteral("opened:first.exe")});
    require(manager->open(firstPath, OpenMode::OpenOnly) == first,
            "OpenOnly duplicated an existing workspace");
    expectEvents({});

    Workspace* second = manager->open(secondPath, OpenMode::OpenOnly);
    require(manager->current() == nullptr, "OpenOnly changed the current workspace");
    expectEvents({QStringLiteral("opened:second.exe")});
    require(manager->close(first) && manager->current() == nullptr,
            "Closing an inactive workspace activated another one");
    expectEvents({QStringLiteral("closed:first.exe")});

    require(manager->activate(second), "Could not activate an opened workspace");
    expectEvents({QStringLiteral("activated:second.exe")});
    require(manager->activate(second), "Repeated activation failed");
    require(!manager->activate(nullptr), "Null activation succeeded");
    expectEvents({});

    first = manager->open(firstPath, OpenMode::OpenAndActivate);
    require(manager->current() == first, "OpenAndActivate did not activate the workspace");
    expectEvents({QStringLiteral("opened:first.exe"), QStringLiteral("activated:first.exe")});
    require(manager->open(secondPath, OpenMode::OpenOnly) == second &&
                manager->current() == first,
            "Reopening with OpenOnly changed the current workspace");
    expectEvents({});
    require(manager->open(secondPath) == second && manager->current() == second,
            "Default open did not activate an existing workspace");
    expectEvents({QStringLiteral("activated:second.exe")});
    manager->open(secondPath);
    expectEvents({});

    expectException<PeDataSourceError>(
        [&] { manager->open(dir.filePath(QStringLiteral("missing.exe"))); });
    writeFile(dir.filePath(QStringLiteral("bad.exe")), QByteArray("invalid PE"));
    expectException<ParseError>(
        [&] { manager->open(dir.filePath(QStringLiteral("bad.exe")), OpenMode::OpenOnly); });
    require(manager->current() == second && manager->size() == 2,
            "Failed open changed the manager");
    expectEvents({});

    require(manager->close(first), "Could not close an inactive workspace");
    expectEvents({QStringLiteral("closed:first.exe")});
    require(manager->close(second) && manager->current() == nullptr,
            "Closing the last workspace kept it active");
    expectEvents({QStringLiteral("activated:none"), QStringLiteral("closed:second.exe")});
    require(!manager->close(nullptr), "Closing an unknown workspace succeeded");
    expectEvents({});

    first = manager->open(firstPath);
    second = manager->open(secondPath);
    expectEvents({QStringLiteral("opened:first.exe"), QStringLiteral("activated:first.exe"),
                  QStringLiteral("opened:second.exe"), QStringLiteral("activated:second.exe")});
    require(manager->close(second) && manager->current() == first,
            "Closing the current workspace did not activate the remaining one");
    expectEvents({QStringLiteral("activated:first.exe"), QStringLiteral("closed:second.exe")});
    manager->open(secondPath, OpenMode::OpenOnly);
    expectEvents({QStringLiteral("opened:second.exe")});
    manager->closeAll();
    require(manager->empty() && manager->current() == nullptr, "closeAll() kept workspaces");
    expectEvents({QStringLiteral("activated:none"), QStringLiteral("closed:first.exe"),
                  QStringLiteral("closed:second.exe")});
    manager->closeAll();
    expectEvents({});

    manager->open(firstPath, OpenMode::OpenOnly);
    manager->open(secondPath, OpenMode::OpenOnly);
    manager->closeAll();
    require(manager->current() == nullptr, "Closing inactive workspaces activated one");
    expectEvents({QStringLiteral("opened:first.exe"), QStringLiteral("opened:second.exe"),
                  QStringLiteral("closed:first.exe"), QStringLiteral("closed:second.exe")});
}

void tests() {
    opensSession();
    managerTracksList();
    managerSignalsAndOpenModes();
}
}  // namespace

int main() { return run(tests); }
