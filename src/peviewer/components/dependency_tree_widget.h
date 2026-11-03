#pragma once

#include <QWidget>

class DependencyGraph;
class DependencyTreeDelegate;
class DependencyTreeModel;
class QTreeView;

// Read-only tree view of a DependencyGraph. The graph is borrowed: keep it alive
// while it is set, and call clear() before destroying it.
class DependencyTreeWidget : public QWidget {
    Q_OBJECT

public:
    explicit DependencyTreeWidget(QWidget* parent = nullptr);
    ~DependencyTreeWidget() override;

    void setGraph(const DependencyGraph* graph);
    void clear();

protected:
    void changeEvent(QEvent* event) override;

private:
    DependencyTreeModel* m_model = nullptr;
    DependencyTreeDelegate* m_delegate = nullptr;
    QTreeView* m_view = nullptr;
};
