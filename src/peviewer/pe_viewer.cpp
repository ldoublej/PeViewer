#include "pe_viewer.h"

#include <QActionGroup>
#include <QEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <memory>

#include "dependency_graph.h"
#include "language_manager.h"
#include "pe_file.h"
#include "pe_file_data_source.h"

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
    // The model borrows the graph, so detach it before members are destroyed.
    ui.dependencyTree->clear();
}

void PeViewer::on_actionOpen_triggered(bool b) {
    Q_UNUSED(b);
    const QString fileName = QFileDialog::getOpenFileName(this, tr("Open PE File"));
    if (fileName.isEmpty()) {
        return;
    }

    try {
        auto dataSource = std::make_unique<PeFileDataSource>(fileName);
        auto peFile = std::make_unique<PeFile>(*dataSource);
        peFile->parse();
        auto graph = std::make_unique<DependencyGraph>(*peFile);
        graph->build();

        // Only replace the displayed file once the new one has loaded cleanly.
        ui.dependencyTree->clear();
        m_dependencyGraph.reset();
        m_peFile.reset();
        m_dataSource.reset();

        m_dataSource = std::move(dataSource);
        m_peFile = std::move(peFile);
        m_dependencyGraph = std::move(graph);
        ui.dependencyTree->setGraph(m_dependencyGraph.get());
        ui.statusBar->showMessage(tr("Loaded %1: %2 modules")
                                      .arg(QFileInfo(fileName).fileName())
                                      .arg(static_cast<qulonglong>(m_dependencyGraph->moduleCount())));
    } catch (const std::exception& error) {
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
