#pragma once

#include <QObject>
#include <QString>
#include <QtGlobal>
#include <cstddef>
#include <memory>
#include <vector>

class Document;

// Singleton that owns every open Document and tracks the active one.
class DocumentManager : public QObject {
    Q_OBJECT

public:
    enum class OpenMode { OpenOnly, OpenAndActivate };
    Q_ENUM(OpenMode)

    static DocumentManager* instance();

    Q_DISABLE_COPY_MOVE(DocumentManager)

    // Constructs a new document and appends it; activates it if requested.
    // Throws on load/parse/build failure; the existing list is left untouched.
    // If path is already open, reuses it and applies the requested mode.
    Document* open(const QString& path, OpenMode mode = OpenMode::OpenAndActivate);

    Document* find(const QString& path) const;
    bool activate(Document* document);

    // Removes the document. If it was current, the most recently opened
    // remaining document becomes current.
    bool close(Document* document);
    void closeAll();

    Document* current() const { return m_current; }
    std::size_t size() const { return m_documents.size(); }
    bool empty() const { return m_documents.empty(); }
    const std::vector<std::unique_ptr<Document>>& documents() const { return m_documents; }

signals:
    void documentOpened(Document* document);
    // Removed from the list, but still alive during synchronous delivery.
    void documentClosed(Document* document);
    // Emitted only when current() changes; nullptr means no active document.
    void documentActivated(Document* document);

private:
    DocumentManager();
    ~DocumentManager() override;

    std::vector<std::unique_ptr<Document>> m_documents;
    Document* m_current = nullptr;
};
