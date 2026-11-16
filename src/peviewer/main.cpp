#include <QtWidgets/QApplication>

#include "editor_registration.h"
#include "language_manager.h"
#include "pe_viewer.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    Q_INIT_RESOURCE(SARibbonResource);
    QCoreApplication::setOrganizationName(QStringLiteral("PeViewer"));
    QCoreApplication::setApplicationName(QStringLiteral("PeViewer"));
    // Editors and views must be registered before any document can be opened.
    registerPeEditorViews();
    LanguageManager languageManager;
    PeViewer window(languageManager);
    window.show();
    return app.exec();
}
