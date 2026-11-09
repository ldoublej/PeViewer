#pragma once

#include <QTreeView>

// 使用虚线连接依赖节点，并单独绘制展开控件。
class DependencyTreeView : public QTreeView {
    Q_OBJECT

public:
    explicit DependencyTreeView(QWidget* parent = nullptr);

protected:
    void drawBranches(QPainter* painter, const QRect& rect,
                      const QModelIndex& index) const override;
};
