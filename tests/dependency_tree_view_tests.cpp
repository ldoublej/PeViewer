#include <QApplication>
#include <QImage>
#include <QPainter>
#include <QRect>
#include <QStandardItemModel>

#include <iostream>
#include <stdexcept>

#include "components/dependency_tree_view.h"

namespace {
class BranchProbe final : public DependencyTreeView {
public:
    QImage paintBranch(const QModelIndex& index, int width) const {
        QImage image(width, 24, QImage::Format_RGB32);
        image.fill(Qt::white);
        QPainter painter(&image);
        drawBranches(&painter, image.rect(), index);
        return image;
    }
};

bool hasLineBelowCenter(const QImage& image, int x) {
    for (int y = 15; y < image.height() - 1; ++y) {
        if (image.pixelColor(x, y) != Qt::white) {
            return true;
        }
    }
    return false;
}

// 统计指示器上方的着色像素：展开/收起三角只画在中心行以上，
// 水平连接线在中心行、祖先竖线在中心行以下，不会造成干扰。
int coloredPixelsAboveCenter(const QImage& image, const QRect& area) {
    int count = 0;
    for (int y = area.top(); y <= area.bottom(); ++y) {
        for (int x = area.left(); x <= area.right(); ++x) {
            if (image.pixelColor(x, y) != Qt::white) {
                ++count;
            }
        }
    }
    return count;
}

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void branchPainting() {
    QStandardItemModel model;
    auto* root = new QStandardItem(QStringLiteral("root"));
    auto* first = new QStandardItem(QStringLiteral("first"));
    auto* last = new QStandardItem(QStringLiteral("last"));
    first->appendRow(new QStandardItem(QStringLiteral("grandchild")));
    last->appendRow(new QStandardItem(QStringLiteral("collapsed child")));
    root->appendRow(first);
    root->appendRow(last);
    model.appendRow(root);

    BranchProbe view;
    view.setModel(&model);
    view.setIndentation(14);
    view.expand(root->index());
    view.expand(first->index());

    // 后续兄弟节点存在时，祖先竖线须贯穿当前行及其展开的子树。
    require(hasLineBelowCenter(view.paintBranch(first->index(), 28), 7),
            "First child did not continue its parent's line");
    require(hasLineBelowCenter(view.paintBranch(first->child(0)->index(), 42), 7),
            "Grandchild lost the line leading to its parent's later sibling");
    require(!hasLineBelowCenter(view.paintBranch(last->index(), 28), 7),
            "Last child incorrectly extended its parent's line");

    // 展开/收起三角：父节点必须有指示器，叶子节点不能有。
    require(coloredPixelsAboveCenter(view.paintBranch(root->index(), 14), QRect(3, 7, 9, 5)) > 0,
            "Expanded root has no expand indicator");
    require(coloredPixelsAboveCenter(view.paintBranch(first->index(), 28), QRect(17, 7, 9, 5)) > 0,
            "Expanded node has no expand indicator");
    require(coloredPixelsAboveCenter(view.paintBranch(last->index(), 28), QRect(17, 7, 9, 5)) > 0,
            "Collapsed node has no expand indicator");
    require(coloredPixelsAboveCenter(view.paintBranch(first->child(0)->index(), 42), QRect(31, 7, 9, 5)) == 0,
            "Leaf node drew an expand indicator");

    view.setLayoutDirection(Qt::RightToLeft);
    require(hasLineBelowCenter(view.paintBranch(first->index(), 28), 20),
            "Right-to-left branch line was drawn on the wrong side");
    require(coloredPixelsAboveCenter(view.paintBranch(last->index(), 28), QRect(2, 7, 9, 5)) > 0,
            "Right-to-left collapsed node has no expand indicator");
}
}  // namespace

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    try {
        branchPainting();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
