#ifndef LIBDENG_UPDATEAVAILABLEDIALOG_H
#define LIBDENG_UPDATEAVAILABLEDIALOG_H

#include <QDialog>

class UpdateAvailableDialog : public QDialog
{
    Q_OBJECT
    
public:
    explicit UpdateAvailableDialog(QWidget *parent = 0);
    ~UpdateAvailableDialog();
    
private:
    struct Instance;
    Instance* d;
};

#endif // LIBDENG_UPDATEAVAILABLEDIALOG_H
