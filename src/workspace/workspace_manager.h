#pragma once

class WorkspaceManager {
public:
    static WorkspaceManager& instance();

private:
    Q_DISABLE_COPY_MOVE(WorkspaceManager)
    WorkspaceManager();
    ~WorkspaceManager() = default;
};
