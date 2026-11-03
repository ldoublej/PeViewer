#pragma once

#include <QStyledItemDelegate>

class DependencyTreeDelegate : public QStyledItemDelegate {
    Q_OBJECT

public:
    explicit DependencyTreeDelegate(QObject* parent = nullptr);

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;

protected:
    void initStyleOption(QStyleOptionViewItem* option, const QModelIndex& index) const override;
};
