#pragma once

#include <QObject>
#include <QString>
#include <QtGlobal>
#include <cstddef>
#include <functional>
#include <memory>
#include <vector>

class Document;
class Editor;

// 单例：持有全部已打开文档和当前文档。文档持有其 Editor，因此这里也是
// "当前编辑器" 的唯一来源。
class DocumentManager : public QObject {
    Q_OBJECT

public:
    enum class OpenMode { OpenOnly, OpenAndActivate };
    Q_ENUM(OpenMode)

    // 由应用在创建窗口之前安装，负责 PeEditor 之类的具体编辑器。
    using EditorFactory = std::function<std::unique_ptr<Editor>(Document&)>;

    static DocumentManager* instance();

    Q_DISABLE_COPY_MOVE(DocumentManager)

    // 不安装时打开的文档没有 Editor（仅测试或纯数据用法）。
    void setEditorFactory(EditorFactory factory);

    // Constructs a new document and appends it; activates it if requested.
    // Throws on load/parse/build failure; the existing list is left untouched.
    // If path is already open, reuses it and applies the requested mode.
    Document* open(const QString& path, OpenMode mode = OpenMode::OpenAndActivate);

    Document* find(const QString& path) const;
    bool activate(Document* document);

    // Removes the document. If it was current, the most recently opened
    // remaining document becomes current. Returns false when the document's
    // editor refuses to close (canClose() == false).
    bool close(Document* document);
    void closeAll();

    Document* current() const { return m_current; }
    // Convenience: current() ? current()->editor() : nullptr.
    Editor* currentEditor() const;
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

    EditorFactory m_editorFactory;
    std::vector<std::unique_ptr<Document>> m_documents;
    Document* m_current = nullptr;
};
