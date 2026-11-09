#pragma once

#include <QObject>
#include <QString>
#include <QtGlobal>
#include <cstddef>
#include <memory>
#include <vector>

class Workspace;

// Singleton that owns every open Workspace and tracks the active one.
class WorkspaceManager : public QObject {
    Q_OBJECT

public:
    enum class OpenMode { OpenOnly, OpenAndActivate };
    Q_ENUM(OpenMode)

    static WorkspaceManager* instance();

    Q_DISABLE_COPY_MOVE(WorkspaceManager)

    // Constructs a new workspace and appends it; activates it if requested.
    // Throws on load/parse/build failure; the existing list is left untouched.
    // If path is already open, reuses it and applies the requested mode.
    Workspace* open(const QString& path, OpenMode mode = OpenMode::OpenAndActivate);

    Workspace* find(const QString& path) const;
    bool activate(Workspace* workspace);

    // Removes the workspace. If it was current, the most recently opened
    // remaining workspace becomes current.
    bool close(Workspace* workspace);
    void closeAll();

    Workspace* current() const { return m_current; }
    std::size_t size() const { return m_workspaces.size(); }
    bool empty() const { return m_workspaces.empty(); }
    const std::vector<std::unique_ptr<Workspace>>& workspaces() const { return m_workspaces; }

signals:
    void workspaceOpened(Workspace* workspace);
    // Removed from the list, but still alive during synchronous delivery.
    void workspaceClosed(Workspace* workspace);
    // Emitted only when current() changes; nullptr means no active workspace.
    void workspaceActivated(Workspace* workspace);

private:
    WorkspaceManager();
    ~WorkspaceManager() override;

    std::vector<std::unique_ptr<Workspace>> m_workspaces;
    Workspace* m_current = nullptr;
};
