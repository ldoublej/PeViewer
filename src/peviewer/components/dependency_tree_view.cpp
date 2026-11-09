#include "dependency_tree_view.h"

#include <QModelIndex>
#include <QPainter>
#include <QStyle>
#include <QStyleOption>
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

    // 仅绘制展开控件，避免 Qt 再叠加一套原生连接线。
    if (hasChildren) {
        QStyleOption option;
        option.initFrom(this);
        option.rect = QRect(qRound(columnX(columns - 1)) - 4, qRound(centerY) - 4, 9, 9);
        option.state &= ~(QStyle::State_Item | QStyle::State_Sibling |
                          QStyle::State_Selected | QStyle::State_MouseOver |
                          QStyle::State_HasFocus);
        option.state |= QStyle::State_Children;
        if (isExpanded(index)) {
            option.state |= QStyle::State_Open;
        }
        style()->drawPrimitive(QStyle::PE_IndicatorBranch, &option, painter, this);
    }
    painter->restore();
}
