#include "workspace_manager.h"

#include <QFileInfo>
#include <algorithm>
#include <utility>

#include "workspace.h"

namespace {
QString canonicalPath(const QString& path) {
    const QFileInfo info(path);
    const QString canonical = info.canonicalFilePath();
    return canonical.isEmpty() ? info.absoluteFilePath() : canonical;
}

bool samePath(const QString& left, const QString& right) {
#ifdef Q_OS_WIN
    return left.compare(right, Qt::CaseInsensitive) == 0;
#else
    return left == right;
#endif
}
}  // namespace

WorkspaceManager* WorkspaceManager::instance() {
    static WorkspaceManager manager;
    return &manager;
}

WorkspaceManager::WorkspaceManager() = default;
WorkspaceManager::~WorkspaceManager() = default;

Workspace* WorkspaceManager::open(const QString& path, OpenMode mode) {
    if (Workspace* workspace = find(path)) {
        if (mode == OpenMode::OpenAndActivate) {
            activate(workspace);
        }
        return workspace;
    }

    // Build first so a failure leaves the list and current workspace intact.
    auto workspace = std::make_unique<Workspace>(path);
    Workspace* opened = workspace.get();
    m_workspaces.push_back(std::move(workspace));
    emit workspaceOpened(opened);
    if (mode == OpenMode::OpenAndActivate) {
        activate(opened);
    }
    return opened;
}

Workspace* WorkspaceManager::find(const QString& path) const {
    const QString wanted = canonicalPath(path);
    for (const auto& workspace : m_workspaces) {
        if (samePath(canonicalPath(workspace->path()), wanted)) {
            return workspace.get();
        }
    }
    return nullptr;
}

bool WorkspaceManager::activate(Workspace* workspace) {
    for (const auto& entry : m_workspaces) {
        if (entry.get() == workspace) {
            if (m_current != workspace) {
                m_current = workspace;
                emit workspaceActivated(workspace);
            }
            return true;
        }
    }
    return false;
}

bool WorkspaceManager::close(Workspace* workspace) {
    const auto found = std::find_if(
        m_workspaces.begin(), m_workspaces.end(),
        [workspace](const std::unique_ptr<Workspace>& entry) { return entry.get() == workspace; });
    if (found == m_workspaces.end()) {
        return false;
    }

    const bool wasCurrent = m_current == workspace;
    auto closing = std::move(*found);
    m_workspaces.erase(found);
    if (wasCurrent) {
        m_current = m_workspaces.empty() ? nullptr : m_workspaces.back().get();
        emit workspaceActivated(m_current);
    }
    emit workspaceClosed(closing.get());
    return true;
}

void WorkspaceManager::closeAll() {
    auto closing = std::move(m_workspaces);
    m_workspaces.clear();
    const bool hadCurrent = m_current != nullptr;
    m_current = nullptr;
    if (hadCurrent) {
        emit workspaceActivated(nullptr);
    }
    for (const auto& workspace : closing) {
        emit workspaceClosed(workspace.get());
    }
}
