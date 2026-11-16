#include <QAction>
#include <QApplication>
#include <QAbstractItemView>
#include <QComboBox>
#include <QCompleter>
#include <QDir>
#include <QFile>
#include <QFontMetrics>
#include <QLocale>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMenuBar>
#include <QSettings>
#include <QStackedWidget>
#include <QStatusBar>
#include <QTemporaryDir>
#include <QTreeView>
#include <QToolButton>
#include <QWidget>
#include <editor.h>
#include <SARibbonApplicationButton.h>
#include <SARibbonBar.h>
#include <SARibbonCategory.h>
#include <SARibbonMenu.h>
#include <SARibbonPanel.h>
#include <SARibbonQuickAccessBar.h>
#include <iostream>
#include <stdexcept>

#include "dependencies_test_support.h"
#include "document.h"
#include "document_manager.h"
#include "editor_registration.h"
#include "language_manager.h"
#include "pe_viewer.h"

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
        auto* applicationButton =
            window.findChild<SARibbonApplicationButton*>(QStringLiteral("ribbonApplicationButton"));
        auto* fileMenu = window.findChild<SARibbonMenu*>(QStringLiteral("ribbonFileMenu"));
        auto* openAction = window.findChild<QAction*>(QStringLiteral("actionOpen"));
        auto* closeAction = window.findChild<QAction*>(QStringLiteral("actionClose"));
        auto* home = window.findChild<SARibbonCategory*>(QStringLiteral("ribbonHome"));
        auto* documentCombo = window.findChild<QComboBox*>(QStringLiteral("documentCombo"));
        auto* english = window.findChild<QAction*>(QStringLiteral("actionEnglish"));
        auto* chinese = window.findChild<QAction*>(QStringLiteral("actionSimplifiedChinese"));
        require(languagePanel && applicationButton && fileMenu && openAction && closeAction &&
                    home && documentCombo && english && chinese &&
                    window.menuWidget() == window.ribbonBar(),
                "Ribbon is incomplete or not installed as the menu widget");
        require(applicationButton->menu() == fileMenu && fileMenu->actions().contains(openAction) &&
                    fileMenu->actions().contains(closeAction) &&
                    !window.findChild<SARibbonPanel*>(QStringLiteral("ribbonFile")),
                "File actions are not in the application button menu");
        require(applicationButton->text() == QStringLiteral("File") &&
                    applicationButton->popupMode() == QToolButton::InstantPopup,
                "English file menu is incorrect");
        const auto menuBars = window.findChildren<QMenuBar*>();
        require(menuBars.size() == 1 && menuBars.first() == window.ribbonBar() &&
                    QFile::exists(QStringLiteral(":/SARibbonTheme/resource/theme-base.qss")),
                "Traditional menu bar remains or static Ribbon resources are missing");
        require(languagePanel->actionToRibbonToolButton(english) &&
                    languagePanel->actionToRibbonToolButton(chinese),
                "Language actions are missing from the Ribbon");
        require(home->categoryName() == QStringLiteral("Home") &&
                    applicationButton->text() == QStringLiteral("File") &&
                    languagePanel->panelName() == QStringLiteral("Language"),
                "English Ribbon labels are incorrect");
        require(documentCombo->accessibleName() == QStringLiteral("Document") &&
                    documentCombo->lineEdit()->placeholderText() == QStringLiteral("Search documents..."),
                "English document selector is incorrect");
        require(window.statusBar()->currentMessage() == QStringLiteral("Ready"),
                "English status is incorrect");
        require(english->isChecked() && !chinese->isChecked(), "English selection is incorrect");

        chinese->trigger();
        QApplication::processEvents();
        require(languages.language() == QStringLiteral("zh_CN"),
                "Chinese action did not switch language");
        require(home->categoryName() == QStringLiteral("主页") &&
                    applicationButton->text() == QStringLiteral("文件") &&
                    openAction->text() == QStringLiteral("打开") &&
                    closeAction->text() == QStringLiteral("关闭") &&
                    languagePanel->panelName() == QStringLiteral("语言"),
                "Ribbon labels were not retranslated");
        require(documentCombo->accessibleName() == QStringLiteral("文档") &&
                    documentCombo->lineEdit()->placeholderText() == QStringLiteral("搜索文档..."),
                "Document selector was not retranslated");
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

