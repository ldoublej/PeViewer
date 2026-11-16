#include "document_manager.h"

#include <QFileInfo>
#include <algorithm>
#include <utility>

#include "document.h"
#include "editor.h"

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

DocumentManager* DocumentManager::instance() {
    static DocumentManager manager;
    return &manager;
}

DocumentManager::DocumentManager() = default;
DocumentManager::~DocumentManager() = default;

void DocumentManager::setEditorFactory(EditorFactory factory) {
    m_editorFactory = std::move(factory);
}

Editor* DocumentManager::currentEditor() const {
    return m_current ? m_current->editor() : nullptr;
}

Document* DocumentManager::open(const QString& path, OpenMode mode) {
    if (Document* document = find(path)) {
        if (mode == OpenMode::OpenAndActivate) {
            activate(document);
        }
        return document;
    }

    // Build first so a failure leaves the list and current document intact.
    auto document = std::make_unique<Document>(path, FileLocatorOptions::systemDefaults(),
                                               m_editorFactory);
    Document* opened = document.get();
    m_documents.push_back(std::move(document));
    emit documentOpened(opened);
    if (mode == OpenMode::OpenAndActivate) {
        activate(opened);
    }
    return opened;
}

Document* DocumentManager::find(const QString& path) const {
    const QString wanted = canonicalPath(path);
    for (const auto& document : m_documents) {
        if (samePath(canonicalPath(document->path()), wanted)) {
            return document.get();
        }
    }
    return nullptr;
}

bool DocumentManager::activate(Document* document) {
    for (const auto& entry : m_documents) {
        if (entry.get() == document) {
            if (m_current != document) {
                m_current = document;
                emit documentActivated(document);
            }
            return true;
        }
    }
    return false;
}

bool DocumentManager::close(Document* document) {
    if (!document) {
        return false;
    }
    const auto found = std::find_if(
        m_documents.begin(), m_documents.end(),
        [document](const std::unique_ptr<Document>& entry) { return entry.get() == document; });
    if (found == m_documents.end()) {
        return false;
    }
    // Only ask after the pointer is known to be alive: callers may pass a stale pointer.
    if (Editor* editor = (*found)->editor(); editor && !editor->canClose()) {
        return false;
    }

    const bool wasCurrent = m_current == document;
    auto closing = std::move(*found);
    m_documents.erase(found);
    if (wasCurrent) {
        m_current = m_documents.empty() ? nullptr : m_documents.back().get();
        emit documentActivated(m_current);
    }
    emit documentClosed(closing.get());
    return true;
}

void DocumentManager::closeAll() {
    auto closing = std::move(m_documents);
    m_documents.clear();
    const bool hadCurrent = m_current != nullptr;
    m_current = nullptr;
    if (hadCurrent) {
        emit documentActivated(nullptr);
    }
    for (const auto& document : closing) {
        emit documentClosed(document.get());
    }
}
