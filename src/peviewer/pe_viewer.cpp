#include "pe_viewer.h"

#include <QAbstractItemView>
#include <QActionGroup>
#include <QComboBox>
#include <QCompleter>
#include <QDir>
#include <QEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontMetrics>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMessageBox>
#include <QSignalBlocker>
#include <QStyle>
#include <QTimer>
#include <QToolButton>

#include <SARibbonApplicationButton.h>
#include <SARibbonBar.h>
#include <SARibbonCategory.h>
#include <SARibbonMenu.h>
#include <SARibbonPanel.h>
#include <SARibbonQuickAccessBar.h>

#include "dependency_graph.h"
#include "language_manager.h"
#include "workspace.h"
#include "workspace_manager.h"

PeViewer::PeViewer(LanguageManager& languageManager, QWidget* parent)
    : SARibbonMainWindow(parent), m_languageManager(languageManager) {
    ui.setupUi(this);
    setupRibbon();
    auto* languageActions = new QActionGroup(this);
    languageActions->setExclusive(true);
    languageActions->addAction(ui.actionEnglish);
    languageActions->addAction(ui.actionSimplifiedChinese);

    connect(ui.actionEnglish, &QAction::triggered, this, [this] {
        m_languageManager.setLanguage(QStringLiteral("en"));
        updateLanguageActions();
    });
    connect(ui.actionSimplifiedChinese, &QAction::triggered, this, [this] {
        m_languageManager.setLanguage(QStringLiteral("zh_CN"));
        updateLanguageActions();
    });
    connect(&m_languageManager, &LanguageManager::languageChanged, this,
            &PeViewer::updateLanguageActions);

    auto* manager = WorkspaceManager::instance();
    connect(manager, &WorkspaceManager::workspaceOpened, this, &PeViewer::addWorkspace);
    connect(manager, &WorkspaceManager::workspaceClosed, this, &PeViewer::removeWorkspace);
    connect(manager, &WorkspaceManager::workspaceActivated, this, &PeViewer::updateCurrentWorkspace);
    for (const auto& workspace : manager->workspaces()) {
        addWorkspace(workspace.get());
    }
    updateCurrentWorkspace(manager->current());
    retranslateUi();
    updateLanguageActions();
}

PeViewer::~PeViewer() {
    // Detach the model from the graph owned by the singleton manager.
    ui.dependencyTree->clear();
}

void PeViewer::setupRibbon() {
    auto* ribbon = ribbonBar();
    ribbon->setObjectName(QStringLiteral("ribbonBar"));
    ribbon->setRibbonStyle(SARibbonBar::RibbonStyleLooseTwoRow);
    setupFileMenu();

    m_homeCategory = ribbon->addCategoryPage(tr("Home"));
    m_homeCategory->setObjectName(QStringLiteral("ribbonHome"));

    m_workspaceCombo = new QComboBox(ribbon);
    m_workspaceCombo->setObjectName(QStringLiteral("workspaceCombo"));
    m_workspaceCombo->setEditable(true);
    m_workspaceCombo->setInsertPolicy(QComboBox::NoInsert);
    m_workspaceCombo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    m_workspaceCombo->setFixedWidth(320);
    m_workspaceCombo->setMaxVisibleItems(12);
    auto* completer = m_workspaceCombo->completer();
    completer->setCaseSensitivity(Qt::CaseInsensitive);
    completer->setFilterMode(Qt::MatchContains);
    completer->setCompletionMode(QCompleter::PopupCompletion);
    completer->setMaxVisibleItems(12);
    completer->popup()->installEventFilter(this);
    ribbon->quickAccessBar()->addWidget(m_workspaceCombo);
    setFramelessHitTestVisible(m_workspaceCombo);
    setFramelessHitTestVisible(m_workspaceCombo->lineEdit());
    m_workspaceCombo->installEventFilter(this);
    m_workspaceCombo->lineEdit()->installEventFilter(this);
    connect(m_workspaceCombo, QOverload<int>::of(&QComboBox::activated), this, [this](int index) {
        auto* manager = WorkspaceManager::instance();
        const QString path = m_workspaceCombo->itemData(index).toString();
        if (!path.isEmpty()) {
            manager->activate(manager->find(path));
        }
        updateWorkspaceSelection();
    });
    connect(m_workspaceCombo->lineEdit(), &QLineEdit::editingFinished, this, [this] {
        // Let completion activation finish before restoring a cancelled or invalid query.
        QTimer::singleShot(0, this, &PeViewer::updateWorkspaceSelection);
    });

    m_languagePanel = m_homeCategory->addPanel(tr("Language"));
    m_languagePanel->setObjectName(QStringLiteral("ribbonLanguage"));
    m_languagePanel->addSmallAction(ui.actionEnglish);
    m_languagePanel->addSmallAction(ui.actionSimplifiedChinese);
}

