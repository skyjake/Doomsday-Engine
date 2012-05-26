#ifndef LIBDENG_UPDATEAVAILABLEDIALOG_H
#define LIBDENG_UPDATEAVAILABLEDIALOG_H

#include "versioninfo.h"
#include <QDialog>

class UpdateAvailableDialog : public QDialog
{
    Q_OBJECT
    
public:
    explicit UpdateAvailableDialog(const VersionInfo& latestVersion, QWidget *parent = 0);
    ~UpdateAvailableDialog();
    
public slots:
    void neverCheckToggled(bool);

private:
    struct Instance;
    Instance* d;
};

#endif // LIBDENG_UPDATEAVAILABLEDIALOG_H
