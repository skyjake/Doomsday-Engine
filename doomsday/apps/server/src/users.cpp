/** @file users.cpp
 *
 * @authors Copyright (c) 2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "users.h"
#include <de/garbage.h>
#include <de/set.h>

using namespace de;

DE_PIMPL_NOREF(Users)
, DE_OBSERVES(User, Disconnect)
{
    Set<User *> users;

    void userDisconnected(User &user) override
    {
        LOG_NET_MSG("User from %s has disconnected") << user.address();

        user.audienceForDisconnect -= this;
        users.remove(&user);
        de::trash(&user);
    }
};

Users::Users() : d(new Impl)
{}

Users::~Users()
{
    for (User *user : d->users)
    {
        delete user;
    }
}

void Users::add(User *user)
{
    DE_ASSERT(user);
    d->users.insert(user);
    user->audienceForDisconnect += d;
}

LoopResult Users::forUsers(const std::function<LoopResult (User &)>& func)
{
    for (User *user : d->users)
    {
        if (auto result = func(*user))
        {
            return result;
        }
    }
    return LoopContinue;
}

int Users::count() const
{
    return d->users.size();
}
