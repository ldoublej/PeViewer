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

WorkspaceManager::~WorkspaceManager() = default;

Workspace* WorkspaceManager::open(const QString& path) {
    const QString wanted = canonicalPath(path);
    for (const auto& workspace : m_workspaces) {
        if (samePath(canonicalPath(workspace->path()), wanted)) {
            m_current = workspace.get();
            return m_current;
        }
    }

    // Build first so a failure leaves the list and current workspace intact.
    auto workspace = std::make_unique<Workspace>(path);
    Workspace* opened = workspace.get();
    m_workspaces.push_back(std::move(workspace));
    m_current = opened;
    return m_current;
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
            m_current = workspace;
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

    if (m_current == workspace) {
        m_current = nullptr;
    }
    m_workspaces.erase(found);
    if (!m_current && !m_workspaces.empty()) {
        m_current = m_workspaces.back().get();
    }
    return true;
}

void WorkspaceManager::closeAll() {
    m_current = nullptr;
    m_workspaces.clear();
}
