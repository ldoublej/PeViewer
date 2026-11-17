#include <editor.h>
#include <editor_view.h>
#include <editor_view_registry.h>

#include <QString>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

class FakeView : public EditorView {
public:
    FakeView(QString id, QString title) : m_id(std::move(id)), m_title(std::move(title)) {}

    QString id() const override { return m_id; }
    QString title() const override { return m_title; }
    QWidget* contentWidget() override { return nullptr; }
    void populateRibbon(SARibbonCategory*) override {}

private:
    QString m_id;
    QString m_title;
};

class FakeEditor : public Editor {
public:
    explicit FakeEditor(const QString& path) : Editor(path) {}
};

bool isFakeEditor(const Editor& editor) {
    return dynamic_cast<const FakeEditor*>(&editor) != nullptr;
}

EditorViewDescriptor makeDescriptor(const QString& id, int order,
                                    std::function<bool(const Editor&)> supports = {}) {
    return {id, order, std::move(supports),
            [](Editor&) { return std::make_unique<FakeView>(QStringLiteral("unused"), QString()); }};
}

void registryRejectsInvalidDescriptors() {
    auto* registry = EditorViewRegistry::instance();
    registry->clear();

    require(!registry->add({}), "Empty descriptor was accepted");
    require(!registry->add({QStringLiteral("view"), 100, {}, {}}), "Descriptor without factory was accepted");
    require(registry->add(makeDescriptor(QStringLiteral("view"), 100)), "Valid descriptor was rejected");
    require(!registry->add(makeDescriptor(QStringLiteral("view"), 200)), "Duplicate id was accepted");
    require(registry->contains(QStringLiteral("view")), "Registered id is missing");
    require(!registry->remove(QStringLiteral("missing")), "Unknown id was removed");
    require(registry->remove(QStringLiteral("view")), "Registered id was not removed");
    require(!registry->contains(QStringLiteral("view")), "Removed id is still present");
    registry->clear();
}

void registryFiltersAndSorts() {
    auto* registry = EditorViewRegistry::instance();
    registry->clear();

    require(registry->add(makeDescriptor(QStringLiteral("late"), 200)), "Add failed");
    require(registry->add(makeDescriptor(QStringLiteral("alpha"), 100, isFakeEditor)), "Add failed");
    require(registry->add(makeDescriptor(QStringLiteral("beta"), 100, isFakeEditor)), "Add failed");
    require(registry->add(makeDescriptor(QStringLiteral("any"), 150)), "Add failed");
    require(registry->add(makeDescriptor(QStringLiteral("hidden"), 0,
                                         [](const Editor&) { return false; })),
            "Add failed");

    FakeEditor editor(QStringLiteral("C:/fixtures/app.exe"));
    const std::vector<EditorViewDescriptor> views = registry->viewsFor(editor);
    require(views.size() == 4, "Unexpected number of matching views");
    require(views[0].id == QStringLiteral("alpha") && views[1].id == QStringLiteral("beta") &&
                views[2].id == QStringLiteral("any") && views[3].id == QStringLiteral("late"),
            "Views are not sorted by (order, id)");
    registry->clear();
}

void attachBuildsViewsForMatchingEditor() {
    auto* registry = EditorViewRegistry::instance();
    registry->clear();

    // The factory captures the editor reference it receives, so the view can
    // reach its editor through the constructor (the association point).
    Editor* createdFor = nullptr;
    require(registry->add({QStringLiteral("first"), 10, isFakeEditor,
                           [&createdFor](Editor& editor) {
                               createdFor = &editor;
                               return std::make_unique<FakeView>(QStringLiteral("first"), QStringLiteral("First"));
                           }}),
            "Add failed");
    require(registry->add({QStringLiteral("second"), 20, {},
                           [](Editor&) {
                               return std::make_unique<FakeView>(QStringLiteral("second"),
                                                                 QStringLiteral("Second"));
                           }}),
            "Add failed");
    require(registry->add({QStringLiteral("other"), 5,
                           [](const Editor&) { return false; },
                           [](Editor&) {
                               return std::make_unique<FakeView>(QStringLiteral("other"),
                                                                 QStringLiteral("Other"));
                           }}),
            "Add failed");

    FakeEditor editor(QStringLiteral("C:/fixtures/app.exe"));
    editor.attachRegisteredViews();
    require(editor.views().size() == 2, "Editor did not attach the matching views");
    require(editor.views()[0]->id() == QStringLiteral("first") &&
                editor.views()[1]->id() == QStringLiteral("second"),
            "Attached views are out of order");
    require(createdFor == &editor, "View factory did not receive the owning editor");
    require(editor.displayName() == QStringLiteral("app.exe"), "Default display name is not the file name");
    require(editor.canClose(), "Editor should be closable by default");
    registry->clear();
}

void attachWithoutDescriptorsKeepsEditorEmpty() {
    EditorViewRegistry::instance()->clear();
    FakeEditor editor(QStringLiteral("C:/fixtures/app.exe"));
    editor.attachRegisteredViews();
    require(editor.views().empty(), "Editor got views without any registration");
}

void run() {
    registryRejectsInvalidDescriptors();
    registryFiltersAndSorts();
    attachBuildsViewsForMatchingEditor();
    attachWithoutDescriptorsKeepsEditorEmpty();
}
}  // namespace

int main() {
    try {
        run();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
