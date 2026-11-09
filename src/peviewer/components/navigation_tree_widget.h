#pragma once

#include <QTreeWidget>

// A single-column navigation tree. Callers provide QTreeWidgetItems and use
// their standard text, icon, data and parent/child APIs to define the content.
// Item ownership and selection signals follow QTreeWidget's normal behavior.
class NavigationTreeWidget : public QTreeWidget {
    Q_OBJECT

public:
    explicit NavigationTreeWidget(QWidget* parent = nullptr);

protected:
    void drawBranches(QPainter* painter, const QRect& rect,
                      const QModelIndex& index) const override;
};
