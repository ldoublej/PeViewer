#include <QApplication>
#include <QCommandLineParser>
#include <QIcon>
#include <QPainter>
#include <QTimer>

#include "components/navigation_tree_widget.h"

namespace {
QIcon navigationIcon(const QString& name, const QPalette& palette) {
    const QIcon source(QStringLiteral(":/navigation-demo/icons/%1.svg").arg(name));
    QPixmap pixmap = source.pixmap(QSize(32, 32));
    if (pixmap.isNull()) {
        qFatal("Could not load navigation icon: %s", qPrintable(name));
    }
    // Keep the prototype's muted line icons readable with the system palette.
    QColor color = palette.color(QPalette::WindowText);
    color.setAlpha(175);
    {
        QPainter painter(&pixmap);
        painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
        painter.fillRect(pixmap.rect(), color);
    }
    pixmap.setDevicePixelRatio(2);
    return QIcon(pixmap);
}

QTreeWidgetItem* makeItem(const QString& text, const QString& icon,
                          const QPalette& palette) {
    auto* item = new QTreeWidgetItem;
    item->setText(0, text);
    item->setToolTip(0, text);
    item->setIcon(0, navigationIcon(icon, palette));
    return item;
}
}  // namespace

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("NavigationTreeDemo"));
    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("PE structure navigation preview"));
    parser.addHelpOption();
    parser.addOption({QStringLiteral("snapshot"),
                      QStringLiteral("Save the navigation preview and exit."),
                      QStringLiteral("path")});
    parser.process(app);

    NavigationTreeWidget navigation;
    navigation.setWindowTitle(QStringLiteral("PE 结构导航"));
    navigation.resize(220, 420);
    const auto palette = navigation.palette();
    navigation.addTopLevelItem(
        makeItem(QStringLiteral("DOS 头"), QStringLiteral("file-code"), palette));

    auto* ntHeaders = new QTreeWidgetItem;
    ntHeaders->setText(0, QStringLiteral("NT Headers"));
    ntHeaders->setFlags(ntHeaders->flags() & ~Qt::ItemIsSelectable);
    ntHeaders->setForeground(0, palette.brush(QPalette::Disabled, QPalette::WindowText));
    navigation.addTopLevelItem(ntHeaders);
    ntHeaders->addChild(makeItem(QStringLiteral("NT 签名"), QStringLiteral("code"), palette));
    ntHeaders->addChild(makeItem(QStringLiteral("COFF 文件头"), QStringLiteral("list"), palette));
    auto* optionalHeader =
        makeItem(QStringLiteral("可选头"), QStringLiteral("list-tree"), palette);
    ntHeaders->addChild(optionalHeader);
    ntHeaders->addChild(makeItem(QStringLiteral("数据目录"), QStringLiteral("folder-tree"), palette));
    ntHeaders->setExpanded(true);

    navigation.addTopLevelItem(
        makeItem(QStringLiteral("节表  5"), QStringLiteral("layers"), palette));
    navigation.addTopLevelItem(
        makeItem(QStringLiteral("导入表"), QStringLiteral("arrow-down-to-line"), palette));
    navigation.addTopLevelItem(
        makeItem(QStringLiteral("导出表"), QStringLiteral("arrow-up-from-line"), palette));
    navigation.setCurrentItem(optionalHeader);
    navigation.show();

    if (parser.isSet(QStringLiteral("snapshot"))) {
        const QString path = parser.value(QStringLiteral("snapshot"));
        QTimer::singleShot(0, &navigation, [&app, &navigation, path] {
            app.exit(navigation.grab().save(path) ? 0 : 1);
        });
    }
    return app.exec();
}
