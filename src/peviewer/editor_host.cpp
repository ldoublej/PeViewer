#include "editor_host.h"

#include <QStackedWidget>

#include <SARibbonBar.h>
#include <SARibbonCategory.h>

#include "document.h"
#include "document_manager.h"
#include "editor.h"
#include "editor_view.h"

EditorHost::EditorHost(SARibbonBar* ribbon, QStackedWidget* stack, QWidget* homePage, QObject* parent)
    : QObject(parent), m_ribbon(ribbon), m_stack(stack), m_homePage(homePage) {
    auto* manager = DocumentManager::instance();
    connect(manager, &DocumentManager::documentActivated, this, &EditorHost::onDocumentActivated);
    connect(manager, &DocumentManager::documentClosed, this, &EditorHost::onDocumentClosed);
    connect(m_ribbon, &SARibbonBar::currentRibbonTabChanged, this, &EditorHost::onRibbonTabChanged);
    onDocumentActivated(manager->current());
}

void EditorHost::detachViews() {
    for (const QPointer<QWidget>& widget : m_stackWidgets) {
        if (!widget) {
            continue;
        }
        m_stack->removeWidget(widget);
        widget->setParent(nullptr);
    }
    m_stackWidgets.clear();
}

void EditorHost::retranslateUi() {
    for (auto it = m_pageViews.cbegin(); it != m_pageViews.cend(); ++it) {
        it.value()->retranslateUi();
        it.key()->setCategoryName(it.value()->title());
    }
}

void EditorHost::onDocumentActivated(Document* document) {
    m_rebuilding = true;
    clearPages();
    showHome();
    if (document && document->editor()) {
        buildPages(document);
    }
    m_rebuilding = false;

    EditorView* target = targetView(document);
    if (!target) {
        showHome();
        return;
    }
    selectViewTab(target);
    showView(target);
}

void EditorHost::onDocumentClosed(Document* document) {
    m_lastView.remove(document);
    // Delete the pages that belong to the closing document; its views die with it.
    if (document && document->editor()) {
        for (const std::unique_ptr<EditorView>& view : document->editor()->views()) {
            QWidget* widget = m_viewWidgets.take(view.get());
            if (!widget) {
                continue;
            }
            m_stack->removeWidget(widget);
            delete widget;
        }
    }
    if (!DocumentManager::instance()->current()) {
        showHome();
    }
}

void EditorHost::onRibbonTabChanged(int index) {
    if (m_rebuilding) {
        return;
    }
    SARibbonCategory* page = m_ribbon->categoryByIndex(index);
    if (!page) {
        return;
    }
    if (EditorView* view = m_pageViews.value(page, nullptr)) {
        showView(view);
    } else {
        showHome();
    }
}

void EditorHost::clearPages() {
    const QList<SARibbonCategory*> pages = m_pageViews.keys();
    for (SARibbonCategory* page : pages) {
        m_ribbon->removeCategory(page);
        delete page;
    }
    m_pageViews.clear();
}

void EditorHost::buildPages(Document* document) {
    for (const std::unique_ptr<EditorView>& view : document->editor()->views()) {
        auto* page = new SARibbonCategory(m_ribbon);
        page->setObjectName(QStringLiteral("editorView/") + view->id());
        page->setCategoryName(view->title());
        view->populateRibbon(page);
        m_ribbon->addCategoryPage(page);
        m_pageViews.insert(page, view.get());

        QWidget* content = view->contentWidget();
        if (content && m_stack->indexOf(content) < 0) {
            m_stack->addWidget(content);
            m_stackWidgets.append(content);
        }
        if (content) {
            m_viewWidgets.insert(view.get(), content);
        }
    }
}

void EditorHost::showView(EditorView* view) {
    if (!view || view == m_currentView) {
        return;
    }
    if (m_currentView) {
        m_currentView->deactivated();
    }
    m_currentView = view;
    if (Document* document = DocumentManager::instance()->current()) {
        m_lastView.insert(document, view);
    }
    QWidget* content = view->contentWidget();
    if (!content) {
        return;
    }
    if (m_stack->indexOf(content) < 0) {
        m_stack->addWidget(content);
        m_stackWidgets.append(content);
    }
    m_viewWidgets.insert(view, content);
    m_stack->setCurrentWidget(content);
    view->activated();
}

void EditorHost::showHome() {
    if (m_currentView) {
        m_currentView->deactivated();
        m_currentView = nullptr;
    }
    m_stack->setCurrentWidget(m_homePage);
}

void EditorHost::selectViewTab(EditorView* view) {
    for (auto it = m_pageViews.cbegin(); it != m_pageViews.cend(); ++it) {
        if (it.value() != view) {
            continue;
        }
        const int index = m_ribbon->categoryIndex(it.key());
        if (index >= 0) {
            m_ribbon->setCurrentIndex(index);
        }
        return;
    }
}

EditorView* EditorHost::targetView(Document* document) const {
    if (!document || !document->editor()) {
        return nullptr;
    }
    if (EditorView* remembered = m_lastView.value(document, nullptr)) {
        return remembered;
    }
    const std::vector<std::unique_ptr<EditorView>>& views = document->editor()->views();
    return views.empty() ? nullptr : views.front().get();
}
