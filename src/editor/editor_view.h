#pragma once

#include <QString>

class QWidget;
class SARibbonCategory;

// 一个视图 = 一个 Ribbon 页签 + 对应的中央页面。
// 生命周期与所属 Editor 相同；Ribbon 页面由 EditorHost 在每次激活文档时重建。
class EditorView {
public:
    EditorView() = default;
    virtual ~EditorView() = default;

    EditorView(const EditorView&) = delete;
    EditorView& operator=(const EditorView&) = delete;

    // Stable identifier, unique inside one Editor, e.g. "dependency-tree".
    virtual QString id() const = 0;
    // Localized Ribbon tab title; re-read on every language change.
    virtual QString title() const = 0;
    // Central widget. Created on demand and reused while it lives; the host owns
    // the QObject parent by putting it into its page stack.
    virtual QWidget* contentWidget() = 0;

    // Fills the empty Ribbon page created by the host. Called again on every
    // activation, so it may only add QActions owned by the view.
    virtual void populateRibbon(SARibbonCategory* page) = 0;

    // Refresh translated texts of actions and widgets.
    virtual void retranslateUi() {}
    // Called when this view becomes / stops being the displayed page.
    virtual void activated() {}
    virtual void deactivated() {}
};
