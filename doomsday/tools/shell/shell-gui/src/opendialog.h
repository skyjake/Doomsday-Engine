#ifndef OPENDIALOG_H
#define OPENDIALOG_H

#include <QDialog>
#include <de/Address>

/**
 * Dialog for specifying the server connection to open.
 */
class OpenDialog : public QDialog
{
    Q_OBJECT

public:
    explicit OpenDialog(QWidget *parent = 0);
    ~OpenDialog();

    QString address() const;

public slots:
    void updateLocalList(bool autoselect = false);

protected slots:
    void saveState();
    void textEdited(QString);

private:
    struct Instance;
    Instance *d;
};

#endif // OPENDIALOG_H
