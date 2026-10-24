#include "PeViewer.h"
#include <QtWidgets/QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    PeViewer window;
    window.show();
    return app.exec();
}
