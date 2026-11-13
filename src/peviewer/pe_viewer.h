#pragma once

#include <SARibbonMainWindow.h>

#include "ui_pe_viewer.h"

class QActionGroup;
class LanguageManager;
class QMenu;
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

private Q_SLOTS:
    void on_actionOpen_triggered(bool b);
    void on_actionClose_triggered();

private:
    void setupRibbon();
    void updateLanguageActions();
    void addWorkspaceAction(Workspace* workspace);
    void removeWorkspaceAction(Workspace* workspace);
    void updateCurrentWorkspace(Workspace* workspace);
    void updateWorkspaceStatus();
    void retranslateUi();

    LanguageManager& m_languageManager;
    QActionGroup* m_workspaceActions = nullptr;
    QMenu* m_workspaceMenu = nullptr;
    SARibbonCategory* m_homeCategory = nullptr;
    SARibbonPanel* m_filePanel = nullptr;
    SARibbonPanel* m_workspacePanel = nullptr;
    SARibbonPanel* m_languagePanel = nullptr;
    Ui::PeViewerClass ui;
};
