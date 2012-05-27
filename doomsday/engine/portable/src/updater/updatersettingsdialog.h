#ifndef LIBDENG_UPDATERSETTINGSDIALOG_H
#define LIBDENG_UPDATERSETTINGSDIALOG_H

#include <QDialog>

class UpdaterSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit UpdaterSettingsDialog(QWidget *parent = 0);
    ~UpdaterSettingsDialog();
    
signals:
    
public slots:
    void neverCheckToggled(bool);
    void pathActivated(int index);
    
private:
    struct Instance;
    Instance* d;
};

#endif // LIBDENG_UPDATERSETTINGSDIALOG_H
