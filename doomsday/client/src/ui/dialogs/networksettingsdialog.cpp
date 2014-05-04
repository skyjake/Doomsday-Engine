/** @file networksettingsdialog.cpp  Dialog for network settings.
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

#include "ui/dialogs/networksettingsdialog.h"
#include "ui/widgets/cvarsliderwidget.h"
#include "ui/widgets/cvartogglewidget.h"
#include "ui/widgets/cvarchoicewidget.h"

#include "clientapp.h"
#include "de_audio.h"

#include <de/SignalAction>
#include <de/GridPopupWidget>
#include <de/VariableLineEditWidget>

using namespace de;
using namespace ui;

DENG_GUI_PIMPL(NetworkSettingsDialog)
{
    VariableLineEditWidget *masterApi;
    GridPopupWidget *devPopup;
    CVarToggleWidget *devInfo;

    Instance(Public *i) : Base(i)
    {
        ScrollAreaWidget &area = self.area();

        area.add(masterApi = new VariableLineEditWidget(App::config()["masterServer.apiUrl"]));

        // Developer options.
        self.add(devPopup = new GridPopupWidget);
        devPopup->layout().setGridSize(1, 0);
        *devPopup << (devInfo = new CVarToggleWidget("net-dev"));
        devPopup->commit();
    }

    void fetch()
    {
        foreach(Widget *w, self.area().childWidgets() + devPopup->content().childWidgets())
        {
            if(ICVarWidget *cv = w->maybeAs<ICVarWidget>())
            {
                cv->updateFromCVar();
            }
        }
    }
};

NetworkSettingsDialog::NetworkSettingsDialog(String const &name)
    : DialogWidget(name, WithHeading), d(new Instance(this))
{
    heading().setText(tr("Network Settings"));

    d->devInfo->setText(tr("Developer Info"));

    LabelWidget *masterApiLabel = LabelWidget::newWithText(tr("Master API URL:"), &area());

    // Layout.
    GridLayout layout(area().contentRule().left(), area().contentRule().top());
    layout.setGridSize(2, 0);
    layout.setColumnAlignment(0, ui::AlignRight);
    layout << *masterApiLabel  << *d->masterApi;

    area().setContentSize(layout.width(), layout.height());

    buttons()
            << new DialogButtonItem(Default | Accept, tr("Close"))
            << new DialogButtonItem(Action, tr("Reset to Defaults"),
                                    new SignalAction(this, SLOT(resetToDefaults())))
            << new DialogButtonItem(Action | Id1, style().images().image("gauge"),
                                    new SignalAction(this, SLOT(showDeveloperPopup())));

    d->devPopup->setAnchorAndOpeningDirection(buttonWidget(Id1)->rule(), ui::Up);

    d->fetch();
}

void NetworkSettingsDialog::resetToDefaults()
{
    ClientApp::networkSettings().resetToDefaults();

    d->fetch();
}

void NetworkSettingsDialog::showDeveloperPopup()
{
    d->devPopup->open();
}
