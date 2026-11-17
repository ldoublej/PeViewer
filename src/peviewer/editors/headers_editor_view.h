#pragma once

#include <editor_view.h>

#include <QPointer>

class PeEditor;
class PeHeadersWidget;
class QAction;
class SARibbonPanel;

// PE 头视图：一个 Ribbon 页签（刷新）+ 一个中央页面，页面内是三张头表。
class HeadersEditorView : public EditorView {
public:
    explicit HeadersEditorView(PeEditor& editor);
    ~HeadersEditorView() override;

    QString id() const override;
    QString title() const override;
    QWidget* contentWidget() override;
    void populateRibbon(SARibbonCategory* page) override;
    void retranslateUi() override;
    void activated() override;

private:
    void updateTables();

    PeEditor& m_editor;
    QPointer<PeHeadersWidget> m_widget;
    QPointer<SARibbonPanel> m_panel;
    QAction* m_refresh = nullptr;
};
