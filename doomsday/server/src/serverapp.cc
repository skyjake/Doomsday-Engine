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

#include "serverapp.h"
#include "client.h"
#include "session.h"
#include <de/data.h>
#include <de/types.h>
#include <de/net.h>

/*
#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
*/

//#include "doomsday.h"

using namespace de;

ServerApp::ServerApp(int argc, char** argv)
    : ConsoleApp(argc, argv, "/config/server/server.de", "server", de::Log::DEBUG),
      _listenSocket(0),
      _session(0)
{
    CommandLine& args = commandLine();

    logBuffer().enableStandardOutput();

    // Start listening.
    duint16 port = duint16(config().getui("net.listenPort"));
    String param;
    if(args.getParameter("--port", param))
    {
        port = param.toInt();
    }
    
    LOG_INFO("Server uses port ") << port;
    _listenSocket = new ListenSocket(port);
    connect(_listenSocket, SIGNAL(incomingConnection()), this, SLOT(acceptIncomingConnection()));
}

ServerApp::~ServerApp()
{
    delete _session;

    // Close all links.
    foreach(Client* c, _clients)
    {
        delete c;
    }
    _clients.clear();
    
    delete _listenSocket;
}

void ServerApp::acceptIncomingConnection()
{
    Socket* incoming = _listenSocket->accept();
    Q_ASSERT(incoming != 0);

    LOG_INFO("New client connected from %s.") << incoming->peerAddress();

    Client* newClient = new Client(incoming);
    _clients.append(newClient);

    // Listen to the client's notifications.
    connect(newClient, SIGNAL(disconnected()), this, SLOT(clientDisconnected()));
    connect(&newClient->base(), SIGNAL(messageReady()), this, SLOT(processIncomingMessage()));
}

void ServerApp::iterate(const Time::Delta& elapsed)
{
    // Perform thinking for the current map.
    if(hasCurrentMap())
    {
        currentMap().think(elapsed);
    }
    
    // libdeng main loop tasks.
    //LOG_TRACE("Entering libdeng gameloop...");
    //DD_GameLoop();

    //LOG_TRACE("Finished iteration.");
}

Client& ServerApp::clientByAddress(const de::Address& address) const
{
    foreach(Client* c, _clients)
    {
        if(c->socket().peerAddress() == address)
        {
            return *c;
        }
    }
    throw UnknownError("ServerApp::clientByAddress", "Address not in use by any client");
}

Client& ServerApp::clientByChannel(de::Channel* channel) const
{
    foreach(Client* c, _clients)
    {
        if(&c->base() == channel || &c->updates() == channel)
        {
            return *c;
        }
    }
    throw UnknownError("ServerApp::clientByChannel", "Channel not in use by any client");
}

void ServerApp::destroyClient(Client& client)
{
    _clients.removeOne(&client);
    client.deleteLater();
}

void ServerApp::clientDisconnected()
{
    Client* client = static_cast<Client*>(sender());

    LOG_INFO("Client from ") << client->socket().peerAddress() << " disconnected.";

    // Remove from the clients list.
    destroyClient(*client);
}

void ServerApp::processIncomingMessage()
{
    Channel* channel = static_cast<Channel*>(sender());

    try
    {
        Client& client = clientByChannel(channel);

        // Process incoming packets.
        QScopedPointer<Message> message(channel->receive());
        if(message)
        {
            QScopedPointer<Packet> packet(protocol().interpret(*message));
            if(packet)
            {
                packet->setFrom(message->address());
                processPacket(*packet);
            }
        }
    }
    catch(const RightsError& err)
    {
        // Reply that required rights are missing.
        protocol().reply(*channel, Protocol::DENY, err.asText());
    }
    catch(const ISerializable::DeserializationError&)
    {
        // Malformed packet!
        LOG_WARNING("Client from ") << channel->socket().peerAddress() << " sent nonsense.";
        destroyClient(clientByChannel(channel));
    }
    catch(const NoSessionError&)
    {
        LOG_WARNING("Client from ") << channel->socket().peerAddress() << " tried to access nonexistent session.";
        destroyClient(clientByChannel(channel));
    }
    catch(const UnknownError&)
    {}
}

void ServerApp::processPacket(const de::Packet& packet)
{
    const CommandPacket* cmd = dynamic_cast<const CommandPacket*>(&packet);
    if(cmd)
    {
        LOG_DEBUG("Server received command (from %s): %s") << packet.from() << cmd->command();
          
        /// @todo  Session new/delete commands require admin access rights.
        // Session commands are handled by the session.
        if(cmd->command().beginsWith("session."))
        {
            if(cmd->command() == "session.new")
            {
                verifyAdmin(packet.from());
                if(_session)
                {
                    // Could allow several...
                    delete _session;
                }
                // Start a new session.
                _session = new Session();
            }
            else if(cmd->command() == "session.delete")
            {
                verifyAdmin(packet.from());
                if(_session)
                {
                    delete _session;
                    _session = 0;
                    return;
                }
            }
            if(!_session)
            {
                throw NoSessionError("ServerApp::processPacket", "No session available");
            }
            // Execute the command.
            _session->processCommand(clientByAddress(packet.from()), *cmd);
        }
        else if(cmd->command() == "status")
        {
            replyStatus(packet.from());
        }
        else if(cmd->command() == "quit")
        {
            verifyAdmin(packet.from());
            stop();
        }
    }
    
    // Perform any function the packet may define for itself.    
    packet.execute();
}

void ServerApp::replyStatus(const de::Address& to)
{
    LOG_AS("replyStatus");
    
    RecordPacket status("server.status");
    Record& rec = status.record();
    
    // Version.
    const Version v = version();
    ArrayValue& array = rec.addArray("version").value<ArrayValue>();
    array.add(new NumberValue(v.major));
    array.add(new NumberValue(v.minor));
    array.add(new NumberValue(v.patchlevel));
    
    // The sessions.
    Record& sub = rec.addRecord("sessions");
    if(_session)
    {
        // Information about the session.
        _session->describe(sub.addRecord(_session->id()));
    }
    
    clientByAddress(to).base() << status;
    
    LOG_TRACE("Finished.");
}

void ServerApp::verifyAdmin(const de::Address& clientAddress) const
{
    if(!clientByAddress(clientAddress).rights.testFlag(Client::AdminRight))
    {
        /// @throw RightsError Client does not have administration rights.
        throw RightsError("ServerApp::verifyAdmin", "Admin rights required");
    }
}

ServerApp& ServerApp::serverApp()
{
    return static_cast<ServerApp&>(App::app());
}
