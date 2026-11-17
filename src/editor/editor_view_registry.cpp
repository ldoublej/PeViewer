#include "editor_view_registry.h"

#include <algorithm>
#include <utility>

#include "editor.h"

EditorViewRegistry* EditorViewRegistry::instance() {
    static EditorViewRegistry registry;
    return &registry;
}

bool EditorViewRegistry::add(EditorViewDescriptor descriptor) {
    if (descriptor.id.isEmpty() || !descriptor.create || contains(descriptor.id)) {
        return false;
    }
    m_descriptors.push_back(std::move(descriptor));
    return true;
}

bool EditorViewRegistry::remove(const QString& id) {
    const auto found = std::find_if(m_descriptors.begin(), m_descriptors.end(),
                                    [&id](const EditorViewDescriptor& descriptor) {
                                        return descriptor.id == id;
                                    });
    if (found == m_descriptors.end()) {
        return false;
    }
    m_descriptors.erase(found);
    return true;
}

bool EditorViewRegistry::contains(const QString& id) const {
    return std::any_of(m_descriptors.begin(), m_descriptors.end(),
                       [&id](const EditorViewDescriptor& descriptor) { return descriptor.id == id; });
}

void EditorViewRegistry::clear() { m_descriptors.clear(); }

std::vector<EditorViewDescriptor> EditorViewRegistry::viewsFor(const Editor& editor) const {
    std::vector<EditorViewDescriptor> matching;
    for (const EditorViewDescriptor& descriptor : m_descriptors) {
        if (!descriptor.supports || descriptor.supports(editor)) {
            matching.push_back(descriptor);
        }
    }
    std::stable_sort(matching.begin(), matching.end(),
                     [](const EditorViewDescriptor& left, const EditorViewDescriptor& right) {
                         if (left.order != right.order) {
                             return left.order < right.order;
                         }
                         return left.id < right.id;
                     });
    return matching;
}
