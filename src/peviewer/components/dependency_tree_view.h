#pragma once

#include <QTreeView>

// QTreeView that connects parent and child rows with dashed branch lines.
// Expansion arrows are still drawn by the base class, on top of the lines.
class DependencyTreeView : public QTreeView {
    Q_OBJECT

public:
    explicit DependencyTreeView(QWidget* parent = nullptr);

protected:
    void drawBranches(QPainter* painter, const QRect& rect,
                      const QModelIndex& index) const override;
};
