#pragma once

#include <QString>
#include <memory>
#include <vector>

class EditorView;

// 一个已打开文档在 UI 侧的表示：只持有视图，不持有数据。
// 由 Document 通过注入的 EditorFactory 创建，随后调用 attachRegisteredViews()。
class Editor {
public:
    virtual ~Editor();

    Editor(const Editor&) = delete;
    Editor& operator=(const Editor&) = delete;

    const QString& path() const { return m_path; }
    // Display name in the document selector, defaults to the file name.
    virtual QString displayName() const;
    // Asked before closing; false cancels the close (unsaved changes).
    virtual bool canClose() const { return true; }
    virtual void retranslateUi() {}

    const std::vector<std::unique_ptr<EditorView>>& views() const { return m_views; }

    // Instantiates every registered view that supports this editor and attaches it.
    // Called once by Document after the concrete editor is fully constructed.
    void attachRegisteredViews();

protected:
    explicit Editor(QString path);

private:
    void addView(std::unique_ptr<EditorView> view);

    QString m_path;
    std::vector<std::unique_ptr<EditorView>> m_views;
};