void PeViewer::setupFileMenu() {
    // SARibbonBar already created a default application button; reuse it so the
    // Ribbon stylesheet keeps applying, and only create one if it was removed.
    auto* ribbon = ribbonBar();
    m_applicationButton = qobject_cast<SARibbonApplicationButton*>(ribbon->applicationButton());
    if (!m_applicationButton) {
        m_applicationButton = new SARibbonApplicationButton(this);
        ribbon->setApplicationButton(m_applicationButton);
    }
    m_applicationButton->setObjectName(QStringLiteral("ribbonApplicationButton"));
    m_applicationButton->setPopupMode(QToolButton::InstantPopup);
    m_applicationButton->setText(tr("File"));

    ui.actionOpen->setIcon(style()->standardIcon(QStyle::SP_DialogOpenButton));
    ui.actionClose->setIcon(style()->standardIcon(QStyle::SP_DialogCloseButton));

    m_fileMenu = new SARibbonMenu(tr("File"), ribbon);
    m_fileMenu->setObjectName(QStringLiteral("ribbonFileMenu"));
    m_fileMenu->addAction(ui.actionOpen);
    m_fileMenu->addSeparator();
    m_fileMenu->addAction(ui.actionClose);
    m_applicationButton->setMenu(m_fileMenu);
}

void PeViewer::updateFileButtonWidth() {
    // SARibbon derives the application button width from its size hint, whose height the
    // Ribbon stylesheet collapses to a few pixels, which inflates the button. Pin the
    // width to the text plus padding so the File button stays as narrow as its label.
    const QFontMetrics metrics = m_applicationButton->fontMetrics();
    m_applicationButton->setFixedWidth(metrics.horizontalAdvance(m_applicationButton->text()) + 32);
}

void PeViewer::on_actionOpen_triggered(bool b) {
    Q_UNUSED(b);
    const QString fileName = QFileDialog::getOpenFileName(this, tr("Open PE File"));
    if (fileName.isEmpty()) {
        return;
    }

    auto* manager = WorkspaceManager::instance();
    try {
        manager->open(fileName);
    } catch (const std::exception& error) {
        QMessageBox::warning(this, tr("Open Failed"),
                             tr("Could not load %1:\n%2")
                                 .arg(fileName, QString::fromUtf8(error.what())));
    }
}

void PeViewer::on_actionClose_triggered() {
    auto* manager = WorkspaceManager::instance();
    manager->close(manager->current());
}

void PeViewer::changeEvent(QEvent* event) {
    SARibbonMainWindow::changeEvent(event);
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
    }
}

