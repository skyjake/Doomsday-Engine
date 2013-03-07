/** @file serverlink.cpp  Network connection to a server.
 *
 * @authors Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "network/serverlink.h"
#include "network/net_main.h"
#include "network/net_buf.h"
#include "network/net_demo.h"
#include "network/protocol.h"
#include "client/cl_def.h"
#include "dd_def.h"
#include <de/memory.h>
#include <de/Socket>
#include <de/Message>
#include <de/ByteRefArray>
#include <de/BlockPacket>
#include <de/shell/ServerFinder>

using namespace de;

enum LinkState
{
    None,
    Discovering,
    WaitingForInfoResponse,
    Joining,
    WaitingForJoinResponse,
    InGame
};

DENG2_PIMPL(ServerLink)
{
    shell::ServerFinder finder; ///< Finding local servers.
    LinkState state;
    typedef QMap<Address, serverinfo_t> Servers;
    Servers discovered;

    Instance(Public *i)
        : Base(i),
          state(None)
    {}

    ~Instance()
    {}

    void notifyDiscoveryUpdate()
    {
        DENG2_FOR_PUBLIC_AUDIENCE(DiscoveryUpdate, i) i->linkDiscoveryUpdate(self);
        emit self.serversDiscovered();
    }

    bool handleInfoResponse(Block const &reply)
    {
        DENG2_ASSERT(state == WaitingForInfoResponse);

        // Address of the server where the info was received.
        Address svAddress = self.address();

        // Local addresses are all represented as "localhost".
        if(svAddress.isLocal()) svAddress.setHost(QHostAddress::LocalHost);

        // Close the connection; that was all the information we need.
        self.disconnect();

        // Did we receive what we expected to receive?
        if(reply.size() >= 5 && reply.startsWith("Info\n"))
        {
            const char *ch;
            ddstring_t *line;
            ddstring_t* response = Str_New();

            // Make a null-terminated copy of the response text.
            Str_PartAppend(response, (char const *) reply.data(), 0, reply.size());

            serverinfo_t svInfo;

            // Convert the string into a serverinfo_s.
            line = Str_New();
            ch = Str_Text(response);
            do
            {
                ch = Str_GetLine(line, ch);
                Net_StringToServerInfo(Str_Text(line), &svInfo);
            }
            while(*ch);

            Str_Delete(line);
            Str_Delete(response);

            LOG_DEBUG("Discovered server at ") << svAddress;

            // Update with the correct address.
            strncpy(svInfo.address, svAddress.host().toString().toAscii(),
                    sizeof(svInfo.address) - 1);

            discovered.insert(svAddress, svInfo);

            // Show the information in the console.
            LOG_INFO("%i server%s been found")
                    << discovered.size()
                    << (discovered.size() != 1 ? "s have" : " has");

            Net_PrintServerInfo(0, NULL);
            Net_PrintServerInfo(0, &svInfo);

            notifyDiscoveryUpdate();
        }
        else
        {
            LOG_WARNING("Reply from %s was invalid") << svAddress;
        }

        return false;
    }

    /**
     * Handles the server's response to a client's join request.
     * @return @c false to stop processing further messages.
     */
    bool handleJoinResponse(Block const &reply)
    {
        if(reply.size() < 5 || reply != "Enter")
        {
            LOG_WARNING("Server refused connection (received %i bytes instead of \"Enter\")")
                    << reply.size();
            self.disconnect();
            return false;
        }

        // We'll switch to joined mode.
        // Clients are allowed to send packets to the server.
        state = InGame;

        handshakeReceived = false;
        allowSending = true;
        netGame = true;             // Allow sending/receiving of packets.
        isServer = false;
        isClient = true;

        // Call game's NetConnect.
        gx.NetConnect(false);

        // G'day mate!  The client is responsible for beginning the
        // handshake.
        Cl_SendHello();

        return true;
    }

    Servers allFound() const
    {
        Servers all = discovered;

        // Append the ones from the server finder.
        foreach(Address sv, finder.foundServers())
        {
            serverinfo_t info;
            Net_RecordToServerInfo(finder.messageFromServer(sv), &info);

            // Update the address in the info, which is missing because this
            // information didn't come from the master.
            strncpy(info.address, sv.host().toString().toAscii(), sizeof(info.address) - 1);

            all.insert(sv, info);
        }

        return all;
    }
};

ServerLink::ServerLink() : d(new Instance(this))
{
    connect(&d->finder, SIGNAL(updated()), this, SLOT(localServersFound()));
    connect(this, SIGNAL(packetsReady()), this, SLOT(handleIncomingPackets()));
    connect(this, SIGNAL(disconnected()), this, SLOT(linkDisconnected()));
}

