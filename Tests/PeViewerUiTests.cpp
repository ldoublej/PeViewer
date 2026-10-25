#include <QAction>
#include <QApplication>
#include <QLocale>
#include <QMenu>
#include <QSettings>
#include <QStatusBar>
#include <QTemporaryDir>
#include <iostream>
#include <stdexcept>

#include "LanguageManager.h"
#include "PeViewer.h"

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void testLanguageSwitching() {
    QSettings().setValue(QStringLiteral("ui/language"), QStringLiteral("en"));
    {
        LanguageManager languages;
        require(languages.language() == QStringLiteral("en"),
                "English preference not loaded");
        PeViewer window(languages);
        auto* menu = window.findChild<QMenu*>(QStringLiteral("menuLanguage"));
        auto* english =
            window.findChild<QAction*>(QStringLiteral("actionEnglish"));
        auto* chinese = window.findChild<QAction*>(
            QStringLiteral("actionSimplifiedChinese"));
        require(menu && english && chinese, "Language menu is incomplete");
        require(menu->title() == QStringLiteral("&Language"),
                "English menu is incorrect");
        require(window.statusBar()->currentMessage() == QStringLiteral("Ready"),
                "English status is incorrect");
        require(english->isChecked() && !chinese->isChecked(),
                "English selection is incorrect");

        chinese->trigger();
        QApplication::processEvents();
        require(languages.language() == QStringLiteral("zh_CN"),
                "Chinese action did not switch language");
        require(menu->title() == QStringLiteral("语言(&L)"),
                "Designer UI was not retranslated");
        require(window.statusBar()->currentMessage() == QStringLiteral("就绪"),
                "C++ text was not retranslated");
        require(chinese->isChecked() && !english->isChecked(),
                "Chinese selection is incorrect");

        english->trigger();
        QApplication::processEvents();
        require(menu->title() == QStringLiteral("&Language"),
                "Switching back to English failed");
        require(window.statusBar()->currentMessage() == QStringLiteral("Ready"),
                "English status was not restored");

        chinese->trigger();
        QApplication::processEvents();
        require(!languages.setLanguage(QStringLiteral("unsupported")),
                "Unsupported language accepted");
        require(languages.language() == QStringLiteral("zh_CN"),
                "Invalid language changed selection");
        require(menu->title() == QStringLiteral("语言(&L)"),
                "Invalid language replaced translation");
        require(QSettings().value(QStringLiteral("ui/language")).toString() ==
                    QStringLiteral("zh_CN"),
                "Language preference was not saved");
    }

    {
        LanguageManager languages;
        PeViewer window(languages);
        require(languages.language() == QStringLiteral("zh_CN"),
                "Saved Chinese preference not restored");
        require(window.statusBar()->currentMessage() == QStringLiteral("就绪"),
                "Translation not loaded before window creation");
    }

    QSettings().setValue(QStringLiteral("ui/language"),
                         QStringLiteral("unsupported"));
    LanguageManager fallback;
    const QString expected = QLocale::system().language() == QLocale::Chinese
                                 ? QStringLiteral("zh_CN")
                                 : QStringLiteral("en");
    require(fallback.language() == expected,
            "Invalid preference did not fall back to system language");

    QSettings().remove(QStringLiteral("ui/language"));
}
}  // namespace

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("PeViewerTests"));
    QCoreApplication::setApplicationName(QStringLiteral("UiTranslations"));
    QTemporaryDir settingsDirectory;
    if (!settingsDirectory.isValid()) return 1;
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope,
                       settingsDirectory.path());

    try {
        testLanguageSwitching();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
