/** @file multiplayermenuwidget.cpp
 *
 * @authors Copyright (c) 2014 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "ui/widgets/multiplayermenuwidget.h"
#include "network/serverlink.h"
#include "clientapp.h"
#include "CommandAction"

#include <de/ui/ActionItem>
#include <de/ui/SubwidgetItem>
#include <QTimer>

using namespace de;

enum {
    POS_STATUS = 1
};

DENG_GUI_PIMPL(MultiplayerMenuWidget)
, DENG2_OBSERVES(ServerLink, Join)
, DENG2_OBSERVES(ServerLink, Leave)
{
    QTimer timer;

    Instance(Public *i) : Base(i)
    {
        timer.setInterval(1000);

        link().audienceForJoin += this;
        link().audienceForLeave += this;
    }

    ~Instance()
    {
        link().audienceForJoin -= this;
        link().audienceForLeave -= this;
    }

    void networkGameJoined()
    {
        /*self.menu().organizer().itemWidget(POS_SERVER_ADDRESS)->as<LabelWidget>()
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

MultiplayerMenuWidget::MultiplayerMenuWidget()
    : PopupMenuWidget("multiplayer-menu"), d(new Instance(this))
{    
    connect(&d->timer, SIGNAL(timeout()), this, SLOT(updateElapsedTime()));

    items()
            << new ui::ActionItem(tr("Disconnect"), new CommandAction("net disconnect"))
            //<< new ui::Item(ui::Item::Separator, tr("Connection:"))
            << new ui::Item(ui::Item::ShownAsLabel, ""); // sv address & time
}

void MultiplayerMenuWidget::updateElapsedTime()
{
    if(d->link().status() != ServerLink::Connected)
        return;
    
    TimeDelta const elapsed = d->link().connectedAt().since();

    items().at(POS_STATUS).setLabel(
                _E(s)_E(l) + tr("Server:") + _E(.) " " + d->link().address().asText() + "\n"
                _E(l) + tr("Connected:") + _E(.) +
                String(" %1:%2:%3")
                .arg(int(elapsed.asHours()))
                .arg(int(elapsed.asMinutes()) % 60, 2, 10, QLatin1Char('0'))
                .arg(int(elapsed) % 60, 2, 10, QLatin1Char('0')));
}

void MultiplayerMenuWidget::preparePanelForOpening()
{
    d->timer.start();
    updateElapsedTime();

    PopupMenuWidget::preparePanelForOpening();
}

void MultiplayerMenuWidget::panelClosing()
{
    d->timer.stop();
    PopupMenuWidget::panelClosing();
}
