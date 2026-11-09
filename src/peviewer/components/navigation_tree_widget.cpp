#include "navigation_tree_widget.h"

#include <QHeaderView>
#include <QPainter>
#include <QStyledItemDelegate>
#include <QStyleOption>

namespace {
class NavigationItemDelegate : public QStyledItemDelegate {
public:
    explicit NavigationItemDelegate(QObject* parent) : QStyledItemDelegate(parent) {}

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override {
        QStyleOptionViewItem opt(option);
        initStyleOption(&opt, index);
        const bool selected = opt.state & QStyle::State_Selected;
        const bool hovered = opt.state & QStyle::State_MouseOver;
        opt.state &= ~(QStyle::State_Selected | QStyle::State_MouseOver);
        QStyledItemDelegate::paint(painter, opt, index);
        // Tint after native painting: Windows styles may fill the item panel
        // even when it is not selected. The translucent tint keeps text legible.
        if (selected || hovered) {
            QColor highlight = opt.palette.color(QPalette::Active, QPalette::Highlight);
            highlight.setAlpha(selected ? 40 : 20);
            painter->save();
            painter->setPen(Qt::NoPen);
            painter->setBrush(highlight);
            painter->drawRoundedRect(opt.rect.adjusted(2, 1, -4, -1), 2, 2);
            painter->restore();
        }
    }

    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        QSize size = QStyledItemDelegate::sizeHint(option, index);
        size.setHeight(qMax(30, size.height() + 10));
        return size;
    }
};
}  // namespace

NavigationTreeWidget::NavigationTreeWidget(QWidget* parent) : QTreeWidget(parent) {
    setColumnCount(1);
    setHeaderHidden(true);
    header()->setStretchLastSection(true);
    setFrameShape(QFrame::NoFrame);
    setSelectionMode(QAbstractItemView::SingleSelection);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setUniformRowHeights(true);
    setIconSize(QSize(16, 16));
    setIndentation(18);
    setMouseTracking(true);
    setItemDelegate(new NavigationItemDelegate(this));
    setStyleSheet(QStringLiteral("QTreeWidget { background-color: palette(window); }"));
}

void NavigationTreeWidget::drawBranches(QPainter* painter, const QRect& rect,
                                         const QModelIndex& index) const {
    // Only show expansion arrows, keeping the navigation free of connector
    // lines and the native saturated selection fill in the indentation area.
    if (!model()->hasChildren(index)) {
        return;
    }
    QStyleOption option;
    option.initFrom(this);
    option.rect = rect;
    option.rect.setWidth(indentation());
    if (layoutDirection() == Qt::LeftToRight) {
        option.rect.moveRight(rect.right());
    }
    option.state &= ~(QStyle::State_Selected | QStyle::State_MouseOver | QStyle::State_HasFocus);
    option.state |= QStyle::State_Children;
    if (isExpanded(index)) {
        option.state |= QStyle::State_Open;
    }
    style()->drawPrimitive(QStyle::PE_IndicatorBranch, &option, painter, this);
}
