#include "dependency_tree_view.h"

#include <QModelIndex>
#include <QPainter>
#include <QPolygonF>
#include <QVector>

namespace {
bool hasSiblingAfter(const QModelIndex& index) {
    return index.isValid() && index.model() &&
           index.row() + 1 < index.model()->rowCount(index.parent());
}
}  // namespace

DependencyTreeView::DependencyTreeView(QWidget* parent) : QTreeView(parent) {}

void DependencyTreeView::drawBranches(QPainter* painter, const QRect& rect,
                                      const QModelIndex& index) const {
    if (!index.isValid() || !index.model() || indentation() <= 0) {
        QTreeView::drawBranches(painter, rect, index);
        return;
    }

    // 从最外层祖先到当前节点；可见列数由 Qt 提供的分支区域决定。
    QVector<QModelIndex> path;
    for (QModelIndex node = index; node.isValid(); node = node.parent()) {
        path.prepend(node);
    }

    const int indent = indentation();
    const int columns = qMin(path.size(), rect.width() / indent);
    if (columns == 0) {
        return;
    }
    const int firstNode = path.size() - columns;
    const qreal halfIndent = indent / 2.0;
    const qreal centerY = rect.top() + rect.height() / 2.0;
    const qreal top = rect.top();
    const qreal bottom = rect.bottom();
    const bool rtl = isRightToLeft();
    const bool hasChildren = index.model()->hasChildren(index);
    const auto columnX = [&](int column) {
        return rtl ? rect.right() - column * indent - halfIndent
                   : rect.left() + column * indent + halfIndent;
    };
    const qreal itemEdge = rtl ? rect.left() : rect.right();

    QColor lineColor = palette().color(QPalette::Text);
    lineColor.setAlpha(90);
    QPen pen(lineColor, 1, Qt::CustomDashLine);
    pen.setDashPattern({2, 3});

    painter->save();
    painter->setClipRect(rect, Qt::IntersectClip);
    painter->setRenderHint(QPainter::Antialiasing, false);
    painter->setPen(pen);

    // 看通向当前行的下一层节点是否还有同级节点，决定祖先竖线是否继续。
    for (int i = 0; i < columns - 1; ++i) {
        const bool parentLevel = i == columns - 2;
        const bool continued = hasSiblingAfter(path.at(firstNode + i + 1));
        if (!parentLevel && !continued) {
            continue;
        }
        const qreal x = columnX(i);
        const qreal lineBottom = continued ? bottom : centerY;
        painter->drawLine(QPointF(x, top), QPointF(x, lineBottom));
    }

    // 从父级所在列连接到当前条目；无根装饰时使用当前列。
    if (path.size() > 1) {
        const qreal x = columnX(qMax(0, columns - 2));
        painter->drawLine(QPointF(x, centerY), QPointF(itemEdge, centerY));
    }

    // 展开的节点向下连接其子节点。
    if (hasChildren && isExpanded(index)) {
        const qreal x = columnX(columns - 1);
        painter->drawLine(QPointF(x, centerY), QPointF(x, bottom));
    }

    // 展开/收起三角自绘：平台样式的 PE_IndicatorBranch 在 Ribbon 主题下不显示，
    // 直接画可以保证任何主题、任何 DPI 下都能看到。
    if (hasChildren) {
        const qreal centerX = columnX(columns - 1);
        QPolygonF arrow;
        if (isExpanded(index)) {
            arrow << QPointF(centerX - 4, centerY - 2.5) << QPointF(centerX + 4, centerY - 2.5)
                  << QPointF(centerX, centerY + 3.5);
        } else if (rtl) {
            arrow << QPointF(centerX + 2.5, centerY - 4) << QPointF(centerX - 3.5, centerY)
                  << QPointF(centerX + 2.5, centerY + 4);
        } else {
            arrow << QPointF(centerX - 2.5, centerY - 4) << QPointF(centerX + 3.5, centerY)
                  << QPointF(centerX - 2.5, centerY + 4);
        }
        QColor arrowColor = palette().color(QPalette::Text);
        arrowColor.setAlpha(170);
        painter->setRenderHint(QPainter::Antialiasing, true);
        painter->setPen(Qt::NoPen);
        painter->setBrush(arrowColor);
        painter->drawPolygon(arrow);
    }
    painter->restore();
}
