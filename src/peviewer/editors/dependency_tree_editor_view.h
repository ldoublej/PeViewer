#pragma once

#include <QPointer>
#include <editor_view.h>

class DependencyTreeWidget;
class PeEditor;
class QAction;
class SARibbonPanel;

// 依赖树视图：一个 Ribbon 页签（刷新）+ 一个中央依赖树页面。
class DependencyTreeEditorView : public EditorView {
public:
    explicit DependencyTreeEditorView(PeEditor& editor);
    ~DependencyTreeEditorView() override;

    QString id() const override;
    QString title() const override;
    QWidget* contentWidget() override;
    void populateRibbon(SARibbonCategory* page) override;
    void retranslateUi() override;

private:
    PeEditor& m_editor;
    QPointer<DependencyTreeWidget> m_widget;
    QPointer<SARibbonPanel> m_panel;
    QAction* m_refresh = nullptr;
};
