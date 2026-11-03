#include "pe_viewer.h"

#include <QActionGroup>
#include <QEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>

#include "dependency_graph.h"
#include "language_manager.h"
#include "workspace.h"

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
    retranslateUi();
    updateLanguageActions();
}

PeViewer::~PeViewer() {
    // The model borrows the workspace's graph, so detach it before the manager
    // destroys the current workspace.
    ui.dependencyTree->clear();
}

void PeViewer::on_actionOpen_triggered(bool b) {
    Q_UNUSED(b);
    const QString fileName = QFileDialog::getOpenFileName(this, tr("Open PE File"));
    if (fileName.isEmpty()) {
        return;
    }

    // Detach first; if the new file fails to open the old session is reattached.
    ui.dependencyTree->clear();
    try {
        Workspace* workspace = m_workspaceManager.open(fileName);
        ui.dependencyTree->setGraph(&workspace->graph());
        ui.statusBar->showMessage(
            tr("Loaded %1: %2 modules")
                .arg(QFileInfo(fileName).fileName())
                .arg(static_cast<qulonglong>(workspace->graph().moduleCount())));
    } catch (const std::exception& error) {
        if (Workspace* current = m_workspaceManager.current()) {
            ui.dependencyTree->setGraph(&current->graph());
        }
        QMessageBox::warning(this, tr("Open Failed"),
                             tr("Could not load %1:\n%2")
                                 .arg(fileName, QString::fromUtf8(error.what())));
    }
}

void PeViewer::changeEvent(QEvent* event) {
    QMainWindow::changeEvent(event);
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
    }
}

void PeViewer::retranslateUi() {
    ui.retranslateUi(this);
    ui.statusBar->showMessage(tr("Ready"));
}

void PeViewer::updateLanguageActions() {
    ui.actionEnglish->setChecked(m_languageManager.language() == QStringLiteral("en"));
    ui.actionSimplifiedChinese->setChecked(m_languageManager.language() == QStringLiteral("zh_CN"));
}
