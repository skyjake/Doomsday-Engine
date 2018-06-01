/** @file aboutdialog.cpp Information about the Doomsday Client.
 *
 * @authors Copyright (c) 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "dd_main.h"
#include "dd_def.h"

#include <de/Version>
#include <de/SequentialLayout>
#include <de/SignalAction>
#include <de/LabelWidget>
#include <de/DocumentPopupWidget>
#include <de/Style>

using namespace de;

DE_PIMPL(AboutDialog)
{
    DocumentPopupWidget *glPopup;
    DocumentPopupWidget *audioPopup;

    Impl(Public *i) : Base(i)
    {
        // Popup with GL info.
        glPopup = new DocumentPopupWidget;
        glPopup->document().setText(Sys_GLDescription());
        self().add(glPopup);

        // Popup with audio info.
        audioPopup = new DocumentPopupWidget;
        audioPopup->document().setText(App_AudioSystem().description());
        self().add(audioPopup);
    }
};

AboutDialog::AboutDialog() : DialogWidget("about"), d(new Impl(this))
{
    /*
     * Construct the widgets.
     */
    LabelWidget *logo = new LabelWidget;
    logo->setImage(style().images().image("logo.px256"));
    logo->setSizePolicy(ui::Fixed, ui::Expand);

    de::Version const version = de::Version::currentBuild();

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
    String txt = String(_E(b) "%4 build %5" _E(.) "\n%6%8\n\n%1 (%2-%7)%3")
            .arg(version.operatingSystem() == "windows"? tr("Windows") :
                 version.operatingSystem() == "macx"? tr("macOS") :
                 version.operatingSystem() == "ios"? tr("iOS") : tr("Unix"))
            .arg(version.cpuBits())
            .arg(version.isDebugBuild()? tr(" Debug") : "")
            .arg(DOOMSDAY_RELEASE_TYPE)
            //.arg(version.compactNumber())
            .arg(version.build)
            .arg(Time::fromText(__DATE__ " " __TIME__, Time::CompilerDateTime)
                 .asDateTime().toString(Qt::SystemLocaleShortDate))
            .arg(tr("bit"))
            .arg(version.gitDescription.isEmpty()? "" : ("\n" _E(s)_E(F) + version.gitDescription + _E(.)_E(.)));
    info->setText(txt);
    info->setSizePolicy(ui::Fixed, ui::Expand);

    area().add(logo);
    area().add(title);
    area().add(info);

    // Layout.
    RuleRectangle const &cont = area().contentRule();
    SequentialLayout layout(cont.left(), cont.top());
    layout.setOverrideWidth(rule("dialog.about.width"));
    layout << *logo << *title << *info;

    // Total size of the dialog's content.
    area().setContentSize(layout);

    buttons()
            << new DialogButtonItem(DialogWidget::Accept | DialogWidget::Default, tr("Close"))
            << new DialogButtonItem(DialogWidget::ActionPopup, tr("GL"))
            << new DialogButtonItem(DialogWidget::ActionPopup, tr("Audio"))
            << new DialogButtonItem(DialogWidget::Action, tr("Homepage..."),
                                    new SignalAction(&ClientApp::app(), SLOT(openHomepageInBrowser())));

    // The popups are anchored to their button.
    popupButtonWidget(tr("GL")).setPopup(*d->glPopup);
    popupButtonWidget(tr("Audio")).setPopup(*d->audioPopup);
}
