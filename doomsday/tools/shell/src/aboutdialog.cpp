#include "aboutdialog.h"
#include "guishellapp.h"
//#include "utils.h"
//#include <QLabel>
//#include <QDialogButtonBox>
//#include <QVBoxLayout>
//#include <QPushButton>
#include <de/LabelWidget>

using namespace de;

AboutDialog::AboutDialog()
{
//    AutoRef<Rule> width = rule("unit") * 100;

    title().setText("Doomsday Shell " SHELL_VERSION);
    message().hide();

    buttons() << new ButtonItem(Default | Accept, "Close");

    auto *logo = new LabelWidget;
    logo->setImage(GuiShellApp::imageBank().image("logo"));
    //logo->rule().setInput(Rule::Height, logo->rule().width());
    logo->setSizePolicy(ui::Fixed, ui::Expand);
    logo->setImageFit(ui::FitToSize);
    area().add(logo);

    auto *text = LabelWidget::newWithText("Copyright © 2013-2019 Jaakko Keränen et al.\n\n"
                                          "The Shell is a utility for controlling and monitoring "
                                          "Doomsday servers.",
                                          &area());
    text->setSizePolicy(ui::Fixed, ui::Expand);
    text->setTextLineAlignment(ui::AlignLeft);
    text->setAlignment(ui::AlignLeft);
    text->setMaximumTextWidth(text->rule().width());

    updateLayout();

//    setLayoutWidth(width);


//    QVBoxLayout *box = new QVBoxLayout;
//    setLayout(box);
//    box->setSizeConstraint(QLayout::SetFixedSize);

//    QImage logo(imageResourcePath(":/images/shell.png"));

//    QLabel *img = new QLabel;
//    img->setPixmap(QPixmap::fromImage(logo));
//    box->addWidget(img, 0, Qt::AlignHCenter);

//    QLabel *txt = new QLabel;
//    box->addWidget(txt);
//    txt->setMaximumWidth(logo.width() * 1.5f);
//    txt->setTextFormat(Qt::RichText);
//    txt->setWordWrap(true);
//    txt->setText(tr("<b><big>Doomsday Shell %1</big></b><p>Copyright &copy; %2<p>"
//                    "The Shell is a utility for controlling and monitoring "
//                    "Doomsday servers.")
//                 .arg(SHELL_VERSION)
//                 .arg("2013-2018 <a href=\"http://dengine.net/\">Deng Team</a>"));

//    connect(txt, SIGNAL(linkActivated(QString)), &GuiShellApp::app(), SLOT(openWebAddress(QString)));

//    QDialogButtonBox *bbox = new QDialogButtonBox;
//    box->addWidget(bbox);
//    QPushButton *button = bbox->addButton(QDialogButtonBox::Close);
//    connect(button, SIGNAL(clicked()), this, SLOT(accept()));
}
