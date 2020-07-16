#include "aboutdialog.h"
#include "guishellapp.h"

#include <de/labelwidget.h>
#include <de/charsymbols.h>

using namespace de;

AboutDialog::AboutDialog()
{
    title().setText("Doomsday Shell " SHELL_VERSION);
    message().hide();

    buttons() << new ButtonItem(Default | Accept, "Close");

    auto *logo = new LabelWidget;
    logo->setImage(GuiShellApp::imageBank().image("logo"));
    logo->setSizePolicy(ui::Fixed, ui::Expand);
    logo->setImageFit(ui::FitToSize);
    area().add(logo);
                 
    auto *text = LabelWidget::newWithText("Copyright " DE_CHAR_COPYRIGHT
                                          " 2013-2020 Jaakko Keränen et al.\n\n"
                                          "The Shell is a utility for controlling and monitoring "
                                          "Doomsday servers.",
                                          &area());
    text->setSizePolicy(ui::Fixed, ui::Expand);
    text->setTextLineAlignment(ui::AlignLeft);
    text->setAlignment(ui::AlignLeft);
    text->setMaximumTextWidth(text->rule().width());

    updateLayout();
}
