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
#include "CommandAction"

#include <de/ui/ActionItem>

using namespace de;

DENG_GUI_PIMPL(MultiplayerMenuWidget)
, DENG2_OBSERVES(ServerLink, Join)
, DENG2_OBSERVES(ServerLink, Leave)
{
    Instance(Public *i) : Base(i)
    {}

    void networkGameJoined()
    {

    }

    void networkGameLeft()
    {

    }
};

MultiplayerMenuWidget::MultiplayerMenuWidget()
    : PopupMenuWidget("multiplayer-menu"), d(new Instance(this))
{
    items()
            << new ui::ActionItem(tr("Disconnect"), new CommandAction("net disconnect"));
}
