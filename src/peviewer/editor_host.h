#pragma once

#include <QHash>
#include <QObject>
#include <QPointer>
#include <QVector>

class Document;
class EditorView;
class QStackedWidget;
class QWidget;
class SARibbonBar;
class SARibbonCategory;

// 把当前文档的视图映射成 Ribbon 页签 + viewStack 页面。
class EditorHost : public QObject {
    Q_OBJECT

public:
    EditorHost(SARibbonBar* ribbon, QStackedWidget* stack, QWidget* homePage, QObject* parent = nullptr);

    EditorView* currentView() const { return m_currentView; }

    // Detaches the view pages from the stack so the window can be destroyed while
    // documents stay open (views own their widgets and rebuild them on demand).
    void detachViews();

public Q_SLOTS:
    void retranslateUi();

private Q_SLOTS:
    void onDocumentActivated(Document* document);
    void onDocumentClosed(Document* document);
    void onRibbonTabChanged(int index);

private:
    void clearPages();
    void buildPages(Document* document);
    void showView(EditorView* view);
    void showHome();
    void selectViewTab(EditorView* view);
    EditorView* targetView(Document* document) const;

    SARibbonBar* m_ribbon = nullptr;
    QStackedWidget* m_stack = nullptr;
    QWidget* m_homePage = nullptr;
    QHash<SARibbonCategory*, EditorView*> m_pageViews;   // 只含当前文档
    QHash<const Document*, EditorView*> m_lastView;      // 每个文档最后显示的视图
    QHash<EditorView*, QPointer<QWidget>> m_viewWidgets; // 视图 → 已创建的页面
    QVector<QPointer<QWidget>> m_stackWidgets;           // 本宿主放进 stack 的页面
    EditorView* m_currentView = nullptr;                 // nullptr 表示正停在主页
    bool m_rebuilding = false;
};
