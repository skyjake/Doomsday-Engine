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
#include "gl/sys_opengl.h"
#include "audio/audiodriver.h"
#include "clientapp.h"
#include "versioninfo.h"

#include "dd_def.h"

#include <de/Version>
#include <de/SequentialLayout>
#include <de/SignalAction>
#include <de/LabelWidget>
#include <de/PopupWidget>
#include <de/DocumentWidget>
#include <de/Style>

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

    VersionInfo version;
    de::Version ver2;

    // Set up the contents of the widget.
    LabelWidget *title = LabelWidget::newWithText(String("%1 %2.%3")
                                                  .arg(DOOMSDAY_NICENAME)
                                                  .arg(version.major)
                                                  .arg(version.minor));
    title->margins().set("");
    title->setFont("title");
    title->setTextColor("accent");
    title->setSizePolicy(ui::Fixed, ui::Expand);

    LabelWidget *info = new LabelWidget;
    String txt = String(_E(b) "%4 %5 #%6" _E(.) "\n%7\n\n%1 (%2-%8)%3")
            .arg(ver2.operatingSystem() == "windows"? tr("Windows") :
                 ver2.operatingSystem() == "macx"? tr("Mac OS X") : tr("Unix"))
            .arg(ver2.cpuBits())
            .arg(ver2.isDebugBuild()? tr(" Debug") : "")
            .arg(DOOMSDAY_RELEASE_TYPE)
            .arg(version.base())
            .arg(ver2.build)
            .arg(Time::fromText(__DATE__ " " __TIME__, Time::CompilerDateTime)
                 .asDateTime().toString(Qt::SystemLocaleShortDate))
            .arg(tr("bit"));
    info->setText(txt);
    info->setSizePolicy(ui::Fixed, ui::Expand);

    area().add(logo);
    area().add(title);
    area().add(info);

    // Layout.
    RuleRectangle const &cont = area().contentRule();
    SequentialLayout layout(cont.left(), cont.top());
    layout.setOverrideWidth(style().rules().rule("dialog.about.width"));
    layout << *logo << *title << *info;

    // Total size of the dialog's content.
    area().setContentSize(layout.width(), layout.height());

    buttons()
            << new DialogButtonItem(DialogWidget::Accept | DialogWidget::Default, tr("Close"))
            << new DialogButtonItem(DialogWidget::Action, tr("GL"), new SignalAction(this, SLOT(showGLInfo())))
            << new DialogButtonItem(DialogWidget::Action, tr("Audio"), new SignalAction(this, SLOT(showAudioInfo())))
            << new DialogButtonItem(DialogWidget::Action, tr("Homepage..."), new SignalAction(&ClientApp::app(), SLOT(openHomepageInBrowser())));

    // The popups are anchored to their button.
    d->glPopup->setAnchorAndOpeningDirection(buttonWidget(tr("GL")).rule(), ui::Up);
    d->audioPopup->setAnchorAndOpeningDirection(buttonWidget(tr("Audio")).rule(), ui::Up);
}

void AboutDialog::showGLInfo()
{
    d->glPopup->open();
}

void AboutDialog::showAudioInfo()
{
    d->audioPopup->open();
}