void ServerLink::clear()
{
    d->finder.clear();
    // TODO: clear all found servers
}

void ServerLink::connectDomain(String const &domain, TimeDelta const &timeout)
{
    LOG_AS("ServerLink::connectDomain");

    AbstractLink::connectDomain(domain, timeout);
    d->state = Joining;
}

void ServerLink::connectHost(Address const &address)
{
    LOG_AS("ServerLink::connectHost");

    AbstractLink::connectHost(address);
    d->state = Joining;
}

void ServerLink::linkDisconnected()
{
    LOG_AS("ServerLink");
    if(d->state != None)
    {
        LOG_INFO("Connection to server was disconnected");

        // Update our state and notify the game.
        disconnect();
    }
}

void ServerLink::disconnect()
{
    if(d->state == None) return;

    LOG_AS("ServerLink::disconnect");

    if(d->state == InGame)
    {
        d->state = None;

        // Tell the Game that a disconnection is about to happen.
        if(gx.NetDisconnect)
            gx.NetDisconnect(true);

        LOG_INFO("Link to server %s disconnected") << address();

        AbstractLink::disconnect();

        Net_StopGame();

        // Tell the Game that the disconnection is now complete.
        if(gx.NetDisconnect)
            gx.NetDisconnect(false);
    }
    else
    {
        d->state = None;

        LOG_INFO("Connection attempts aborted");
        AbstractLink::disconnect();
    }
}

void ServerLink::discover(String const &domain)
{
    AbstractLink::connectDomain(domain, 5 /*timeout*/);

    d->discovered.clear();
    d->state = Discovering;
}

bool ServerLink::isDiscovering() const
{
    return (d->state == Discovering || d->state == WaitingForInfoResponse);
}

int ServerLink::foundServerCount() const
{
    return d->allFound().size();
}

QList<Address> ServerLink::foundServers() const
{
    return d->allFound().keys();
}

bool ServerLink::foundServerInfo(int index, serverinfo_t *info) const
{
    Instance::Servers all = d->allFound();
    QList<Address> listed = all.keys();
    if(index < 0 || index >= listed.size()) return false;
    memcpy(info, &all[listed[index]], sizeof(*info));
    return true;
}

bool ServerLink::foundServerInfo(de::Address const &host, serverinfo_t *info) const
{
    Instance::Servers all = d->allFound();
    if(!all.contains(host)) return false;
    memcpy(info, &all[host], sizeof(*info));
    return true;
}

Packet *ServerLink::interpret(Message const &msg)
{
    return new BlockPacket(msg);
}

void ServerLink::initiateCommunications()
{
    if(d->state == Discovering)
    {
        // Ask for the serverinfo.
        *this << ByteRefArray("Info?", 5);

        d->state = WaitingForInfoResponse;
    }
    else if(d->state == Joining)
    {
        Demo_StopPlayback();

        // Tell the Game that a connection is about to happen.
        // The counterpart (false) call will occur after joining is successfully done.
        gx.NetConnect(true);

        // Connect by issuing: "Join (myname)"
        String pName = (playerName? playerName : "");
        if(pName.isEmpty())
        {
            pName = "Player";
        }
        String req = String("Join %1 %2").arg(SV_VERSION, 4, 16, QChar('0')).arg(pName);
        *this << req.toUtf8();

        d->state = WaitingForJoinResponse;
    }
    else
    {
        DENG2_ASSERT(false);
    }
}

void ServerLink::localServersFound()
{
    d->notifyDiscoveryUpdate();
}

void ServerLink::handleIncomingPackets()
{
    if(d->state == Discovering || d->state == Joining)
        return;

    LOG_AS("ServerLink");
    forever
    {
        // Only BlockPackets received (see interpret()).
        QScopedPointer<BlockPacket> packet(static_cast<BlockPacket *>(nextPacket()));
        if(packet.isNull()) break;

        switch(d->state)
        {
        case WaitingForInfoResponse:
            if(!d->handleInfoResponse(*packet)) return;
            break;

        case WaitingForJoinResponse:
            if(!d->handleJoinResponse(*packet)) return;
            break;

        case InGame: {
            /// @todo The incoming packets should go be handled immediately.

            // Post the data into the queue.
            netmessage_t *msg = (netmessage_t *) M_Calloc(sizeof(netmessage_t));

            msg->sender = 0; // the server
            msg->data = new byte[packet->size()];
            memcpy(msg->data, packet->data(), packet->size());
            msg->size = packet->size();
            msg->handle = msg->data; // needs delete[]

            // The message queue will handle the message from now on.
            N_PostMessage(msg);
            break; }

        default:
            // Ignore any packets left.
            break;
        }
    }
}
