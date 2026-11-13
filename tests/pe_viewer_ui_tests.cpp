#include <QAction>
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QLocale>
#include <QMenu>
#include <QMenuBar>
#include <QSettings>
#include <QStatusBar>
#include <QTemporaryDir>
#include <QTimer>
#include <QTreeView>
#include <SARibbonBar.h>
#include <SARibbonCategory.h>
#include <SARibbonPanel.h>
#include <iostream>
#include <stdexcept>

#include "dependencies_test_support.h"
#include "language_manager.h"
#include "pe_viewer.h"
#include "workspace.h"
#include "workspace_manager.h"

namespace {
void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void testLanguageSwitching() {
    QSettings().setValue(QStringLiteral("ui/language"), QStringLiteral("en"));
    {
        LanguageManager languages;
        require(languages.language() == QStringLiteral("en"), "English preference not loaded");
        PeViewer window(languages);
        auto* languagePanel = window.findChild<SARibbonPanel*>(QStringLiteral("ribbonLanguage"));
        auto* filePanel = window.findChild<SARibbonPanel*>(QStringLiteral("ribbonFile"));
        auto* workspacePanel = window.findChild<SARibbonPanel*>(QStringLiteral("ribbonWorkspace"));
        auto* home = window.findChild<SARibbonCategory*>(QStringLiteral("ribbonHome"));
        auto* workspaceMenu = window.findChild<QMenu*>(QStringLiteral("menuWorkspace"));
        auto* english = window.findChild<QAction*>(QStringLiteral("actionEnglish"));
        auto* chinese = window.findChild<QAction*>(QStringLiteral("actionSimplifiedChinese"));
        require(languagePanel && filePanel && workspacePanel && home && workspaceMenu && english &&
                    chinese && window.menuWidget() == window.ribbonBar(),
                "Ribbon is incomplete or not installed as the menu widget");
        const auto menuBars = window.findChildren<QMenuBar*>();
        require(menuBars.size() == 1 && menuBars.first() == window.ribbonBar() &&
                    QFile::exists(QStringLiteral(":/SARibbonTheme/resource/theme-base.qss")),
                "Traditional menu bar remains or static Ribbon resources are missing");
        require(languagePanel->actionToRibbonToolButton(english) &&
                    languagePanel->actionToRibbonToolButton(chinese),
                "Language actions are missing from the Ribbon");
        require(home->categoryName() == QStringLiteral("Home") &&
                    filePanel->panelName() == QStringLiteral("File") &&
                    workspacePanel->panelName() == QStringLiteral("Workspace") &&
                    languagePanel->panelName() == QStringLiteral("Language"),
                "English Ribbon labels are incorrect");
        require(workspaceMenu->title() == QStringLiteral("Switch workspace"),
                "English workspace button is incorrect");
        require(window.statusBar()->currentMessage() == QStringLiteral("Ready"),
                "English status is incorrect");
        require(english->isChecked() && !chinese->isChecked(), "English selection is incorrect");

        chinese->trigger();
        QApplication::processEvents();
        require(languages.language() == QStringLiteral("zh_CN"),
                "Chinese action did not switch language");
        require(home->categoryName() == QStringLiteral("主页") &&
                    filePanel->panelName() == QStringLiteral("文件") &&
                    workspacePanel->panelName() == QStringLiteral("工作区") &&
                    languagePanel->panelName() == QStringLiteral("语言"),
                "Ribbon labels were not retranslated");
        require(workspaceMenu->title() == QStringLiteral("切换工作区"),
                "Workspace button was not retranslated");
        require(window.statusBar()->currentMessage() == QStringLiteral("就绪"),
                "C++ text was not retranslated");
        require(chinese->isChecked() && !english->isChecked(), "Chinese selection is incorrect");

        english->trigger();
        QApplication::processEvents();
        require(languagePanel->panelName() == QStringLiteral("Language"),
                "Switching back to English failed");
        require(window.statusBar()->currentMessage() == QStringLiteral("Ready"),
                "English status was not restored");

        chinese->trigger();
        QApplication::processEvents();
        require(!languages.setLanguage(QStringLiteral("unsupported")),
                "Unsupported language accepted");
        require(languages.language() == QStringLiteral("zh_CN"),
                "Invalid language changed selection");
        require(languagePanel->panelName() == QStringLiteral("语言"),
                "Invalid language replaced translation");
        require(
            QSettings().value(QStringLiteral("ui/language")).toString() == QStringLiteral("zh_CN"),
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

    QSettings().setValue(QStringLiteral("ui/language"), QStringLiteral("unsupported"));
    LanguageManager fallback;
    const QString expected = QLocale::system().language() == QLocale::Chinese
                                 ? QStringLiteral("zh_CN")
                                 : QStringLiteral("en");
    require(fallback.language() == expected,
            "Invalid preference did not fall back to system language");

    QSettings().remove(QStringLiteral("ui/language"));
}

void testWorkspaceMenu() {
    QTemporaryDir temporary;
    require(temporary.isValid(), "Temporary directory failed");
    const QDir firstDir(temporary.path() + QStringLiteral("/left&part"));
    const QDir secondDir(temporary.path() + QStringLiteral("/right"));
    DependencyTests::writePe(firstDir, QStringLiteral("app.exe"));
    DependencyTests::writePe(secondDir, QStringLiteral("app.exe"));
    const QString firstPath = firstDir.filePath(QStringLiteral("app.exe"));
    const QString secondPath = secondDir.filePath(QStringLiteral("app.exe"));
    const QString firstIdentity = FileModuleLocator::fileIdentity(firstPath);
    const QString secondIdentity = FileModuleLocator::fileIdentity(secondPath);

    QSettings().setValue(QStringLiteral("ui/language"), QStringLiteral("en"));
    LanguageManager languages;
    PeViewer window(languages);
    auto* manager = WorkspaceManager::instance();
    auto* menu = window.findChild<QMenu*>(QStringLiteral("menuWorkspace"));
    auto* tree = window.findChild<QTreeView*>(QStringLiteral("dependencyTreeView"));
    auto* closeAction = window.findChild<QAction*>(QStringLiteral("actionClose"));
    auto* workspacePanel = window.findChild<SARibbonPanel*>(QStringLiteral("ribbonWorkspace"));
    auto* filePanel = window.findChild<SARibbonPanel*>(QStringLiteral("ribbonFile"));
    auto* openAction = window.findChild<QAction*>(QStringLiteral("actionOpen"));
    require(menu && tree && closeAction && openAction && workspacePanel && filePanel,
            "Workspace Ribbon or dependency tree is missing");
    auto* workspaceButton = workspacePanel->actionToRibbonToolButton(menu->menuAction());
    auto* closeButton = filePanel->actionToRibbonToolButton(closeAction);
    require(workspaceButton && closeButton && filePanel->actionToRibbonToolButton(openAction),
            "File or workspace Ribbon buttons are missing");
    require(workspaceButton->defaultAction()->menu() == menu,
            "Workspace Ribbon action is not connected to its menu");
    require(workspaceButton->popupMode() == QToolButton::InstantPopup,
            "Workspace Ribbon button does not open its menu directly");
    require(menu->actions().isEmpty() && !menu->isEnabled(), "Empty workspace menu is enabled");
    require(!closeAction->isEnabled(), "Close is enabled without a current workspace");
    require(!workspaceButton->isEnabled() && !closeButton->isEnabled(),
            "Ribbon buttons are enabled without an open workspace");

    Workspace* first = manager->open(firstPath, WorkspaceManager::OpenMode::OpenOnly);
    require(menu->isEnabled() && menu->actions().size() == 1, "Opened workspace is not listed");
    require(workspaceButton->isEnabled(), "Workspace Ribbon button did not become enabled");
    auto* firstAction = menu->actions().first();
    require(firstAction->isCheckable() && !firstAction->isChecked() && !closeAction->isEnabled() &&
                tree->model()->rowCount() == 0,
            "OpenOnly selected or displayed a workspace");
    require(firstAction->data().toString() == firstPath &&
                firstAction->text() == QDir::toNativeSeparators(firstPath).replace('&', "&&"),
            "Workspace path is missing or its mnemonic is not escaped");
    bool workspacePopupShown = false;
    QTimer::singleShot(0, menu, [&] {
        workspacePopupShown = menu->isVisible();
        if (workspacePopupShown) {
            firstAction->trigger();
            menu->close();
        }
    });
    workspaceButton->showMenu();
    require(workspacePopupShown, "Workspace Ribbon button did not open its popup menu");
    require(manager->current() == first && firstAction->isChecked() && closeAction->isEnabled() &&
                tree->model()->index(0, 2).data().toString() == firstIdentity,
            "Selecting the workspace did not activate and display it");
    require(window.statusBar()->currentMessage() == QStringLiteral("Loaded app.exe: 1 modules"),
            "Workspace activation did not update the status");

    Workspace* second = manager->open(secondPath);
    require(menu->actions().size() == 2, "Second workspace is not listed");
    auto* secondAction = menu->actions().last();
    require(secondAction->isChecked() && !firstAction->isChecked() &&
                firstAction->text() != secondAction->text() &&
                tree->model()->index(0, 2).data().toString() == secondIdentity,
            "Workspaces with the same filename cannot be distinguished or switched");
    firstAction->trigger();
    firstAction->trigger();
    require(manager->current() == first && firstAction->isChecked() && !secondAction->isChecked() &&
                tree->model()->index(0, 2).data().toString() == firstIdentity,
            "Menu selection did not switch back to the first workspace");
    manager->open(firstPath);
    require(menu->actions().size() == 2, "Reopening a workspace duplicated its menu item");

    {
        PeViewer otherWindow(languages);
        auto* otherMenu = otherWindow.findChild<QMenu*>(QStringLiteral("menuWorkspace"));
        auto* otherTree = otherWindow.findChild<QTreeView*>(QStringLiteral("dependencyTreeView"));
        require(otherMenu && otherTree && otherMenu->actions().size() == 2 &&
                    otherMenu->actions().first()->isChecked() &&
                    otherTree->model()->index(0, 2).data().toString() == firstIdentity,
                "New window did not load existing workspaces");
        otherMenu->actions().last()->trigger();
        require(manager->current() == second && secondAction->isChecked() &&
                    !firstAction->isChecked() &&
                    tree->model()->index(0, 2).data().toString() == secondIdentity,
                "Workspace activation was not synchronized between windows");
    }

    require(languages.setLanguage(QStringLiteral("zh_CN")), "Could not switch to Chinese");
    QApplication::processEvents();
    require(menu->title() == QStringLiteral("切换工作区") && secondAction->isChecked() &&
                window.statusBar()->currentMessage() == QStringLiteral("已加载 app.exe：1 个模块"),
            "Language switching lost the active workspace or its translated status");

    closeButton->click();
    require(manager->current() == first && menu->actions().size() == 1 &&
                firstAction->isChecked() && closeAction->isEnabled() &&
                tree->model()->index(0, 2).data().toString() == firstIdentity,
            "Closing the current workspace did not remove its action and display the fallback");
    second = manager->open(secondPath, WorkspaceManager::OpenMode::OpenOnly);
    require(menu->actions().size() == 2 && firstAction->isChecked() &&
                !menu->actions().last()->isChecked(),
            "Opening an inactive workspace changed the menu selection");
    require(manager->close(second) && menu->actions().size() == 1 && firstAction->isChecked(),
            "Closing an inactive workspace did not preserve the menu selection");
    manager->open(secondPath, WorkspaceManager::OpenMode::OpenOnly);
    manager->closeAll();
    require(menu->actions().isEmpty() && !menu->isEnabled() && !closeAction->isEnabled() &&
                tree->model()->rowCount() == 0 &&
                window.statusBar()->currentMessage() == QStringLiteral("就绪"),
            "Closing all workspaces did not clear the menu and dependency tree");
    manager->open(firstPath);
    require(closeAction->isEnabled(), "Opening a workspace did not reenable Close");
    closeAction->trigger();
    require(manager->empty() && manager->current() == nullptr && !closeAction->isEnabled() &&
                menu->actions().isEmpty() && !menu->isEnabled() && tree->model()->rowCount() == 0 &&
                window.statusBar()->currentMessage() == QStringLiteral("就绪"),
            "Close did not clear the final workspace and disable itself");
    QSettings().remove(QStringLiteral("ui/language"));
}
}  // namespace

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    Q_INIT_RESOURCE(SARibbonResource);
    QCoreApplication::setOrganizationName(QStringLiteral("PeViewerTests"));
    QCoreApplication::setApplicationName(QStringLiteral("UiTranslations"));
    QTemporaryDir settingsDirectory;
    if (!settingsDirectory.isValid()) {
        return 1;
    }
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settingsDirectory.path());

    try {
        testLanguageSwitching();
        testWorkspaceMenu();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
