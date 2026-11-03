#pragma once

#include <QtWidgets/QMainWindow>
#include <memory>

#include "ui_pe_viewer.h"

class DependencyGraph;
class LanguageManager;
class PeFile;
class PeFileDataSource;

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
    // Destruction order matters: the graph borrows the parsed PE file, which
    // borrows the data source.
    std::unique_ptr<PeFileDataSource> m_dataSource;
    std::unique_ptr<PeFile> m_peFile;
    std::unique_ptr<DependencyGraph> m_dependencyGraph;
    Ui::PeViewerClass ui;
};
