#include "workspace_manager.h"

WorkspaceManager& WorkspaceManager::instance() {
    static WorkspaceManager manager;
    return manager;
}

WorkspaceManager::WorkspaceManager(){
    
}
