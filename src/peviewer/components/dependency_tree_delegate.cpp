#include "dependency_tree_delegate.h"

#include <QApplication>
#include <QFontMetrics>
#include <QPainter>
#include <QPalette>
#include <QStyle>

#include "dependency_tree_model.h"

namespace {
using Status = DependencyTreeModel::Status;

// Keeps the pill compact so it fits the slimmer rows.
constexpr int kPillHorizontalPadding = 8;
constexpr int kPillVerticalPadding = 2;
constexpr int kCellHorizontalMargin = 6;
constexpr int kRowVerticalPadding = 3;

QFont pillFont(const QFont& base) {
    QFont font(base);
    font.setWeight(QFont::DemiBold);
    return font;
}

QColor statusColor(Status status, bool dark) {
    switch (status) {
        case Status::Root:
            return dark ? QColor(0x64, 0xb5, 0xf6) : QColor(0x15, 0x65, 0xc0);
        case Status::Resolved:
        case Status::ApiSet:
            return dark ? QColor(0x81, 0xc7, 0x84) : QColor(0x2e, 0x7d, 0x32);
        case Status::Cycle:
            return dark ? QColor(0xff, 0xb7, 0x4d) : QColor(0xe6, 0x51, 0x00);
        default:
            return dark ? QColor(0xef, 0x9a, 0x9a) : QColor(0xc6, 0x28, 0x28);
    }
}
}  // namespace

DependencyTreeDelegate::DependencyTreeDelegate(QObject* parent) : QStyledItemDelegate(parent) {}

void DependencyTreeDelegate::initStyleOption(QStyleOptionViewItem* option,
                                             const QModelIndex& index) const {
    QStyledItemDelegate::initStyleOption(option, index);
    // Selection already highlights the row; skip the dotted keyboard-focus frame.
    option->state &= ~QStyle::State_HasFocus;
}

void DependencyTreeDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option,
                                   const QModelIndex& index) const {
    if (index.column() != DependencyTreeModel::StatusColumn) {
        QStyledItemDelegate::paint(painter, option, index);
        return;
    }

    QStyleOptionViewItem opt(option);
    initStyleOption(&opt, index);
    const QString text = opt.text;
    opt.text.clear();

    const QWidget* widget = opt.widget;
    QStyle* style = widget ? widget->style() : QApplication::style();
    style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, widget);

    if (text.isEmpty()) {
        return;
    }

    const QFont font = pillFont(opt.font);
    const QFontMetrics metrics(font);
    const QRect area = opt.rect.adjusted(kCellHorizontalMargin, kRowVerticalPadding,
                                         -kCellHorizontalMargin, -kRowVerticalPadding);
    QString label = text;
    const int available = qMax(0, area.width() - 2 * kPillHorizontalPadding);
    if (metrics.horizontalAdvance(label) > available) {
        label = metrics.elidedText(label, Qt::ElideRight, available);
    }

    const int height = qMin(area.height(), metrics.height() + 2 * kPillVerticalPadding);
    const int width =
        qMin(area.width(), metrics.horizontalAdvance(label) + 2 * kPillHorizontalPadding);
    if (width <= 0 || height <= 0) {
        return;
    }
    const QRect pill(area.left(), area.top() + (area.height() - height) / 2, width, height);

    const auto status =
        static_cast<Status>(index.data(DependencyTreeModel::StatusRole).toInt());
    const bool dark = opt.palette.color(QPalette::Base).lightness() < 128;
    const QColor color = statusColor(status, dark);
    QColor fill = color;
    fill.setAlpha(dark ? 60 : 32);

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setFont(font);
    painter->setPen(QPen(color, 1));
    painter->setBrush(fill);
    const qreal radius = height / 2.0;
    painter->drawRoundedRect(QRectF(pill).adjusted(0.5, 0.5, -0.5, -0.5), radius, radius);
    painter->setPen(color);
    painter->drawText(pill, Qt::AlignCenter, label);
    painter->restore();
}

QSize DependencyTreeDelegate::sizeHint(const QStyleOptionViewItem& option,
                                       const QModelIndex& index) const {
    QSize size = QStyledItemDelegate::sizeHint(option, index);
    // Slim, uniform rows: just enough padding to keep text off the row edges.
    size.setHeight(qMax(18, option.fontMetrics.height() + 2 * kRowVerticalPadding));

    if (index.column() == DependencyTreeModel::StatusColumn) {
        QStyleOptionViewItem opt(option);
        initStyleOption(&opt, index);
        const QFontMetrics metrics(pillFont(opt.font));
        const int textWidth = metrics.horizontalAdvance(opt.text);
        size.setWidth(textWidth + 2 * kPillHorizontalPadding + 2 * kCellHorizontalMargin + 2);
    }
    return size;
}
