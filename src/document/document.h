#pragma once

#include <QString>
#include <functional>
#include <memory>

#include "file_module_locator.h"
#include "pe_file_store.h"

class DependencyGraph;
class Editor;
class PeFile;
class PeFileDataSource;

// 一次打开的会话：持有根文件、数据源、依赖文件、依赖图，以及该文档的 Editor。
// 构造时完成打开、解析、依赖构建和编辑器创建；任何一步失败都抛异常，不留半成品。
class Document {
public:
    using EditorFactory = std::function<std::unique_ptr<Editor>(Document&)>;

    // editorFactory 为空时只建数据（测试 / 非 UI 用法），此时 editor() 为 nullptr。
    explicit Document(const QString& path,
                      FileLocatorOptions options = FileLocatorOptions::systemDefaults(),
                      EditorFactory editorFactory = {});
    ~Document();

    Q_DISABLE_COPY_MOVE(Document)

    // Absolute path of the opened file.
    const QString& path() const { return m_path; }
    const PeFile& peFile() const;
    const DependencyGraph& graph() const;
    const PeFileStore& files() const { return m_files; }
    // nullptr only when the document was created without an editor factory.
    Editor* editor() const { return m_editor.get(); }

private:
    QString m_path;
    // Destruction order matters: the editor (views) is destroyed before the graph,
    // the graph before the store and the root file, which borrow the data source.
    std::unique_ptr<PeFileDataSource> m_dataSource;
    std::unique_ptr<PeFile> m_peFile;
    PeFileStore m_files;
    std::unique_ptr<DependencyGraph> m_graph;
    std::unique_ptr<Editor> m_editor;
};
