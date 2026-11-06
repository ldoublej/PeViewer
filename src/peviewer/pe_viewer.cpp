#include "pe_viewer.h"

#include <QActionGroup>
#include <QDir>
#include <QEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>

#include "dependency_graph.h"
#include "language_manager.h"
#include "workspace.h"
#include "workspace_manager.h"

PeViewer::PeViewer(LanguageManager& languageManager, QWidget* parent)
    : QMainWindow(parent), m_languageManager(languageManager) {
    ui.setupUi(this);
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
    ui.menuWorkspace->setEnabled(!manager->empty());
    updateCurrentWorkspace(manager->current());
    retranslateUi();
    updateLanguageActions();
}

PeViewer::~PeViewer() {
    // Detach the model from the graph owned by the singleton manager.
    ui.dependencyTree->clear();
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
    QMainWindow::changeEvent(event);
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
    }
}

void PeViewer::retranslateUi() {
    ui.retranslateUi(this);
    updateWorkspaceStatus();
}

void PeViewer::addWorkspaceAction(Workspace* workspace) {
    const QString path = workspace->path();
    const QString displayPath = QDir::toNativeSeparators(path);
    auto* action = ui.menuWorkspace->addAction(QString(displayPath).replace('&', "&&"));
    action->setToolTip(displayPath);
    action->setData(path);
    action->setCheckable(true);
    action->setChecked(workspace == WorkspaceManager::instance()->current());
    m_workspaceActions->addAction(action);
    connect(action, &QAction::triggered, this, [path] {
        auto* manager = WorkspaceManager::instance();
        manager->activate(manager->find(path));
    });
    ui.menuWorkspace->setEnabled(true);
}

void PeViewer::removeWorkspaceAction(Workspace* workspace) {
    for (auto* action : m_workspaceActions->actions()) {
        if (action->data().toString() == workspace->path()) {
            m_workspaceActions->removeAction(action);
            ui.menuWorkspace->removeAction(action);
            action->deleteLater();
            break;
        }
    }
    ui.menuWorkspace->setEnabled(!WorkspaceManager::instance()->empty());
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
