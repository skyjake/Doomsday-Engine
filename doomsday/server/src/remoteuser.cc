/*
 * The Doomsday Engine Project -- dengsv
 *
 * Copyright (c) 2009, 2010 Jaakko Keränen <jaakko.keranen@iki.fi>
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
    : _client(&client), _session(session), _user(0)
{
    connect(_client, SIGNAL(disconnected()), this, SIGNAL(disconnected()));

    _user = GAME_SYMBOL(deng_NewUser)();
}

RemoteUser::~RemoteUser()
{
    if(_session)
    {
        // Remove this user from the session.
        _session->demote(*this);
    }
    delete _user;
}

Client& RemoteUser::client() const
{
    Q_ASSERT(_client != NULL);
    return *_client;
}

Session& RemoteUser::session() const
{
    if(!_session)
    {
        /// @throw NotInSessionError The remote user is not associated with any session.
        throw NotInSessionError("RemoteUser::session", "Remote user not in session");
    }
    return *_session;
}

void RemoteUser::setSession(Session* session)
{
    _session = session;
}

de::User& RemoteUser::user()
{
    return *_user;
}

de::Address RemoteUser::address() const
{
    return client().socket().peerAddress();
}
