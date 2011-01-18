/*
 * The Doomsday Engine Project -- dengsv
 *
 * Copyright (c) 2009, 2011 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef REMOTEUSER_H
#define REMOTEUSER_H

#include "serverapp.h"
#include "session.h"
#include <de/net.h>

#include <QObject>

class Client;

/**
 * RemoteUser represents a User on the serverside.
 *
 * @ingroup server
 */
class RemoteUser : public QObject
{
public:
    /// No session specified for the remote user. @ingroup errors
    DEFINE_ERROR(NotInSessionError);
    
public:
    /**
     * Constructs a new remote user.
     *
     * @param client  Network link for communicating with the user.
     * @param session  Session to which this RemoteUser belongs.
     */
    RemoteUser(Client& client, Session* session);
    
    virtual ~RemoteUser();

    /**
     * Returns the user's id.
     */
    const de::Id& id() const { return _user->id(); }

    /**
     * Returns the address of the remote user.
     */
    de::Address address() const;
    
    /**
     * Returns the network link for communicating with the remote user.
     */
    Client& client() const;
    
    /**
     * Returns the session to which this remote user belongs.
     */
    Session& session() const;
    
    /**
     * Sets the session this remote user belong to.
     *
     * @param session  Session.
     */
    void setSession(Session* session);
        
    /**
     * Returns the User instance of the remote user.
     */
    de::User& user();

    /**
     * Returns the User instance of the remote user.
     */
    const de::User& user() const;

signals:
    void disconnected();

private:
    Client* _client;
    
    /// Session to which this remote user belongs.
    Session* _session;
    
    /// The game user.
    de::User* _user;
};

#endif /* REMOTEUSER_H */
