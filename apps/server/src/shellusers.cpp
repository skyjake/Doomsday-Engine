/** @file shellusers.cpp  All remote shell users.
 * @ingroup server
 *
 * @authors Copyright © 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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
#include <de/garbage.h>
#include <de/timer.h>

using namespace de;

static constexpr TimeSpan PLAYER_INFO_INTERVAL = 2.5_s;

DE_PIMPL_NOREF(ShellUsers)
{
    Timer infoTimer;

    Impl()
    {
        infoTimer.setInterval(PLAYER_INFO_INTERVAL);
    }
};

ShellUsers::ShellUsers() : d(new Impl)
{
    // Player information is sent periodically to all shell users.
    d->infoTimer += [this]() {
        forUsers([](User &user) {
            user.as<ShellUser>().sendPlayerInfo();
            return LoopContinue;
        });
    };
    d->infoTimer.start();
}

void ShellUsers::add(User *user)
{
    DE_ASSERT(is<ShellUser>(user));
    Users::add(user);

    LOG_NET_NOTE("New shell user from %s") << user->address();
    user->as<ShellUser>().sendInitialUpdate();
}

void ShellUsers::worldMapChanged()
{
    forUsers([] (User &user)
    {
        ShellUser &shellUser = user.as<ShellUser>();
        shellUser.sendGameState();
        shellUser.sendMapOutline();
        shellUser.sendPlayerInfo();
        return LoopContinue;
    });
}
