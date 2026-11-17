#include "editor_registration.h"

#include <editor.h>
#include <editor_view.h>
#include <editor_view_registry.h>

#include <memory>

#include "document.h"
#include "document_manager.h"
#include "editors/dependency_tree_editor_view.h"
#include "editors/headers_editor_view.h"
#include "editors/pe_editor.h"

namespace {
bool isPeEditor(const Editor& editor) {
    return dynamic_cast<const PeEditor*>(&editor) != nullptr;
}
}  // namespace

void registerPeEditorViews() {
    DocumentManager::instance()->setEditorFactory([](Document& document) {
        return std::make_unique<PeEditor>(document);
    });
    EditorViewRegistry::instance()->add(
        {QStringLiteral("headers"), 10, isPeEditor, [](Editor& editor) {
             return std::make_unique<HeadersEditorView>(static_cast<PeEditor&>(editor));
         }});
    EditorViewRegistry::instance()->add(
        {QStringLiteral("dependency-tree"), 100, isPeEditor, [](Editor& editor) {
             return std::make_unique<DependencyTreeEditorView>(static_cast<PeEditor&>(editor));
         }});
}
