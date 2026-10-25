#include "PeViewer.h"

#include <QActionGroup>
#include <QEvent>

#include "LanguageManager.h"

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

PeViewer::~PeViewer() = default;

void PeViewer::changeEvent(QEvent* event) {
    QMainWindow::changeEvent(event);
    if (event->type() == QEvent::LanguageChange) retranslateUi();
}

void PeViewer::retranslateUi() {
    ui.retranslateUi(this);
    ui.statusBar->showMessage(tr("Ready"));
}

void PeViewer::updateLanguageActions() {
    ui.actionEnglish->setChecked(m_languageManager.language() ==
                                 QStringLiteral("en"));
    ui.actionSimplifiedChinese->setChecked(m_languageManager.language() ==
                                           QStringLiteral("zh_CN"));
}
