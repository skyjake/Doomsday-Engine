#include "errorlogdialog.h"
#include "preferences.h"
#include <QLabel>
#include <QDialogButtonBox>
#include <QTextEdit>
#include <QVBoxLayout>

using namespace de;

DENG2_PIMPL(ErrorLogDialog)
{
    QLabel *msg;
    QTextEdit *text;

    Instance(Public *i) : Base(i)
    {
        QVBoxLayout *layout = new QVBoxLayout;

        msg = new QLabel;
        layout->addWidget(msg, 0);

        text = new QTextEdit;
        text->setReadOnly(true);
        text->setFont(Preferences::consoleFont());
        QFontMetrics metrics(text->font());
        text->setMinimumWidth(metrics.width("A") * 90);
        text->setMinimumHeight(metrics.height() * 15);
        layout->addWidget(text, 1);

        QDialogButtonBox *buttons = new QDialogButtonBox;
        buttons->addButton(QDialogButtonBox::Ok);
        QObject::connect(buttons, SIGNAL(accepted()), thisPublic, SLOT(accept()));
        layout->addWidget(buttons, 0);

        self.setLayout(layout);
    }
};

ErrorLogDialog::ErrorLogDialog(QWidget *parent) : QDialog(parent),
    d(new Instance(this))
{
    setWindowTitle(tr("Error Log"));
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
}

void ErrorLogDialog::setMessage(QString const &message)
{
    d->msg->setText(message);
}

void ErrorLogDialog::setLogContent(QString const &text)
{
    d->text->setText(text);
}
