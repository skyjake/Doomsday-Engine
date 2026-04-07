#ifndef OPENDIALOG_H
#define OPENDIALOG_H

#include <de/dialogwidget.h>
#include <de/address.h>

/**
 * Dialog for specifying the server connection to open.
 */
class OpenDialog : public de::DialogWidget
{
public:
    explicit OpenDialog();

    de::String address() const;

public:
    void updateLocalList(bool autoselect = false);

protected:
    void saveState();
    void textEdited(const de::String &);
    void prepare() override;

private:
    DE_PRIVATE(d)
};

#endif // OPENDIALOG_H
