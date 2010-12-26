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

#ifndef SESSION_H
#define SESSION_H

#include "serverapp.h"
#include "client.h"
#include <de/deng.h>
#include <de/net.h>
#include <de/types.h>
#include <de/world.h>

#include <QObject>
#include <QMap>

class RemoteUser;

/**
 * A session instance is the host for a game session. It owns the game world and 
 * is responsible for synchronizing the clients' UserSession instances.
 *
 * @ingroup server
 */
class Session : public QObject
{
public:
    /// Given address is not in use by anyone. @ingroup errors
    DEFINE_ERROR(UnknownAddressError);
    
    /// A client that already was a user in the session attempted promotion. @ingroup errors
    DEFINE_ERROR(AlreadyPromotedError);
    
    /**
     * Utility for sending a message to all remote users. Reception is not
     * supported.
     */
    class Broadcast : public de::Transmitter {
    public:
        Broadcast(Session& session) : _session(session), _exclude(0) {}
        Broadcast& exclude(RemoteUser* user) { 
            _exclude = user; 
            return *this;
        }
        /// Sends @a data to all users in the session.
        void send(const de::IByteArray& data);
    private:
        Session& _session;
        RemoteUser* _exclude;
    };
    
public:
    Session();

    virtual ~Session();

    const de::Id& id() const { return _id; }

    /**
     * Process a command related to the session. Any access rights must be
     * checked before calling this.
     *
     * @param sender  Sender of the command. A reply will be sent here.
     * @param packet  Packet received from the network.
     */
    void processCommand(Client& sender, const de::CommandPacket& packet);

    /**
     * Promotes a client to a User in the session. 
     */
    RemoteUser& promote(Client& client);

    /**
     * Remove a user from the session.
     *
     * @param remoteUser  User to remove. The RemoteUser instance is not deleted.
     */
    void demote(RemoteUser& remoteUser);

    /**
     * Finds a user.
     *
     * @param address  Address of the user to find.
     *
     * @return  User with the given address.
     */
    RemoteUser& userByAddress(const de::Address& address) const;

    /**
     * Returns a broadcast transmitter that sends to all users in the session.
     */
    Broadcast broadcast() { return Broadcast(*this); }

    /**
     * Forms a description of the session.
     *
     * @param record  Record where to add variables that contain the description.
     */
    void describe(de::Record& record) const;

protected slots:
    void remoteUserDisconnected();

private:
    de::Id _id;

    /// The game world.
    de::World* _world;
    
    // The remote users.
    typedef QMap<de::Id, RemoteUser*> Users;
    Users _users;
};

#endif /* SESSION_H */