void selectDocument(QComboBox* combo, int row) {
    combo->showPopup();
    combo->view()->setCurrentIndex(combo->model()->index(row, 0));
    pressKey(combo->view(), Qt::Key_Return);
}

void searchDocuments(QComboBox* combo, const QString& query) {
    combo->setFocus();
    QApplication::processEvents();
    combo->lineEdit()->selectAll();
    pressKey(combo->lineEdit(), 0, query);
}

bool displayShowsIdentity(QStackedWidget* stack, const QString& identity) {
    QWidget* page = stack ? stack->currentWidget() : nullptr;
    auto* tree = page ? page->findChild<QTreeView*>(QStringLiteral("dependencyTreeView")) : nullptr;
    return tree && tree->model() && tree->model()->index(0, 2).data().toString() == identity;
}

void testDocumentSelector() {
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
    auto* manager = DocumentManager::instance();
    auto* combo = window.findChild<QComboBox*>(QStringLiteral("documentCombo"));
    auto* stack = window.findChild<QStackedWidget*>(QStringLiteral("viewStack"));
    auto* homePage = window.findChild<QWidget*>(QStringLiteral("homePage"));
    auto* closeAction = window.findChild<QAction*>(QStringLiteral("actionClose"));
    auto* fileMenu = window.findChild<SARibbonMenu*>(QStringLiteral("ribbonFileMenu"));
    auto* fileButton = window.findChild<SARibbonApplicationButton*>(QStringLiteral("ribbonApplicationButton"));
    require(combo && stack && homePage && closeAction && fileMenu && fileButton &&
                fileMenu->actions().contains(closeAction),
            "Document selector, view stack or file menu is missing");
    require(fileButton->width() - fileButton->fontMetrics().horizontalAdvance(fileButton->text()) <= 40,
            "File button is much wider than its text");
    require(combo->parentWidget() == window.ribbonBar()->quickAccessBar(),
            "Document selector is not in the title bar");
    const QPoint comboTop = combo->mapTo(window.ribbonBar(), QPoint());
    require(comboTop.y() >= 0 && comboTop.y() + combo->height() <= window.ribbonBar()->titleBarHeight(),
            "Document selector extends outside the title bar");
    require(combo->count() == 0 && combo->currentIndex() == -1 && !combo->isEnabled() &&
                !closeAction->isEnabled(), "Empty document controls are enabled or selected");
    require(stack->currentWidget() == homePage, "Empty window does not show the home page");

    Document* first = manager->open(firstPath, DocumentManager::OpenMode::OpenOnly);
    require(combo->isEnabled() && combo->count() == 1 && combo->currentIndex() == -1 &&
                combo->currentText().isEmpty() && !closeAction->isEnabled() &&
                stack->currentWidget() == homePage,
            "OpenOnly selected or displayed a document");
    require(combo->itemData(0).toString() == firstPath &&
                combo->itemText(0) == QDir::toNativeSeparators(firstPath),
            "Document path is missing or its ampersand was changed");
    selectDocument(combo, 0);
    require(manager->current() == first && combo->currentData().toString() == firstPath &&
                closeAction->isEnabled() && displayShowsIdentity(stack, firstIdentity),
            "Selecting a document did not activate and display it");
    require(window.statusBar()->currentMessage() == QStringLiteral("Loaded app.exe: 1 modules"),
            "Document activation did not update the status");

    Document* second = manager->open(secondPath);
    require(combo->count() == 2 && combo->currentData().toString() == secondPath &&
                combo->itemText(0) != combo->itemText(1) &&
                displayShowsIdentity(stack, secondIdentity),
            "Documents with the same filename cannot be distinguished");
    searchDocuments(combo, QStringLiteral("LEFT&PART"));
    auto* completer = combo->completer();
    require(manager->current() == second && completer->completionCount() == 1 &&
                completer->popup()->isVisible(), "Typing did not filter paths without switching documents");
    completer->popup()->setCurrentIndex(completer->completionModel()->index(0, 0));
    pressKey(completer->popup(), Qt::Key_Return);
    require(manager->current() == first && combo->currentData().toString() == firstPath &&
                combo->currentText() == QDir::toNativeSeparators(firstPath) &&
                displayShowsIdentity(stack, firstIdentity),
            "Selecting a filtered result did not activate the correct document");
    searchDocuments(combo, QStringLiteral("RIGHT"));
    require(completer->completionCount() == 1, "Second document did not match the query");
    completer->popup()->setCurrentIndex(completer->completionModel()->index(0, 0));
    pressKey(completer->popup(), Qt::Key_Return);
    require(manager->current() == second && combo->currentData().toString() == secondPath,
            "Filtered row zero was not mapped to the correct source document");
    manager->activate(first);

    searchDocuments(combo, QStringLiteral("APP.EXE"));
    require(completer->completionCount() == 2, "Filename search did not include all matching documents");
    pressKey(completer->popup(), Qt::Key_Escape);
    require(!completer->popup()->isVisible() && manager->current() == first &&
                combo->currentText() == QDir::toNativeSeparators(firstPath),
            "Escape did not cancel search and restore the active document");
    searchDocuments(combo, QStringLiteral("missing-document"));
    require(completer->completionCount() == 0 && manager->current() == first,
            "An unmatched query changed the current document");
    pressKey(combo->lineEdit(), Qt::Key_Return);
    require(combo->count() == 2 && combo->currentText() == QDir::toNativeSeparators(firstPath),
            "Invalid input inserted a document or left stale text");
    searchDocuments(combo, QStringLiteral("left"));
    // An outside click dismisses the native popup before focus moves to the page.
    completer->popup()->hide();
    QWidget* activePage = stack->currentWidget();
    require(activePage != nullptr, "Active document page is missing");
    activePage->setFocus();
    QApplication::processEvents();
    require(manager->current() == first && combo->currentText() == QDir::toNativeSeparators(firstPath),
            "Leaving the search field did not restore the current document");
    manager->open(firstPath);
    require(combo->count() == 2, "Reopening a document duplicated its item");

    {
        PeViewer otherWindow(languages);
        auto* otherCombo = otherWindow.findChild<QComboBox*>(QStringLiteral("documentCombo"));
        auto* otherStack = otherWindow.findChild<QStackedWidget*>(QStringLiteral("viewStack"));
        require(otherCombo && otherStack && otherCombo->count() == 2 &&
                    otherCombo->currentData().toString() == firstPath,
                "New window did not load existing documents");
        otherWindow.show();
        QApplication::processEvents();
        selectDocument(otherCombo, otherCombo->findData(secondPath));
        require(manager->current() == second && combo->currentData().toString() == secondPath,
                "Document activation was not synchronized between windows");
    }

    require(languages.setLanguage(QStringLiteral("zh_CN")), "Could not switch to Chinese");
    QApplication::processEvents();
    require(combo->lineEdit()->placeholderText() == QStringLiteral("搜索文档...") &&
                combo->currentData().toString() == secondPath &&
                window.statusBar()->currentMessage() == QStringLiteral("已加载 app.exe：1 个模块"),
            "Language switching lost the active document or its translated status");
    closeAction->trigger();
    require(manager->current() == first && combo->count() == 1 &&
                combo->currentData().toString() == firstPath && closeAction->isEnabled() &&
                displayShowsIdentity(stack, firstIdentity),
            "Closing the current document did not select and display the fallback");
    second = manager->open(secondPath, DocumentManager::OpenMode::OpenOnly);
    require(combo->count() == 2 && combo->currentData().toString() == firstPath,
            "Opening an inactive document changed the selection");
    searchDocuments(combo, QStringLiteral("right"));
    require(manager->close(second) && combo->count() == 1 &&
                combo->currentData().toString() == firstPath &&
                !completer->popup()->isVisible(),
            "Closing a search result did not clear the stale popup and preserve selection");
    manager->open(secondPath, DocumentManager::OpenMode::OpenOnly);
    manager->closeAll();
    require(combo->count() == 0 && combo->currentIndex() == -1 && combo->currentText().isEmpty() &&
                !combo->isEnabled() && !closeAction->isEnabled() && stack->currentWidget() == homePage &&
                window.statusBar()->currentMessage() == QStringLiteral("就绪"),
            "Closing all documents did not clear the selector and view pages");
    manager->open(firstPath);
    closeAction->trigger();
    require(manager->empty() && combo->count() == 0 && !combo->isEnabled() &&
                !closeAction->isEnabled() && stack->currentWidget() == homePage,
            "Closing the final document did not disable document controls");
    QSettings().remove(QStringLiteral("ui/language"));
}

