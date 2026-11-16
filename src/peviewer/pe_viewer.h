#pragma once

#include <SARibbonMainWindow.h>

#include "ui_pe_viewer.h"

class LanguageManager;
class QComboBox;
class SARibbonCategory;
class SARibbonPanel;
class Workspace;

class PeViewer : public SARibbonMainWindow {
    Q_OBJECT

public:
    explicit PeViewer(LanguageManager& languageManager, QWidget* parent = nullptr);
    ~PeViewer() override;

protected:
    void changeEvent(QEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private Q_SLOTS:
    void on_actionOpen_triggered(bool b);
    void on_actionClose_triggered();

private:
    void setupRibbon();
    void updateLanguageActions();
    void addWorkspace(Workspace* workspace);
    void removeWorkspace(Workspace* workspace);
    void updateWorkspaceSelection();
    void updateCurrentWorkspace(Workspace* workspace);
    void updateWorkspaceStatus();
    void retranslateUi();

    LanguageManager& m_languageManager;
    QComboBox* m_workspaceCombo = nullptr;
    SARibbonCategory* m_homeCategory = nullptr;
    SARibbonPanel* m_filePanel = nullptr;
    SARibbonPanel* m_languagePanel = nullptr;
    Ui::PeViewerClass ui;
};
