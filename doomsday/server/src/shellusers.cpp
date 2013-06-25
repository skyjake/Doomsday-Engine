/** @file shellusers.cpp  All remote shell users.
 * @ingroup server
 *
 * @authors Copyright © 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "shellusers.h"
#include "dd_main.h"
#include <QTimer>

using namespace de;

static int const PLAYER_INFO_INTERVAL = 2500; // ms

DENG2_PIMPL_NOREF(ShellUsers)
{
    QSet<ShellUser *> users;
    QTimer *infoTimer;

    Instance()
    {
        infoTimer = new QTimer;
        infoTimer->setInterval(PLAYER_INFO_INTERVAL);
    }

    ~Instance()
    {
        delete infoTimer;
    }
};

ShellUsers::ShellUsers() : d(new Instance)
{
    App_World().audienceForMapChange += this;

    // Player information is sent periodically to all shell users.
    connect(d->infoTimer, SIGNAL(timeout()), this, SLOT(sendPlayerInfoToAll()));
    d->infoTimer->start();
}

ShellUsers::~ShellUsers()
{
    d->infoTimer->stop();

    App_World().audienceForMapChange -= this;

    foreach(ShellUser *user, d->users)
    {
        delete user;
    }
}

void ShellUsers::add(ShellUser *user)
{
    LOG_INFO("New shell user from %s") << user->address();

    d->users.insert(user);
    connect(user, SIGNAL(disconnected()), this, SLOT(userDisconnected()));

    user->sendInitialUpdate();
}

int ShellUsers::count() const
{
    return d->users.size();
}

void ShellUsers::worldMapChanged(World &world)
{
    DENG2_UNUSED(world);

    foreach(ShellUser *user, d->users)
    {
        user->sendGameState();
        user->sendMapOutline();
        user->sendPlayerInfo();
    }
}

void ShellUsers::sendPlayerInfoToAll()
{
    foreach(ShellUser *user, d->users)
    {
        user->sendPlayerInfo();
    }
}

void ShellUsers::userDisconnected()
{
    DENG2_ASSERT(dynamic_cast<ShellUser *>(sender()) != 0);

    ShellUser *user = static_cast<ShellUser *>(sender());
    d->users.remove(user);

    LOG_INFO("Shell user from %s has disconnected") << user->address();

    user->deleteLater();
}
