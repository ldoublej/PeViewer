#include <QAction>
#include <QApplication>
#include <QAbstractItemView>
#include <QComboBox>
#include <QCompleter>
#include <QDir>
#include <QFile>
#include <QLocale>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMenuBar>
#include <QSettings>
#include <QStatusBar>
#include <QTemporaryDir>
#include <QTreeView>
#include <SARibbonBar.h>
#include <SARibbonCategory.h>
#include <SARibbonPanel.h>
#include <SARibbonQuickAccessBar.h>
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
        auto* home = window.findChild<SARibbonCategory*>(QStringLiteral("ribbonHome"));
        auto* workspaceCombo = window.findChild<QComboBox*>(QStringLiteral("workspaceCombo"));
        auto* english = window.findChild<QAction*>(QStringLiteral("actionEnglish"));
        auto* chinese = window.findChild<QAction*>(QStringLiteral("actionSimplifiedChinese"));
        require(languagePanel && filePanel && home && workspaceCombo && english &&
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
                    languagePanel->panelName() == QStringLiteral("Language"),
                "English Ribbon labels are incorrect");
        require(workspaceCombo->accessibleName() == QStringLiteral("Workspace") &&
                    workspaceCombo->lineEdit()->placeholderText() == QStringLiteral("Search workspaces..."),
                "English workspace selector is incorrect");
        require(window.statusBar()->currentMessage() == QStringLiteral("Ready"),
                "English status is incorrect");
        require(english->isChecked() && !chinese->isChecked(), "English selection is incorrect");

        chinese->trigger();
        QApplication::processEvents();
        require(languages.language() == QStringLiteral("zh_CN"),
                "Chinese action did not switch language");
        require(home->categoryName() == QStringLiteral("主页") &&
                    filePanel->panelName() == QStringLiteral("文件") &&
                    languagePanel->panelName() == QStringLiteral("语言"),
                "Ribbon labels were not retranslated");
        require(workspaceCombo->accessibleName() == QStringLiteral("工作区") &&
                    workspaceCombo->lineEdit()->placeholderText() == QStringLiteral("搜索工作区..."),
                "Workspace selector was not retranslated");
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

void pressKey(QWidget* widget, int key, const QString& text = QString()) {
    QKeyEvent press(QEvent::KeyPress, key, Qt::NoModifier, text);
    QApplication::sendEvent(widget, &press);
    QKeyEvent release(QEvent::KeyRelease, key, Qt::NoModifier, text);
    QApplication::sendEvent(widget, &release);
    QApplication::processEvents();
}

void selectWorkspace(QComboBox* combo, int row) {
    combo->showPopup();
    combo->view()->setCurrentIndex(combo->model()->index(row, 0));
    pressKey(combo->view(), Qt::Key_Return);
}

void searchWorkspaces(QComboBox* combo, const QString& query) {
    combo->setFocus();
    QApplication::processEvents();
    combo->lineEdit()->selectAll();
    pressKey(combo->lineEdit(), 0, query);
}

