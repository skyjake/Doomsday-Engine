/*
 * The Doomsday Engine Project -- dengcl
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

#ifndef USERSESSION_H
#define USERSESSION_H

#include <de/types.h>
#include <de/net.h>
#include <de/world.h>

#include <QMap>

/**
 * Maintains the game session on the clientside.
 *
 * @ingroup client
 */
class UserSession
{
public:
    /// The serverside session has ended, making this user session invalid. @ingroup errors
    DEFINE_ERROR(SessionEndedError);
    
public:
    /**
     * Constructs a new user session.
     *
     * @param socket  Open connection to the server. Ownership given to UserSession.
     * @param id      Session to join. Ongoing sessions on a server can be queried with
     *                the "status" command.
     */
    UserSession(de::Socket* socket, const de::Id& id = 0);
    
    virtual ~UserSession();
    
    /**
     * Listen to updates and other data coming from the server.
     */
    void listen();
    
protected:
    /// Listens on the updates channel.
    void listenForUpdates();
    
    /**
     * Processes a packet received from the server.
     *
     * @param packet  Packet to process.
     */
    void processPacket(const de::Packet& packet);

    void clearOthers();
    
private:
    /// Connection to the server.
    de::Socket* _socket;
    
    /// Id of the session on the server.
    de::Id _sessionId;

    /// The game world. Mirrors the game world in the server's Session.
    de::World* _world;

    /// The user that owns the UserSession.
    de::User* _user;

    /// The others.
    typedef QMap<de::Id, de::User*> Others;
    Others _others;
};

#endif /* USERSESSION_H */
