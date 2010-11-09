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

#ifndef SERVERAPP_H
#define SERVERAPP_H

#include <de/core.h>
#include <de/net.h>

class Session;
class Client;

/**
 * @defgroup client Client
 * The server application.
 */

/**
 * The server application.
 *
 * @ingroup server
 */
class ServerApp : public de::App
{
public:
    /// No session is currently active. @ingroup errors
    DEFINE_ERROR(NoSessionError);
    
    /// Specified address was not in use by any client. @ingroup errors
    DEFINE_ERROR(UnknownAddressError);
    
    /// Client does not have access rights to perform the operation. @ingroup errors
    DEFINE_ERROR(RightsError);
    
public:
    ServerApp(const de::CommandLine& commandLine);
    ~ServerApp();
    
    void iterate(const de::Time::Delta& elapsed);

    /**
     * Returns the client with the given address. 
     *
     * @param address  Address of client.
     *
     * @return  Client using the address.
     */
    Client& clientByAddress(const de::Address& address) const;

    /**
     * Check if there are any incoming requests from connected clients.
     * Process any incoming packets.
     */
    void tendClients();

    /**
     * Process a packet received from the network.
     *
     * @param packet  Packet.
     */    
    void processPacket(const de::Packet& packet);

    /**
     * Sends information about the current status of the server to a client.
     *
     * @param to  Address of the client to send the status to.
     */
    void replyStatus(const de::Address& to);

    /**
     * Checks that a client has administration rights.
     *
     * @param clientAddress  Address of the client.
     */
    void verifyAdmin(const de::Address& clientAddress) const;

public:
    /// Returns the singleton Server instance.
    static ServerApp& serverApp();
    
private:
    /// The server listens on this socket.
    de::ListenSocket* _listenSocket;
    
    /// The active game session.
    Session* _session;
    
    typedef std::list<Client*> Clients;
    Clients _clients;
};

#endif /* SERVERAPP_H */
