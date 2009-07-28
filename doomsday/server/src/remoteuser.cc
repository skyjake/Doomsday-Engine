/*
 * The Doomsday Engine Project -- dengsv
 *
 * Copyright (c) 2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "remoteuser.h"
#include <de/core.h>

using namespace de;

RemoteUser::RemoteUser(Client& client, Session* session) 
    : client_(&client), session_(session), user_(0)
{
    user_ = App::game().SYMBOL(deng_NewUser)();
}

RemoteUser::~RemoteUser()
{
    if(session_)
    {
        // Remove this user from the session.
        session_->demote(*this);
    }
    delete user_;
}

Client& RemoteUser::client() const
{
    assert(client_ != NULL);
    return *client_;
}

Session& RemoteUser::session() const
{
    if(!session_)
    {
        /// @throw NotInSessionError The remote user is not associated with any session.
        throw NotInSessionError("RemoteUser::session", "Remote user not in session");
    }
    return *session_;
}

void RemoteUser::setSession(Session* session)
{
    session_ = session;
}

de::User& RemoteUser::user()
{
    return *user_;
}

de::Address RemoteUser::address() const
{
    return client().peerAddress();
}
