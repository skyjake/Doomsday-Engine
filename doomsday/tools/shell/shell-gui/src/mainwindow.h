#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <de/String>

class MainWindow : public QMainWindow
{
    Q_OBJECT
    
public:
    MainWindow(QWidget *parent = 0);
    ~MainWindow();

    void setTitle(QString const &title);

    bool isConnected() const;
    void closeEvent(QCloseEvent *);

public slots:
    void openConnection(QString address);
    void closeConnection();

protected slots:
    void handleIncomingPackets();
    void sendCommandToServer(de::String command);
    void disconnected();

private:
    struct Instance;
    Instance *d;
};

#endif // MAINWINDOW_H
