#pragma once

#include <QtWidgets/QMainWindow>
#include "ui_PeViewer.h"

class PeViewer : public QMainWindow
{
    Q_OBJECT

public:
    PeViewer(QWidget *parent = nullptr);
    ~PeViewer();

private:
    Ui::PeViewerClass ui;
};

