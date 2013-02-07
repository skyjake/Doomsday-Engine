#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

class MainWindow : public QMainWindow
{
    Q_OBJECT
    
public:
    MainWindow(QWidget *parent = 0);
    ~MainWindow();

    void setTitle(QString const &title);

    bool isConnected() const;
    void closeEvent(QCloseEvent *);

private:
    struct Instance;
    Instance *d;
};

#endif // MAINWINDOW_H