void testEditorViews() {
    QTemporaryDir temporary;
    require(temporary.isValid(), "Temporary directory failed");
    const QDir dir(temporary.path());
    DependencyTests::writePe(dir, QStringLiteral("editor.exe"));
    const QString path = dir.filePath(QStringLiteral("editor.exe"));

    QSettings().setValue(QStringLiteral("ui/language"), QStringLiteral("en"));
    LanguageManager languages;
    PeViewer window(languages);
    window.show();
    QApplication::processEvents();

    auto* ribbon = window.ribbonBar();
    auto* stack = window.findChild<QStackedWidget*>(QStringLiteral("viewStack"));
    auto* homePage = window.findChild<QWidget*>(QStringLiteral("homePage"));
    auto* home = window.findChild<SARibbonCategory*>(QStringLiteral("ribbonHome"));
    auto* closeAction = window.findChild<QAction*>(QStringLiteral("actionClose"));
    require(ribbon && stack && homePage && home && closeAction, "Editor host widgets are missing");
    require(stack->currentWidget() == homePage, "Home page is not shown without documents");
    require(ribbon->categoryPages().size() == 1, "Unexpected Ribbon tabs without documents");

    auto* manager = DocumentManager::instance();
    Document* document = manager->open(path);
    QApplication::processEvents();
    auto* viewPage = window.findChild<SARibbonCategory*>(QStringLiteral("editorView/dependency-tree"));
    require(document && document->editor() && viewPage && document->editor()->views().size() == 1,
            "Opening a document did not create its editor view");
    require(ribbon->categoryPages().size() == 2, "View tab was not added next to Home");
    require(ribbon->categoryByIndex(ribbon->currentIndex()) == viewPage, "View tab is not selected");
    require(viewPage->categoryName() == QStringLiteral("Dependency"),
            "View tab title is not translated");
    require(stack->currentWidget() != homePage, "View page is not displayed");
    require(stack->count() == 2, "View page was not added to the stack");
    require(closeAction->isEnabled(), "Close action is disabled with an open document");

    // Home switches the central page; the view tab restores it.
    ribbon->setCurrentIndex(ribbon->categoryIndex(home));
    QApplication::processEvents();
    require(stack->currentWidget() == homePage, "Home tab did not show the home page");
    ribbon->setCurrentIndex(ribbon->categoryIndex(viewPage));
    QApplication::processEvents();
    require(stack->currentWidget() != homePage, "View tab did not restore the view page");

    require(languages.setLanguage(QStringLiteral("zh_CN")), "Could not switch to Chinese");
    QApplication::processEvents();
    require(viewPage->categoryName() == QStringLiteral("依赖"), "View tab was not retranslated");
    require(languages.setLanguage(QStringLiteral("en")), "Could not switch to English");
    QApplication::processEvents();
    require(viewPage->categoryName() == QStringLiteral("Dependency"), "View tab title was not restored");

    closeAction->trigger();
    QApplication::processEvents();
    require(manager->empty() && !closeAction->isEnabled() && stack->currentWidget() == homePage,
            "Closing the document did not return to the home page");
    require(window.ribbonBar()->categoryPages().size() == 1, "View tab was not removed");
    require(stack->count() == 1, "View page was not released with its document");
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
    registerPeEditorViews();

    try {
        testLanguageSwitching();
        testDocumentSelector();
        testEditorViews();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
