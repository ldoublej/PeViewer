#include "dependency_tree_editor_view.h"

#include <QAction>
#include <QApplication>
#include <QCoreApplication>
#include <QStyle>

#include <SARibbonCategory.h>
#include <SARibbonPanel.h>

#include "components/dependency_tree_widget.h"
#include "document.h"
#include "pe_editor.h"

DependencyTreeEditorView::DependencyTreeEditorView(PeEditor& editor) : m_editor(editor) {
    m_refresh = new QAction(nullptr);  // EditorView is not a QObject; the view owns the action.
    QObject::connect(m_refresh, &QAction::triggered, m_refresh, [this] {
        if (m_widget) {
            m_widget->setGraph(&m_editor.document().graph());
        }
    });
}

DependencyTreeEditorView::~DependencyTreeEditorView() {
    delete m_widget.data();
    delete m_refresh;
}

QString DependencyTreeEditorView::id() const { return QStringLiteral("dependency-tree"); }

QString DependencyTreeEditorView::title() const {
    return QCoreApplication::translate("DependencyTreeEditorView", "Dependency");
}

QWidget* DependencyTreeEditorView::contentWidget() {
    if (!m_widget) {
        m_widget = new DependencyTreeWidget();
        m_widget->setGraph(&m_editor.document().graph());
    }
    return m_widget;
}

void DependencyTreeEditorView::populateRibbon(SARibbonCategory* page) {
    m_panel = page->addPanel(QCoreApplication::translate("DependencyTreeEditorView", "View"));
    m_panel->setObjectName(QStringLiteral("dependencyPanel"));
    m_refresh->setIcon(QApplication::style()->standardIcon(QStyle::SP_BrowserReload));
    m_refresh->setText(QCoreApplication::translate("DependencyTreeEditorView", "Refresh"));
    m_panel->addSmallAction(m_refresh);
}

void DependencyTreeEditorView::retranslateUi() {
    m_refresh->setText(QCoreApplication::translate("DependencyTreeEditorView", "Refresh"));
    if (m_panel) {
        m_panel->setPanelName(QCoreApplication::translate("DependencyTreeEditorView", "View"));
    }
}
