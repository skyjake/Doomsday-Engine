/** @file networksettingsdialog.cpp  Dialog for network settings.
 *
 * @authors Copyright © 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2015 Daniel Swanson <danij@dengine.net>
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

#include "ui/dialogs/networksettingsdialog.h"
#include "ui/widgets/cvarsliderwidget.h"
#include "ui/widgets/cvartogglewidget.h"
#include "ui/widgets/cvarchoicewidget.h"

#include "ConfigProfiles"
#include "clientapp.h"

#include <de/SignalAction>
#include <de/GridPopupWidget>
#include <de/VariableLineEditWidget>
#include <de/VariableToggleWidget>

using namespace de;
using namespace de::ui;

DE_GUI_PIMPL(NetworkSettingsDialog)
{
    VariableToggleWidget *localPackages;
    VariableLineEditWidget *webApiUrl;
    GridPopupWidget *devPopup;
    CVarToggleWidget *devInfo;

    Impl(Public *i) : Base(i)
    {
        ScrollAreaWidget &area = self().area();

        area.add(localPackages = new VariableToggleWidget(tr("Local Multiplayer Mods"),
                                                          App::config("resource.localPackages")));

        // Developer options.
        self().add(devPopup = new GridPopupWidget);
        *devPopup << LabelWidget::newWithText(tr("Web API:"))
                  << (webApiUrl = new VariableLineEditWidget(App::config("apiUrl")))
                  << Const(0)
                  << (devInfo = new CVarToggleWidget("net-dev"));
        devPopup->commit();
    }

    void fetch()
    {
        for (GuiWidget *w : self().area().childWidgets() + devPopup->content().childWidgets())
        {
            if (ICVarWidget *cv = maybeAs<ICVarWidget>(w))
            {
                cv->updateFromCVar();
            }
        }
    }
};

NetworkSettingsDialog::NetworkSettingsDialog(String const &name)
    : DialogWidget(name, WithHeading), d(new Impl(this))
{
    heading().setText(tr("Network Settings"));
    heading().setStyleImage("network");

    d->devInfo->setText(tr("Developer Info"));

    // Layout.
    GridLayout layout(area().contentRule().left(), area().contentRule().top());
    layout.setGridSize(1, 0);
    //layout.setColumnAlignment(0, ui::AlignRight);
    layout << *d->localPackages;

    auto *caution = LabelWidget::newWithText(
                tr("Caution: Loading additional add-ons or mods may cause gameplay bugs "
                   "or client instability in multiplayer games."), &area());
    caution->margins().setTop("");
    caution->setTextLineAlignment(ui::AlignLeft);
    caution->setMaximumTextWidth(area().rule().width() - area().margins().width());
    caution->setFont("separator.annotation");
    caution->setTextColor("altaccent");
    layout << *caution;

    area().setContentSize(layout);

    buttons()
            << new DialogButtonItem(Default | Accept, tr("Close"))
            << new DialogButtonItem(Action, tr("Reset to Defaults"),
                                    new SignalAction(this, SLOT(resetToDefaults())))
            << new DialogButtonItem(ActionPopup | Id1, style().images().image("gauge"));

    popupButtonWidget(Id1)->setPopup(*d->devPopup);

    d->fetch();
}

void NetworkSettingsDialog::resetToDefaults()
{
    ClientApp::networkSettings().resetToDefaults();

    d->fetch();
}
