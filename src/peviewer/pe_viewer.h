#pragma once

#include <QtWidgets/QMainWindow>

#include "ui_pe_viewer.h"

class QActionGroup;
class LanguageManager;
class Workspace;

class PeViewer : public QMainWindow {
    Q_OBJECT

public:
    explicit PeViewer(LanguageManager& languageManager, QWidget* parent = nullptr);
    ~PeViewer() override;

protected:
    void changeEvent(QEvent* event) override;

private Q_SLOTS:
    void on_actionOpen_triggered(bool b);

private:
    void updateLanguageActions();
    void addWorkspaceAction(Workspace* workspace);
    void removeWorkspaceAction(Workspace* workspace);
    void updateCurrentWorkspace(Workspace* workspace);
    void updateWorkspaceStatus();
    void retranslateUi();

    LanguageManager& m_languageManager;
    QActionGroup* m_workspaceActions = nullptr;
    Ui::PeViewerClass ui;
};
