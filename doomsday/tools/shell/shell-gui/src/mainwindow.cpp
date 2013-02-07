#include "mainwindow.h"
#include "qtrootwidget.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    QtRootWidget *w = new QtRootWidget;
    w->setFont(QFont("Courier", 16));
    setCentralWidget(w);

    w->QWidget::setFocus();
}

MainWindow::~MainWindow()
{
}
