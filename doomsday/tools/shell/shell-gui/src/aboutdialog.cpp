#include "aboutdialog.h"
#include "guishellapp.h"
#include <QLabel>
#include <QDialogButtonBox>
#include <QVBoxLayout>
#include <QPushButton>

AboutDialog::AboutDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("About Doomsday Shell"));
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

    QVBoxLayout *box = new QVBoxLayout;
    setLayout(box);
    box->setSizeConstraint(QLayout::SetFixedSize);

    QImage logo(":/images/shell.png");

    QLabel *img = new QLabel;
    img->setPixmap(QPixmap::fromImage(logo));
    box->addWidget(img, 0, Qt::AlignHCenter);

    QLabel *txt = new QLabel;
    box->addWidget(txt);
    txt->setMaximumWidth(logo.width() * 1.5f);
    txt->setTextFormat(Qt::RichText);
    txt->setWordWrap(true);
    txt->setText(tr("<b><big>Doomsday Shell %1</big></b><p>Copyright &copy; %2<p>"
                    "The Shell is a utility for controlling and monitoring "
                    "Doomsday servers.")
                 .arg(SHELL_VERSION)
                 .arg("2013 <a href=\"http://dengine.net/\">Deng Team</a>"));

    connect(txt, SIGNAL(linkActivated(QString)), &GuiShellApp::app(), SLOT(openWebAddress(QString)));

    QDialogButtonBox *bbox = new QDialogButtonBox;
    box->addWidget(bbox);
    QPushButton *button = bbox->addButton(QDialogButtonBox::Close);
    connect(button, SIGNAL(clicked()), this, SLOT(accept()));
}
