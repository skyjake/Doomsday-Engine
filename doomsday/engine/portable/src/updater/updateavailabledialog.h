#ifndef LIBDENG_UPDATEAVAILABLEDIALOG_H
#define LIBDENG_UPDATEAVAILABLEDIALOG_H

#include <QDialog>
#include "versioninfo.h"

class UpdateAvailableDialog : public QDialog
{
    Q_OBJECT
    
public:
    /// The dialog is initialized with the "Checking" page visible.
    explicit UpdateAvailableDialog(QWidget *parent = 0);

    /// The dialog is initialized with the result page visible.
    explicit UpdateAvailableDialog(const VersionInfo& latestVersion,
                                   de::String changeLogUri, QWidget *parent = 0);
    ~UpdateAvailableDialog();
    
public slots:
    void showResult(const VersionInfo& latestVersion, de::String changeLogUri);
    void neverCheckToggled(bool);
    void showWhatsNew();
    void editSettings();

signals:
    void checkAgain();

private:
    struct Instance;
    Instance* d;
};

#endif // LIBDENG_UPDATEAVAILABLEDIALOG_H
