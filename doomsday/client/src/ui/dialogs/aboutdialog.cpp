/** @file aboutdialog.cpp Information about the Doomsday Client.
 *
 * @authors Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small> 
 */

#include "ui/dialogs/aboutdialog.h"
#include "ui/framework/sequentiallayout.h"
#include "ui/framework/signalaction.h"
#include "ui/widgets/labelwidget.h"
#include "ui/widgets/popupwidget.h"
#include "ui/widgets/documentwidget.h"
#include "ui/style.h"
#include "gl/sys_opengl.h"
#include "audio/audiodriver.h"
#include "clientapp.h"
#include "versioninfo.h"

#include "dd_def.h"

#include <de/Version>

using namespace de;

DENG2_PIMPL(AboutDialog)
{
    PopupWidget *glPopup;
    PopupWidget *audioPopup;

    Instance(Public *i) : Base(i)
    {
        // Popup with GL info.
        glPopup = new PopupWidget;
        glPopup->useInfoStyle();
        DocumentWidget *doc = new DocumentWidget;
        doc->setText(Sys_GLDescription());
        glPopup->setContent(doc);
        self.add(glPopup);

        // Popup with audio info.
        audioPopup = new PopupWidget;
        audioPopup->useInfoStyle();
        doc = new DocumentWidget;
        doc->setText(AudioDriver_InterfaceDescription());
        audioPopup->setContent(doc);
        self.add(audioPopup);
    }
};

AboutDialog::AboutDialog() : DialogWidget("about"), d(new Instance(this))
{
    /*
     * Construct the widgets.
     */
    LabelWidget *logo = new LabelWidget;
    logo->setImage(style().images().image("logo.px256"));
    logo->setSizePolicy(ui::Fixed, ui::Expand);

    // Set up the contents of the widget.
    LabelWidget *title = new LabelWidget;
    title->margins().set("");
    title->setFont("title");
    title->setText(DOOMSDAY_NICENAME);
    title->setSizePolicy(ui::Fixed, ui::Expand);

    VersionInfo version;
    de::Version ver2;

    LabelWidget *info = new LabelWidget;
    String txt = String(_E(D)_E(b) "%3 %1" _E(.) " #%2\n" _E(.) "%4-bit %5%6\n\n%7")
            .arg(version.base())
            .arg(ver2.build)
            .arg(DOOMSDAY_RELEASE_TYPE)
            .arg(ver2.cpuBits())
            .arg(ver2.operatingSystem())
            .arg(ver2.isDebugBuild()? " debug" : "")
            .arg(__DATE__ " " __TIME__);
    info->setText(txt);
    info->setSizePolicy(ui::Fixed, ui::Expand);

    ButtonWidget *homepage = new ButtonWidget;
    homepage->setText(tr("Go to Homepage"));
    homepage->setAction(new SignalAction(&ClientApp::app(), SLOT(openHomepageInBrowser())));

    area().add(logo);
    area().add(title);
    area().add(info);
    area().add(homepage);

    // Layout.
    RuleRectangle const &cont = area().contentRule();
    SequentialLayout layout(cont.left(), cont.top());
    layout.setOverrideWidth(style().rules().rule("dialog.about.width"));
    layout << *logo << *title << *info;

    // Center the button.
    homepage->rule()
            .setInput(Rule::AnchorX, cont.left() + layout.width() / 2)
            .setInput(Rule::Top, info->rule().bottom())
            .setAnchorPoint(Vector2f(.5f, 0));

    // Total size of the dialog's content.
    area().setContentSize(layout.width(), layout.height() + homepage->rule().height());

    buttons().items()
            << new DialogButtonItem(DialogWidget::Accept | DialogWidget::Default, tr("Close"))
            << new DialogButtonItem(DialogWidget::Action, tr("GL"), new SignalAction(this, SLOT(showGLInfo())))
            << new DialogButtonItem(DialogWidget::Action, tr("Audio"), new SignalAction(this, SLOT(showAudioInfo())));

    // The popups are anchored to their button.
    d->glPopup->setAnchorAndOpeningDirection(buttons().organizer().itemWidget(tr("GL"))->rule(), ui::Up);
    d->audioPopup->setAnchorAndOpeningDirection(buttons().organizer().itemWidget(tr("Audio"))->rule(), ui::Up);
}

void AboutDialog::showGLInfo()
{
    d->glPopup->open();
}

void AboutDialog::showAudioInfo()
{
    d->audioPopup->open();
}
