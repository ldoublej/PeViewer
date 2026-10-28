#include <QtWidgets/QApplication>

#include "language_manager.h"
#include "pe_viewer.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("PeViewer"));
    QCoreApplication::setApplicationName(QStringLiteral("PeViewer"));
    LanguageManager languageManager;
    PeViewer window(languageManager);
    window.show();
    return app.exec();
}
