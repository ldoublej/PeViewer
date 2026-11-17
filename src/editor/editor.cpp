#include "editor.h"

#include <QFileInfo>
#include <utility>

#include "editor_view.h"
#include "editor_view_registry.h"

Editor::Editor(QString path) : m_path(std::move(path)) {}

Editor::~Editor() = default;

QString Editor::displayName() const { return QFileInfo(m_path).fileName(); }

void Editor::attachRegisteredViews() {
    const std::vector<EditorViewDescriptor> descriptors =
        EditorViewRegistry::instance()->viewsFor(*this);
    for (const EditorViewDescriptor& descriptor : descriptors) {
        std::unique_ptr<EditorView> view = descriptor.create(*this);
        if (view) {
            addView(std::move(view));
        }
    }
}

void Editor::addView(std::unique_ptr<EditorView> view) { m_views.push_back(std::move(view)); }
