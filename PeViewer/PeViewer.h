#pragma once

#include <QtWidgets/QMainWindow>

#include "ui_PeViewer.h"

class LanguageManager;

class PeViewer : public QMainWindow {
    Q_OBJECT

   public:
    explicit PeViewer(LanguageManager& languageManager,
                      QWidget* parent = nullptr);
    ~PeViewer() override;

   protected:
    void changeEvent(QEvent* event) override;

   private:
    void updateLanguageActions();
    void retranslateUi();

    LanguageManager& m_languageManager;
    Ui::PeViewerClass ui;
};
