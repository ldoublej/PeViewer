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
#include <QPushButton>
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
#include "document.h"
#include "document_manager.h"
#include "editor_host.h"
#include "language_manager.h"

PeViewer::PeViewer(LanguageManager& languageManager, QWidget* parent)
    : SARibbonMainWindow(parent), m_languageManager(languageManager) {
    ui.setupUi(this);
    setupRibbon();
    m_editorHost = new EditorHost(ribbonBar(), ui.viewStack, ui.homePage, this);
    connect(ui.homeOpenButton, &QPushButton::clicked, ui.actionOpen, &QAction::trigger);

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

    auto* manager = DocumentManager::instance();
    connect(manager, &DocumentManager::documentOpened, this, &PeViewer::addDocument);
    connect(manager, &DocumentManager::documentClosed, this, &PeViewer::removeDocument);
    connect(manager, &DocumentManager::documentActivated, this, &PeViewer::updateCurrentDocument);
    for (const auto& document : manager->documents()) {
        addDocument(document.get());
    }
    updateCurrentDocument(manager->current());
    retranslateUi();
    updateLanguageActions();
}

PeViewer::~PeViewer() {
    // Keep the view pages alive: documents outlive the window and rebuild them later.
    if (m_editorHost) {
        m_editorHost->detachViews();
    }
}

void PeViewer::setupRibbon() {
    auto* ribbon = ribbonBar();
    ribbon->setObjectName(QStringLiteral("ribbonBar"));
    ribbon->setRibbonStyle(SARibbonBar::RibbonStyleLooseTwoRow);
    setupFileMenu();

    m_homeCategory = ribbon->addCategoryPage(tr("Home"));
    m_homeCategory->setObjectName(QStringLiteral("ribbonHome"));

    m_documentCombo = new QComboBox(ribbon);
    m_documentCombo->setObjectName(QStringLiteral("documentCombo"));
    m_documentCombo->setEditable(true);
    m_documentCombo->setInsertPolicy(QComboBox::NoInsert);
    m_documentCombo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    m_documentCombo->setFixedWidth(320);
    m_documentCombo->setMaxVisibleItems(12);
    auto* completer = m_documentCombo->completer();
    completer->setCaseSensitivity(Qt::CaseInsensitive);
    completer->setFilterMode(Qt::MatchContains);
    completer->setCompletionMode(QCompleter::PopupCompletion);
    completer->setMaxVisibleItems(12);
    completer->popup()->installEventFilter(this);
    ribbon->quickAccessBar()->addWidget(m_documentCombo);
    setFramelessHitTestVisible(m_documentCombo);
    setFramelessHitTestVisible(m_documentCombo->lineEdit());
    m_documentCombo->installEventFilter(this);
    m_documentCombo->lineEdit()->installEventFilter(this);
    connect(m_documentCombo, QOverload<int>::of(&QComboBox::activated), this, [this](int index) {
        auto* manager = DocumentManager::instance();
        const QString path = m_documentCombo->itemData(index).toString();
        if (!path.isEmpty()) {
            manager->activate(manager->find(path));
        }
        updateDocumentSelection();
    });
    connect(m_documentCombo->lineEdit(), &QLineEdit::editingFinished, this, [this] {
        // Let completion activation finish before restoring a cancelled or invalid query.
        QTimer::singleShot(0, this, &PeViewer::updateDocumentSelection);
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

    auto* manager = DocumentManager::instance();
    try {
        manager->open(fileName);
    } catch (const std::exception& error) {
        QMessageBox::warning(this, tr("Open Failed"),
                             tr("Could not load %1:\n%2")
                                 .arg(fileName, QString::fromUtf8(error.what())));
    }
}

void PeViewer::on_actionClose_triggered() {
    auto* manager = DocumentManager::instance();
    manager->close(manager->current());
}

void PeViewer::changeEvent(QEvent* event) {
    SARibbonMainWindow::changeEvent(event);
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
    }
}

bool PeViewer::eventFilter(QObject* watched, QEvent* event) {
    if (m_documentCombo &&
        (watched == m_documentCombo || watched == m_documentCombo->lineEdit() ||
         watched == m_documentCombo->completer()->popup())) {
        if (event->type() == QEvent::KeyPress &&
            static_cast<QKeyEvent*>(event)->key() == Qt::Key_Escape) {
            m_documentCombo->hidePopup();
            updateDocumentSelection();
            return true;
        }
        if (event->type() == QEvent::FocusIn &&
            watched != m_documentCombo->completer()->popup()) {
            QTimer::singleShot(0, m_documentCombo->lineEdit(), &QLineEdit::selectAll);
        }
        if (event->type() == QEvent::FocusOut ||
            (watched == m_documentCombo->completer()->popup() && event->type() == QEvent::Hide)) {
            QTimer::singleShot(0, this, [this] {
                if (!m_documentCombo->hasFocus() && !m_documentCombo->lineEdit()->hasFocus() &&
                    !m_documentCombo->completer()->popup()->isVisible()) {
                    updateDocumentSelection();
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
    m_documentCombo->setAccessibleName(tr("Document"));
    m_documentCombo->lineEdit()->setPlaceholderText(tr("Search documents..."));
    m_languagePanel->setPanelName(tr("Language"));
    if (m_editorHost) {
        m_editorHost->retranslateUi();
    }
    updateDocumentStatus();
    updateDocumentSelection();
}

void PeViewer::addDocument(Document* document) {
    const QString path = document->path();
    const QString displayPath = QDir::toNativeSeparators(path);
    const QSignalBlocker blocker(m_documentCombo);
    m_documentCombo->addItem(displayPath, path);
    m_documentCombo->setItemData(m_documentCombo->count() - 1, displayPath, Qt::ToolTipRole);
    updateDocumentSelection();
}

void PeViewer::removeDocument(Document* document) {
    const QSignalBlocker blocker(m_documentCombo);
    m_documentCombo->removeItem(m_documentCombo->findData(document->path()));
    updateDocumentSelection();
}

void PeViewer::updateDocumentSelection() {
    const QSignalBlocker blocker(m_documentCombo);
    auto* current = DocumentManager::instance()->current();
    const int index = current ? m_documentCombo->findData(current->path()) : -1;
    m_documentCombo->completer()->popup()->hide();
    m_documentCombo->setCurrentIndex(index);
    // setCurrentIndex does not restore edited text when the index is unchanged.
    m_documentCombo->setEditText(index >= 0 ? m_documentCombo->itemText(index) : QString());
    m_documentCombo->setToolTip(current ? QDir::toNativeSeparators(current->path())
                                        : tr("Search documents..."));
    m_documentCombo->setEnabled(m_documentCombo->count() > 0);
}

void PeViewer::updateCurrentDocument(Document* document) {
    ui.actionClose->setEnabled(document != nullptr);
    updateDocumentSelection();
    updateDocumentStatus();
}

void PeViewer::updateDocumentStatus() {
    if (auto* document = DocumentManager::instance()->current()) {
        ui.statusBar->showMessage(
            tr("Loaded %1: %2 modules")
                .arg(QFileInfo(document->path()).fileName())
                .arg(static_cast<qulonglong>(document->graph().moduleCount())));
    } else {
        ui.statusBar->showMessage(tr("Ready"));
    }
}

void PeViewer::updateLanguageActions() {
    ui.actionEnglish->setChecked(m_languageManager.language() == QStringLiteral("en"));
    ui.actionSimplifiedChinese->setChecked(m_languageManager.language() == QStringLiteral("zh_CN"));
}
