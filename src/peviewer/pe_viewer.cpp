#include "pe_viewer.h"

#include <QActionGroup>
#include <QDir>
#include <QEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QMenu>
#include <QStyle>

#include <SARibbonBar.h>
#include <SARibbonCategory.h>
#include <SARibbonPanel.h>

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

    m_workspaceActions = new QActionGroup(this);
    m_workspaceActions->setExclusive(true);
    auto* manager = WorkspaceManager::instance();
    connect(manager, &WorkspaceManager::workspaceOpened, this, &PeViewer::addWorkspaceAction);
    connect(manager, &WorkspaceManager::workspaceClosed, this, &PeViewer::removeWorkspaceAction);
    connect(manager, &WorkspaceManager::workspaceActivated, this, &PeViewer::updateCurrentWorkspace);
    for (const auto& workspace : manager->workspaces()) {
        addWorkspaceAction(workspace.get());
    }
    m_workspaceMenu->setEnabled(!manager->empty());
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
    ribbon->setApplicationButton(nullptr);

    m_homeCategory = ribbon->addCategoryPage(tr("Home"));
    m_homeCategory->setObjectName(QStringLiteral("ribbonHome"));
    m_filePanel = m_homeCategory->addPanel(tr("File"));
    m_filePanel->setObjectName(QStringLiteral("ribbonFile"));
    ui.actionOpen->setIcon(style()->standardIcon(QStyle::SP_DialogOpenButton));
    ui.actionClose->setIcon(style()->standardIcon(QStyle::SP_DialogCloseButton));
    m_filePanel->addLargeAction(ui.actionOpen);
    m_filePanel->addLargeAction(ui.actionClose);

    m_workspacePanel = m_homeCategory->addPanel(tr("Workspace"));
    m_workspacePanel->setObjectName(QStringLiteral("ribbonWorkspace"));
    m_workspaceMenu = new QMenu(tr("Switch workspace"), this);
    m_workspaceMenu->setObjectName(QStringLiteral("menuWorkspace"));
    m_workspaceMenu->setIcon(style()->standardIcon(QStyle::SP_DirOpenIcon));
    m_workspacePanel->addLargeMenu(m_workspaceMenu);

    m_languagePanel = m_homeCategory->addPanel(tr("Language"));
    m_languagePanel->setObjectName(QStringLiteral("ribbonLanguage"));
    m_languagePanel->addSmallAction(ui.actionEnglish);
    m_languagePanel->addSmallAction(ui.actionSimplifiedChinese);
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

void PeViewer::retranslateUi() {
    ui.retranslateUi(this);
    m_homeCategory->setCategoryName(tr("Home"));
    m_filePanel->setPanelName(tr("File"));
    m_workspacePanel->setPanelName(tr("Workspace"));
    m_workspaceMenu->setTitle(tr("Switch workspace"));
    m_languagePanel->setPanelName(tr("Language"));
    updateWorkspaceStatus();
}

void PeViewer::addWorkspaceAction(Workspace* workspace) {
    const QString path = workspace->path();
    const QString displayPath = QDir::toNativeSeparators(path);
    auto* action = m_workspaceMenu->addAction(QString(displayPath).replace('&', "&&"));
    action->setToolTip(displayPath);
    action->setData(path);
    action->setCheckable(true);
    action->setChecked(workspace == WorkspaceManager::instance()->current());
    m_workspaceActions->addAction(action);
    connect(action, &QAction::triggered, this, [path] {
        auto* manager = WorkspaceManager::instance();
        manager->activate(manager->find(path));
    });
    m_workspaceMenu->setEnabled(true);
}

void PeViewer::removeWorkspaceAction(Workspace* workspace) {
    for (auto* action : m_workspaceActions->actions()) {
        if (action->data().toString() == workspace->path()) {
            m_workspaceActions->removeAction(action);
            m_workspaceMenu->removeAction(action);
            action->deleteLater();
            break;
        }
    }
    m_workspaceMenu->setEnabled(!WorkspaceManager::instance()->empty());
}

void PeViewer::updateCurrentWorkspace(Workspace* workspace) {
    ui.actionClose->setEnabled(workspace != nullptr);
    for (auto* action : m_workspaceActions->actions()) {
        action->setChecked(workspace && action->data().toString() == workspace->path());
    }
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
