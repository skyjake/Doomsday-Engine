/** @file multiplayerstatuswidget.cpp
 *
 * @authors Copyright (c) 2014-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "ui/widgets/multiplayerstatuswidget.h"
#include "network/serverlink.h"
#include "clientapp.h"
#include "ui/commandaction.h"

#include <de/ui/actionitem.h>
#include <de/ui/subwidgetitem.h>
#include <de/timer.h>

using namespace de;

enum {
    POS_STATUS = 1
};

DE_GUI_PIMPL(MultiplayerStatusWidget)
, DE_OBSERVES(ServerLink, Join)
, DE_OBSERVES(ServerLink, Leave)
{
    Timer timer;

    Impl(Public *i) : Base(i)
    {
        timer.setInterval(1.0);

        link().audienceForJoin() += this;
        link().audienceForLeave() += this;
    }

    void networkGameJoined()
    {
        /*self().menu().organizer().itemWidget(POS_SERVER_ADDRESS)->as<LabelWidget>()
                .setText(_E(l) + tr("Server:") + _E(.) +
                         " " + link().address().asText());*/
    }

    void networkGameLeft()
    {

    }

    static ServerLink &link()
    {
        return ClientApp::serverLink();
    }
};

MultiplayerStatusWidget::MultiplayerStatusWidget()
    : PopupMenuWidget("multiplayer-menu"), d(new Impl(this))
{
    d->timer += [this](){ updateElapsedTime(); };

    items()
            << new ui::ActionItem("Disconnect", new CommandAction("net disconnect"))
            //<< new ui::Item(ui::Item::Separator, tr("Connection:"))
            << new ui::Item(ui::Item::ShownAsLabel, ""); // sv address & time
}

void MultiplayerStatusWidget::updateElapsedTime()
{
    if (d->link().status() != ServerLink::Connected)
        return;

    const TimeSpan elapsed = d->link().connectedAt().since();

    items()
        .at(POS_STATUS)
        .setLabel(_E(s) _E(l) "Server:" _E(.) " " + d->link().address().asText() +
                  "\n" _E(l) "Connected:" _E(.) +
                  Stringf(" %i:%02i:%02i",
                                 int(elapsed.asHours()),
                                 int(elapsed.asMinutes()) % 60,
                                 int(elapsed) % 60));
}

void MultiplayerStatusWidget::preparePanelForOpening()
{
    d->timer.start();
    updateElapsedTime();

    PopupMenuWidget::preparePanelForOpening();
}

void MultiplayerStatusWidget::panelClosing()
{
    d->timer.stop();
    PopupMenuWidget::panelClosing();
}
