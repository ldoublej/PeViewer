#pragma once

#include <QString>
#include <functional>
#include <memory>
#include <vector>

class Editor;
class EditorView;

// 一个视图类型的注册信息。启动时注册一次，Document 创建 Editor 后逐条实例化。
struct EditorViewDescriptor {
    QString id;                                            // 唯一，如 "dependency-tree"
    int order = 100;                                       // 页签顺序，升序
    std::function<bool(const Editor&)> supports;           // 空 = 对所有 Editor 适用
    std::function<std::unique_ptr<EditorView>(Editor&)> create;
};

class EditorViewRegistry {
public:
    static EditorViewRegistry* instance();

    EditorViewRegistry(const EditorViewRegistry&) = delete;
    EditorViewRegistry& operator=(const EditorViewRegistry&) = delete;

    // Returns false for an empty id, a missing factory or a duplicate id.
    bool add(EditorViewDescriptor descriptor);
    bool remove(const QString& id);
    bool contains(const QString& id) const;
    // Test-only: drop every descriptor.
    void clear();

    // Descriptors whose supports() passes, sorted by (order, id).
    std::vector<EditorViewDescriptor> viewsFor(const Editor& editor) const;

private:
    EditorViewRegistry() = default;

    std::vector<EditorViewDescriptor> m_descriptors;
};
