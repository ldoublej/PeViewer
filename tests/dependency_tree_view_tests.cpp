#include <QApplication>
#include <QImage>
#include <QPainter>
#include <QStandardItemModel>

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

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}
}  // namespace

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QStandardItemModel model;
    auto* root = new QStandardItem(QStringLiteral("root"));
    auto* first = new QStandardItem(QStringLiteral("first"));
    auto* last = new QStandardItem(QStringLiteral("last"));
    first->appendRow(new QStandardItem(QStringLiteral("grandchild")));
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

    view.setLayoutDirection(Qt::RightToLeft);
    require(hasLineBelowCenter(view.paintBranch(first->index(), 28), 20),
            "Right-to-left branch line was drawn on the wrong side");
}
