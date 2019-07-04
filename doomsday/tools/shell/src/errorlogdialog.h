#ifndef ERRORLOGDIALOG_H
#define ERRORLOGDIALOG_H

#include <de/MessageDialog>

class ErrorLogDialog : public de::MessageDialog
{
public:
    explicit ErrorLogDialog();

    void setMessage(const de::String &message);
    void setLogContent(const de::String &text);

private:
    DE_PRIVATE(d)
};

#endif // ERRORLOGDIALOG_H
