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

ShellUsers::ShellUsers()
{
    audienceForMapChange += this;
}

ShellUsers::~ShellUsers()
{
    audienceForMapChange -= this;

    foreach(ShellUser *user, _users)
    {
        delete user;
    }
}

void ShellUsers::add(ShellUser *user)
{
    LOG_INFO("New shell user from %s") << user->address();

    _users.insert(user);
    connect(user, SIGNAL(disconnected()), this, SLOT(userDisconnected()));

    user->sendInitialUpdate();
}

int ShellUsers::count() const
{
    return _users.size();
}

void ShellUsers::currentMapChanged()
{
    foreach(ShellUser *user, _users)
    {
        user->sendGameState();
    }
}

void ShellUsers::userDisconnected()
{
    DENG2_ASSERT(dynamic_cast<ShellUser *>(sender()) != 0);

    ShellUser *user = static_cast<ShellUser *>(sender());
    _users.remove(user);

    LOG_INFO("Shell user from %s has disconnected") << user->address();

    user->deleteLater();
}