bool PeViewer::eventFilter(QObject* watched, QEvent* event) {
    if (m_workspaceCombo &&
        (watched == m_workspaceCombo || watched == m_workspaceCombo->lineEdit() ||
         watched == m_workspaceCombo->completer()->popup())) {
        if (event->type() == QEvent::KeyPress &&
            static_cast<QKeyEvent*>(event)->key() == Qt::Key_Escape) {
            m_workspaceCombo->hidePopup();
            updateWorkspaceSelection();
            return true;
        }
        if (event->type() == QEvent::FocusIn &&
            watched != m_workspaceCombo->completer()->popup()) {
            QTimer::singleShot(0, m_workspaceCombo->lineEdit(), &QLineEdit::selectAll);
        }
        if (event->type() == QEvent::FocusOut ||
            (watched == m_workspaceCombo->completer()->popup() && event->type() == QEvent::Hide)) {
            QTimer::singleShot(0, this, [this] {
                if (!m_workspaceCombo->hasFocus() && !m_workspaceCombo->lineEdit()->hasFocus() &&
                    !m_workspaceCombo->completer()->popup()->isVisible()) {
                    updateWorkspaceSelection();
                }
            });
        }
    }
    return SARibbonMainWindow::eventFilter(watched, event);
}

void PeViewer::retranslateUi() {
    ui.retranslateUi(this);
    m_homeCategory->setCategoryName(tr("Home"));
    m_applicationButton->setText(tr("File"));
    m_fileMenu->setTitle(tr("File"));
    updateFileButtonWidth();
    m_workspaceCombo->setAccessibleName(tr("Workspace"));
    m_workspaceCombo->lineEdit()->setPlaceholderText(tr("Search workspaces..."));
    m_languagePanel->setPanelName(tr("Language"));
    updateWorkspaceStatus();
    updateWorkspaceSelection();
}

void PeViewer::addWorkspace(Workspace* workspace) {
    const QString path = workspace->path();
    const QString displayPath = QDir::toNativeSeparators(path);
    const QSignalBlocker blocker(m_workspaceCombo);
    m_workspaceCombo->addItem(displayPath, path);
    m_workspaceCombo->setItemData(m_workspaceCombo->count() - 1, displayPath, Qt::ToolTipRole);
    updateWorkspaceSelection();
}

void PeViewer::removeWorkspace(Workspace* workspace) {
    const QSignalBlocker blocker(m_workspaceCombo);
    m_workspaceCombo->removeItem(m_workspaceCombo->findData(workspace->path()));
    updateWorkspaceSelection();
}

void PeViewer::updateWorkspaceSelection() {
    const QSignalBlocker blocker(m_workspaceCombo);
    auto* current = WorkspaceManager::instance()->current();
    const int index = current ? m_workspaceCombo->findData(current->path()) : -1;
    m_workspaceCombo->completer()->popup()->hide();
    m_workspaceCombo->setCurrentIndex(index);
    // setCurrentIndex does not restore edited text when the index is unchanged.
    m_workspaceCombo->setEditText(index >= 0 ? m_workspaceCombo->itemText(index) : QString());
    m_workspaceCombo->setToolTip(current ? QDir::toNativeSeparators(current->path())
                                        : tr("Search workspaces..."));
    m_workspaceCombo->setEnabled(m_workspaceCombo->count() > 0);
}

void PeViewer::updateCurrentWorkspace(Workspace* workspace) {
    ui.actionClose->setEnabled(workspace != nullptr);
    updateWorkspaceSelection();
    ui.dependencyTree->setGraph(workspace ? &workspace->graph() : nullptr);
    updateWorkspaceStatus();
}

void PeViewer::updateWorkspaceStatus() {
    if (auto* workspace = WorkspaceManager::instance()->current()) {
        ui.statusBar->showMessage(
            tr("Loaded %1: %2 modules")
                .arg(QFileInfo(workspace->path()).fileName())
                .arg(static_cast<qulonglong>(workspace->graph().moduleCount())));
    } else {
        ui.statusBar->showMessage(tr("Ready"));
    }
}

void PeViewer::updateLanguageActions() {
    ui.actionEnglish->setChecked(m_languageManager.language() == QStringLiteral("en"));
    ui.actionSimplifiedChinese->setChecked(m_languageManager.language() == QStringLiteral("zh_CN"));
}
