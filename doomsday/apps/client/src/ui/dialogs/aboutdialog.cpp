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

#include <de/version.h>
#include <de/sequentiallayout.h>
#include <de/labelwidget.h>
#include <de/documentpopupwidget.h>
#include <de/ui/style.h>

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

    const de::Version version = de::Version::currentBuild();

    // Set up the contents of the widget.
    LabelWidget *title = LabelWidget::newWithText(
        Stringf("%s %i.%i", DOOMSDAY_NICENAME, version.major, version.minor));
    title->margins().set("");
    title->setFont("title");
    title->setTextColor("accent");
    title->setSizePolicy(ui::Fixed, ui::Expand);

    LabelWidget *info = new LabelWidget;

    String txt = Stringf(
        _E(b) DOOMSDAY_RELEASE_TYPE " %s #%d" _E(.) "\n" __DATE__ " " __TIME__ "%s\n\n%s %d-bit%s",
        version.compactNumber().c_str(),
        version.build,
        version.gitDescription.isEmpty()
            ? ""
            : ("\n" _E(s) _E(F) + version.gitDescription + _E(.) _E(.)).c_str(),
        version.operatingSystem()       == "windows" ? "Windows"
            : version.operatingSystem() == "macx"    ? "macOS"
            : version.operatingSystem() == "ios"     ? "iOS" : "Unix",
        version.cpuBits(),
        version.isDebugBuild() ? " (Debug)" : "");

    info->setText(txt);
    info->setSizePolicy(ui::Fixed, ui::Expand);

    area().add(logo);
    area().add(title);
    area().add(info);

    // Layout.
    const RuleRectangle &cont = area().contentRule();
    SequentialLayout layout(cont.left(), cont.top());
    layout.setOverrideWidth(rule("dialog.about.width"));
    layout << *logo << *title << *info;

    // Total size of the dialog's content.
    area().setContentSize(layout);

    buttons()
            << new DialogButtonItem(DialogWidget::Accept | DialogWidget::Default, "Close")
            << new DialogButtonItem(DialogWidget::ActionPopup, "GL")
            << new DialogButtonItem(DialogWidget::ActionPopup, "Audio")
            << new DialogButtonItem(DialogWidget::Action, "Homepage...",
                                    [](){ ClientApp::app().openHomepageInBrowser(); });

    // The popups are anchored to their button.
    popupButtonWidget("GL").setPopup(*d->glPopup);
    popupButtonWidget("Audio").setPopup(*d->audioPopup);
}
