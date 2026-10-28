#pragma once

#include <QtWidgets/QMainWindow>

#include "ui_pe_viewer.h"

class LanguageManager;

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
    void retranslateUi();

    LanguageManager& m_languageManager;
    Ui::PeViewerClass ui;
};