void testWorkspaceSelector() {
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
    window.show();
    QApplication::processEvents();
    auto* manager = WorkspaceManager::instance();
    auto* combo = window.findChild<QComboBox*>(QStringLiteral("workspaceCombo"));
    auto* tree = window.findChild<QTreeView*>(QStringLiteral("dependencyTreeView"));
    auto* closeAction = window.findChild<QAction*>(QStringLiteral("actionClose"));
    auto* filePanel = window.findChild<SARibbonPanel*>(QStringLiteral("ribbonFile"));
    require(combo && tree && closeAction && filePanel, "Workspace selector or dependency tree is missing");
    auto* closeButton = filePanel->actionToRibbonToolButton(closeAction);
    require(closeButton && combo->parentWidget() == window.ribbonBar()->quickAccessBar(),
            "Workspace selector is not in the title bar");
    const QPoint comboTop = combo->mapTo(window.ribbonBar(), QPoint());
    require(comboTop.y() >= 0 && comboTop.y() + combo->height() <= window.ribbonBar()->titleBarHeight(),
            "Workspace selector extends outside the title bar");
    require(combo->count() == 0 && combo->currentIndex() == -1 && !combo->isEnabled() &&
                !closeAction->isEnabled(), "Empty workspace controls are enabled or selected");

    Workspace* first = manager->open(firstPath, WorkspaceManager::OpenMode::OpenOnly);
    require(combo->isEnabled() && combo->count() == 1 && combo->currentIndex() == -1 &&
                combo->currentText().isEmpty() && !closeAction->isEnabled() && tree->model()->rowCount() == 0,
            "OpenOnly selected or displayed a workspace");
    require(combo->itemData(0).toString() == firstPath &&
                combo->itemText(0) == QDir::toNativeSeparators(firstPath),
            "Workspace path is missing or its ampersand was changed");
    selectWorkspace(combo, 0);
    require(manager->current() == first && combo->currentData().toString() == firstPath &&
                closeAction->isEnabled() && tree->model()->index(0, 2).data().toString() == firstIdentity,
            "Selecting a workspace did not activate and display it");
    require(window.statusBar()->currentMessage() == QStringLiteral("Loaded app.exe: 1 modules"),
            "Workspace activation did not update the status");

    Workspace* second = manager->open(secondPath);
    require(combo->count() == 2 && combo->currentData().toString() == secondPath &&
                combo->itemText(0) != combo->itemText(1) &&
                tree->model()->index(0, 2).data().toString() == secondIdentity,
            "Workspaces with the same filename cannot be distinguished");
    searchWorkspaces(combo, QStringLiteral("LEFT&PART"));
    auto* completer = combo->completer();
    require(manager->current() == second && completer->completionCount() == 1 &&
                completer->popup()->isVisible(), "Typing did not filter paths without switching workspaces");
    completer->popup()->setCurrentIndex(completer->completionModel()->index(0, 0));
    pressKey(completer->popup(), Qt::Key_Return);
    require(manager->current() == first && combo->currentData().toString() == firstPath &&
                combo->currentText() == QDir::toNativeSeparators(firstPath) &&
                tree->model()->index(0, 2).data().toString() == firstIdentity,
            "Selecting a filtered result did not activate the correct workspace");
    searchWorkspaces(combo, QStringLiteral("RIGHT"));
    require(completer->completionCount() == 1, "Second workspace did not match the query");
    completer->popup()->setCurrentIndex(completer->completionModel()->index(0, 0));
    pressKey(completer->popup(), Qt::Key_Return);
    require(manager->current() == second && combo->currentData().toString() == secondPath,
            "Filtered row zero was not mapped to the correct source workspace");
    manager->activate(first);

    searchWorkspaces(combo, QStringLiteral("APP.EXE"));
    require(completer->completionCount() == 2, "Filename search did not include all matching workspaces");
    pressKey(completer->popup(), Qt::Key_Escape);
    require(!completer->popup()->isVisible() && manager->current() == first &&
                combo->currentText() == QDir::toNativeSeparators(firstPath),
            "Escape did not cancel search and restore the active workspace");
    searchWorkspaces(combo, QStringLiteral("missing-workspace"));
    require(completer->completionCount() == 0 && manager->current() == first,
            "An unmatched query changed the current workspace");
    pressKey(combo->lineEdit(), Qt::Key_Return);
    require(combo->count() == 2 && combo->currentText() == QDir::toNativeSeparators(firstPath),
            "Invalid input inserted a workspace or left stale text");
    searchWorkspaces(combo, QStringLiteral("left"));
    // An outside click dismisses the native popup before focus moves to the tree.
    completer->popup()->hide();
    tree->setFocus();
    QApplication::processEvents();
    require(manager->current() == first && combo->currentText() == QDir::toNativeSeparators(firstPath),
            "Leaving the search field did not restore the current workspace");
    manager->open(firstPath);
    require(combo->count() == 2, "Reopening a workspace duplicated its item");

    {
        PeViewer otherWindow(languages);
        auto* otherCombo = otherWindow.findChild<QComboBox*>(QStringLiteral("workspaceCombo"));
        auto* otherTree = otherWindow.findChild<QTreeView*>(QStringLiteral("dependencyTreeView"));
        require(otherCombo && otherTree && otherCombo->count() == 2 &&
                    otherCombo->currentData().toString() == firstPath &&
                    otherTree->model()->index(0, 2).data().toString() == firstIdentity,
                "New window did not load existing workspaces");
        otherWindow.show();
        QApplication::processEvents();
        selectWorkspace(otherCombo, otherCombo->findData(secondPath));
        require(manager->current() == second && combo->currentData().toString() == secondPath &&
                    tree->model()->index(0, 2).data().toString() == secondIdentity,
                "Workspace activation was not synchronized between windows");
    }

    require(languages.setLanguage(QStringLiteral("zh_CN")), "Could not switch to Chinese");
    QApplication::processEvents();
    require(combo->lineEdit()->placeholderText() == QStringLiteral("搜索工作区...") &&
                combo->currentData().toString() == secondPath &&
                window.statusBar()->currentMessage() == QStringLiteral("已加载 app.exe：1 个模块"),
            "Language switching lost the active workspace or its translated status");
    closeButton->click();
    require(manager->current() == first && combo->count() == 1 &&
                combo->currentData().toString() == firstPath && closeAction->isEnabled() &&
                tree->model()->index(0, 2).data().toString() == firstIdentity,
            "Closing the current workspace did not select and display the fallback");
    second = manager->open(secondPath, WorkspaceManager::OpenMode::OpenOnly);
    require(combo->count() == 2 && combo->currentData().toString() == firstPath,
            "Opening an inactive workspace changed the selection");
    searchWorkspaces(combo, QStringLiteral("right"));
    require(manager->close(second) && combo->count() == 1 &&
                combo->currentData().toString() == firstPath &&
                !completer->popup()->isVisible(),
            "Closing a search result did not clear the stale popup and preserve selection");
    manager->open(secondPath, WorkspaceManager::OpenMode::OpenOnly);
    manager->closeAll();
    require(combo->count() == 0 && combo->currentIndex() == -1 && combo->currentText().isEmpty() &&
                !combo->isEnabled() && !closeAction->isEnabled() && tree->model()->rowCount() == 0 &&
                window.statusBar()->currentMessage() == QStringLiteral("就绪"),
            "Closing all workspaces did not clear the selector and dependency tree");
    manager->open(firstPath);
    closeAction->trigger();
    require(manager->empty() && combo->count() == 0 && !combo->isEnabled() &&
                !closeAction->isEnabled() && tree->model()->rowCount() == 0,
            "Closing the final workspace did not disable workspace controls");
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
        testWorkspaceSelector();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
