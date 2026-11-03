#pragma once

#include <QString>
#include <QtGlobal>
#include <cstddef>
#include <memory>
#include <vector>

class Workspace;

// Owns every open Workspace and tracks the active one. Opening a path that is
// already open activates the existing workspace instead of duplicating it.
class WorkspaceManager {
public:
    WorkspaceManager() = default;
    ~WorkspaceManager();

    Q_DISABLE_COPY_MOVE(WorkspaceManager)

    // Constructs a new workspace, appends it and makes it current.
    // Throws on load/parse/build failure; the existing list is left untouched.
    // If path is already open, activates that workspace and returns it.
    Workspace* open(const QString& path);

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

private:
    std::vector<std::unique_ptr<Workspace>> m_workspaces;
    Workspace* m_current = nullptr;
};
