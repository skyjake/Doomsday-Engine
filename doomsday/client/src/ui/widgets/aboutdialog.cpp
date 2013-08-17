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

#include "ui/widgets/aboutdialog.h"
#include "ui/widgets/labelwidget.h"
#include "ui/signalaction.h"
#include "ui/style.h"
#include "ui/signalaction.h"
#include "clientapp.h"
#include "../../updater/versioninfo.h"

#include "dd_def.h"

#include <de/Version>

using namespace de;

AboutDialog::AboutDialog() : DialogWidget("about")
{
    Rule const &width = style().rules().rule("about.width");

    LabelWidget *logo = new LabelWidget;
    logo->setImage(style().images().image("logo.px256"));
    logo->setSizePolicy(ui::Fixed, ui::Expand);

    // Set up the contents of the widget.
    LabelWidget *title = new LabelWidget;
    title->setMargin("");
    title->setFont("title");
    title->setText(DOOMSDAY_NICENAME);
    title->setSizePolicy(ui::Fixed, ui::Expand);

    VersionInfo version;
    de::Version ver2;

    LabelWidget *info = new LabelWidget;
    String txt = String(_E(D)_E(b) "%1" _E(.) " #%2 %3\n" _E(.)_E(l) "%4-bit %5%6\n\n%7")
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
    homepage->setSizePolicy(ui::Expand, ui::Expand);
    homepage->setAction(new SignalAction(&ClientApp::app(), SLOT(openHomepageInBrowser())));

    content().add(logo);
    content().add(title);
    content().add(info);
    content().add(homepage);

    // Position inside the content.
    RuleRectangle const &cont = content().contentRule();
    logo->rule()
            .setLeftTop(cont.left(), cont.top())
            .setInput(Rule::Width, width);
    title->rule()
            .setLeftTop(cont.left(), logo->rule().bottom())
            .setInput(Rule::Width, width);
    info->rule()
            .setLeftTop(cont.left(), title->rule().bottom())
            .setInput(Rule::Width, width);
    homepage->rule()
            .setInput(Rule::AnchorX, cont.left() + width / 2)
            .setInput(Rule::Top, info->rule().bottom())
            .setAnchorPoint(Vector2f(.5f, 0));

    // Total size of the dialog's content.
    content().setContentWidth(width);
    content().setContentHeight(logo->rule().height() + title->rule().height() +
                               info->rule().height() + homepage->rule().height());

    // Just an OK button.
    buttons().items()
            << new ui::ActionItem(tr("Close"), new SignalAction(this, SLOT(accept())));
}
