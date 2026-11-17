#include "headers_editor_view.h"

#include <SARibbonCategory.h>
#include <SARibbonPanel.h>

#include <QAction>
#include <QApplication>
#include <QCoreApplication>
#include <QStyle>

#include "components/pe_headers_widget.h"
#include "document.h"
#include "pe_editor.h"
#include "pe_file.h"

HeadersEditorView::HeadersEditorView(PeEditor& editor) : m_editor(editor) {
    m_refresh = new QAction(nullptr);  // EditorView is not a QObject; the view owns the action.
    QObject::connect(m_refresh, &QAction::triggered, m_refresh, [this] { updateTables(); });
}

HeadersEditorView::~HeadersEditorView() {
    delete m_widget.data();
    delete m_refresh;
}

QString HeadersEditorView::id() const { return QStringLiteral("headers"); }

QString HeadersEditorView::title() const {
    return QCoreApplication::translate("HeadersEditorView", "Headers");
}

QWidget* HeadersEditorView::contentWidget() {
    if (!m_widget) {
        m_widget = new PeHeadersWidget();
        updateTables();
    }
    return m_widget;
}

void HeadersEditorView::populateRibbon(SARibbonCategory* page) {
    m_panel = page->addPanel(QCoreApplication::translate("HeadersEditorView", "View"));
    m_panel->setObjectName(QStringLiteral("headersPanel"));
    m_refresh->setIcon(QApplication::style()->standardIcon(QStyle::SP_BrowserReload));
    m_refresh->setText(QCoreApplication::translate("HeadersEditorView", "Refresh"));
    m_panel->addSmallAction(m_refresh);
}

void HeadersEditorView::retranslateUi() {
    m_refresh->setText(QCoreApplication::translate("HeadersEditorView", "Refresh"));
    if (m_panel) {
        m_panel->setPanelName(QCoreApplication::translate("HeadersEditorView", "View"));
    }
}

void HeadersEditorView::activated() { updateTables(); }

void HeadersEditorView::updateTables() {
    if (!m_widget) {
        return;
    }
    const PeFile& peFile = m_editor.document().peFile();
    m_widget->setPeFile(peFile.isParsed() ? &peFile : nullptr);
}
