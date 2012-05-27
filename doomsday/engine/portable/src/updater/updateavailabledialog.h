#ifndef LIBDENG_UPDATEAVAILABLEDIALOG_H
#define LIBDENG_UPDATEAVAILABLEDIALOG_H

#include <QDialog>
#include "versioninfo.h"

class UpdateAvailableDialog : public QDialog
{
    Q_OBJECT
    
public:
    explicit UpdateAvailableDialog(const VersionInfo& latestVersion,
                                   de::String changeLogUri, QWidget *parent = 0);
    ~UpdateAvailableDialog();
    
public slots:
    void neverCheckToggled(bool);
    void showWhatsNew();
    void editSettings();

private:
    struct Instance;
    Instance* d;
};

#endif // LIBDENG_UPDATEAVAILABLEDIALOG_H
