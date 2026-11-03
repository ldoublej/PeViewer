#include "dependency_tree_view.h"

#include <QModelIndex>
#include <QPainter>
#include <QVector>

namespace {
bool hasSiblingAfter(const QModelIndex& index) {
    return index.isValid() && index.model() &&
           index.row() + 1 < index.model()->rowCount(index.parent());
}

int depthOf(const QModelIndex& index) {
    int depth = 0;
    for (QModelIndex parent = index.parent(); parent.isValid(); parent = parent.parent()) {
        ++depth;
    }
    return depth;
}
}  // namespace

DependencyTreeView::DependencyTreeView(QWidget* parent) : QTreeView(parent) {}

void DependencyTreeView::drawBranches(QPainter* painter, const QRect& rect,
                                      const QModelIndex& index) const {
    if (!index.isValid() || !index.model() || indentation() <= 0) {
        QTreeView::drawBranches(painter, rect, index);
        return;
    }

    // Nodes from the outermost ancestor down to the item, so each level knows
    // whether its line has to continue below this row.
    QVector<QModelIndex> path;
    const int level = depthOf(index);
    path.reserve(level + 1);
    for (QModelIndex node = index; node.isValid(); node = node.parent()) {
        path.prepend(node);
    }

    const int indent = indentation();
    const qreal halfIndent = indent / 2.0;
    const qreal centerY = rect.top() + rect.height() / 2.0;
    const qreal left = rect.left();
    const qreal right = rect.right();
    const qreal top = rect.top();
    const qreal bottom = rect.bottom();

    QColor lineColor = palette().color(QPalette::Text);
    lineColor.setAlpha(90);
    QPen pen(lineColor, 1, Qt::CustomDashLine);
    pen.setDashPattern({2, 3});

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, false);
    painter->setPen(pen);

    // Ancestor columns: keep the vertical line going while a later sibling
    // exists, and always bring the parent's line down to this row's center.
    for (int i = 0; i < level; ++i) {
        const bool parentLevel = i == level - 1;
        const bool continued = hasSiblingAfter(path.at(i));
        if (!parentLevel && !continued) {
            continue;
        }
        const qreal x = left + i * indent + halfIndent;
        const qreal lineBottom = continued ? bottom : centerY;
        painter->drawLine(QPointF(x, top), QPointF(x, lineBottom));
    }

    // Horizontal connector from the parent column to the item.
    if (level > 0) {
        const qreal x = left + (level - 1) * indent + halfIndent;
        painter->drawLine(QPointF(x, centerY), QPointF(right, centerY));
    }

    // The item's own column only continues downward while its children show.
    if (index.model()->hasChildren(index) && isExpanded(index)) {
        const qreal x = left + level * indent + halfIndent;
        painter->drawLine(QPointF(x, centerY), QPointF(x, bottom));
    }

    painter->restore();

    // Draw the expand/collapse arrows on top of the connector lines.
    QTreeView::drawBranches(painter, rect, index);
}
