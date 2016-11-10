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

#include "de_platform.h"
#include "network/serverlink.h"
#include "network/masterserver.h"
#include "network/net_main.h"
#include "network/net_buf.h"
#include "network/net_demo.h"
#include "network/net_event.h"
#include "network/protocol.h"
#include "client/cl_def.h"
#include "ui/clientwindow.h"
#include "dd_def.h"

#include <QTimer>
#include <doomsday/Games>
#include <de/memory.h>
#include <de/data/json.h>
#include <de/GuiApp>
#include <de/Socket>
#include <de/RecordValue>
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
    bool fetching;
    typedef QMap<Address, shell::ServerInfo> Servers;
    Servers discovered;
    Servers fromMaster;
    std::unique_ptr<GameProfile> serverProfile; ///< Profile used when joining.
    std::function<void (GameProfile const *)> profileResultCallback;
    LoopCallback mainCall;

    Impl(Public *i)
        : Base(i)
        , state(None)
        , fetching(false)
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
        if (svAddress.isLocal()) svAddress.setHost(QHostAddress::LocalHost);

        // Close the connection; that was all the information we need.
        self.disconnect();

        // Did we receive what we expected to receive?
        if (reply.size() >= 5 && reply.startsWith("Info\n"))
        {
            try
            {
                QVariant const response = parseJSON(String::fromUtf8(Block(reply.mid(5))));
                std::unique_ptr<Value> rec(Value::constructFrom(response.toMap()));
                if (!rec->is<RecordValue>())
                {
                    throw Error("ServerLink::handleInfoResponse", "Failed to parse response contents");
                }
                shell::ServerInfo svInfo(*rec->as<RecordValue>().record());

                LOG_NET_VERBOSE("Discovered server at ") << svAddress;

                // Update with the correct address.
                svAddress = Address(svAddress.host(), svInfo.port());
                svInfo.setAddress(svAddress);
                svInfo.printToLog(0, true);

                discovered.insert(svAddress, svInfo);

                // Show the information in the console.
                LOG_NET_NOTE("%i server%s been found")
                        << discovered.size()
                        << (discovered.size() != 1 ? "s have" : " has");

                notifyDiscoveryUpdate();

                // If the server's profile is being acquired, do the callback now.
                if (profileResultCallback)
                {
                    if (prepareServerProfile(svAddress))
                    {
                        LOG_NET_MSG("Received server's game profile from ") << svAddress;
                        mainCall.enqueue([this] ()
                        {
                            profileResultCallback(serverProfile.get());
                            profileResultCallback = nullptr;
                        });
                    }
                }
                return true;
            }
            catch (Error const &er)
            {
                LOG_NET_WARNING("Reply from %s was invalid: %s") << svAddress << er.asText();
            }
        }
        else
        {
            LOG_NET_WARNING("Reply from %s was invalid") << svAddress;
        }
        return false;
    }

    /**
     * Handles the server's response to a client's join request.
     * @return @c false to stop processing further messages.
     */
    bool handleJoinResponse(Block const &reply)
    {
        if (reply.size() < 5 || reply != "Enter")
        {
            LOG_NET_WARNING("Server refused connection");
            LOGDEV_NET_WARNING("Received %i bytes instead of \"Enter\")")
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

        DENG2_FOR_PUBLIC_AUDIENCE(Join, i) i->networkGameJoined();

        // G'day mate!  The client is responsible for beginning the
        // handshake.
        Cl_SendHello();

        return true;
    }

    void fetchFromMaster()
    {
        if (fetching) return;

        fetching = true;
        N_MAPost(MAC_REQUEST);
        N_MAPost(MAC_WAIT);
        mainCall.enqueue([this] () { checkMasterReply(); });
    }

    void checkMasterReply()
    {
        DENG2_ASSERT(fetching);

        if (N_MADone())
        {
            fetching = false;

            fromMaster.clear();
            int const count = N_MasterGet(0, 0);
            for (int i = 0; i < count; i++)
            {
                shell::ServerInfo info;
                N_MasterGet(i, &info);
                fromMaster.insert(info.address(), info);
            }

            notifyDiscoveryUpdate();
        }
        else
        {
            // Check again later.
            mainCall.enqueue([this] () { checkMasterReply(); });
        }
    }

    bool prepareServerProfile(Address const &host)
    {
        serverProfile.reset();

        // Do we have information about this host?
        shell::ServerInfo info;
        if (!self.foundServerInfo(host, info))
        {
            return false;
        }
        qDebug() << "For profile:" << info.asText().toLatin1().constData();
        if (info.gameId().isEmpty() || info.packages().isEmpty())
        {
            // There isn't enough information here.
            return false;
        }

        serverProfile.reset(new GameProfile(info.name()));
        serverProfile->setGame(info.gameId());

        try
        {
            // Figure out the packages needed in the profile.
            Game const &game = DoomsdayApp::games()[info.gameId()];
            StringList packagesForProfile = info.packages();
            // Omit the required packages from the profile.
            foreach (String requiredPackage, game.requiredPackages())
            {
                //auto const svPkg  = Package::split(packagesForProfile.first());
                //auto const reqPkg = Package::split(requiredPackage);
                //File const *reqPkg = App::packageLoader().select(requiredPackage);

                //qDebug() << svPkg.first << svPkg.second.asText() << reqPkg.first << reqPkg.second.asText();

                if (Package::equals(packagesForProfile.first(), requiredPackage))
                {
                    // Both IDs and versions must match.
                    packagesForProfile.removeFirst();
                }
                else
                {
                    break;
                }
            }
            serverProfile->setPackages(packagesForProfile);
            return true;
        }
        catch (Error const &er)
        {
            LOG_ERROR("Server's game \"%s\" not supported locally: ")
                    << info.gameId() << er.asText();
            return false;
        }
    }

    Servers allFound(FoundMask const &mask) const
    {
        Servers all;
        if (mask.testFlag(LocalNetwork))
        {
            // Append the ones from the server finder.
            foreach (Address const &sv, finder.foundServers())
            {
                all.insert(sv, finder.messageFromServer(sv));
            }
        }
        if (mask.testFlag(MasterServer))
        {
            // Append from master (if available).
            for (auto i = fromMaster.constBegin(); i != fromMaster.constEnd(); ++i)
            {
                all.insert(i.key(), i.value());
            }
        }
        if (mask.testFlag(Direct))
        {
            for (auto i = discovered.constBegin(); i != discovered.constEnd(); ++i)
            {
                all.insert(i.key(), i.value());
            }
        }
        return all;
    }
};

ServerLink::ServerLink() : d(new Impl(this))
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

void ServerLink::acquireServerProfile(Address const &address,
                                      std::function<void (GameProfile const *)> resultHandler)
{
    if (d->prepareServerProfile(address))
    {
        // We already know everything that is needed for the profile.
        d->mainCall.enqueue([this, resultHandler] ()
        {
            DENG2_ASSERT(d->serverProfile.get() != nullptr);
            resultHandler(d->serverProfile.get());
        });
    }
    else
    {
        AbstractLink::connectHost(address);
        d->profileResultCallback = resultHandler;
        d->state = Discovering;
        LOG_NET_MSG("Querying %s for full status") << address;
    }
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
    if (d->state != None)
    {
        LOG_NET_NOTE("Connection to server was disconnected");

        // Update our state and notify the game.
        disconnect();
    }
}

void ServerLink::disconnect()
{
    if (d->state == None) return;

    LOG_AS("ServerLink::disconnect");

    if (d->state == InGame)
    {
        d->state = None;

        // Tell the Game that a disconnection is about to happen.
        if (gx.NetDisconnect)
            gx.NetDisconnect(true);

        DENG2_FOR_AUDIENCE(Leave, i) i->networkGameLeft();

        LOG_NET_NOTE("Link to server %s disconnected") << address();

        AbstractLink::disconnect();

        Net_StopGame();

        // Tell the Game that the disconnection is now complete.
        if (gx.NetDisconnect)
            gx.NetDisconnect(false);
    }
    else
    {
        d->state = None;

        LOG_NET_NOTE("Connection attempts aborted");
        AbstractLink::disconnect();
    }
}

void ServerLink::discover(String const &domain)
{
    AbstractLink::connectDomain(domain, 5 /*timeout*/);

    d->discovered.clear();
    d->state = Discovering;
}

void ServerLink::discoverUsingMaster()
{
    d->fetchFromMaster();
}

bool ServerLink::isDiscovering() const
{
    return (d->state == Discovering ||
            d->state == WaitingForInfoResponse ||
            d->fetching);
}

int ServerLink::foundServerCount(FoundMask mask) const
{
    return d->allFound(mask).size();
}

QList<Address> ServerLink::foundServers(FoundMask mask) const
{
    return d->allFound(mask).keys();
}

bool ServerLink::isFound(Address const &host, FoundMask mask) const
{
    Address addr = host;
    if (!addr.port())
    {
        // Zero means default port.
        addr.setPort(shell::DEFAULT_PORT);
    }
    return d->allFound(mask).contains(addr);
}

bool ServerLink::foundServerInfo(int index, shell::ServerInfo &info, FoundMask mask) const
{
    Impl::Servers const all = d->allFound(mask);
    QList<Address> const listed = all.keys();
    if (index < 0 || index >= listed.size()) return false;
    info = all[listed[index]];
    return true;
}

bool ServerLink::isServerOnLocalNetwork(Address const &host) const
{
    return d->finder.foundServers().contains(host);
}

bool ServerLink::foundServerInfo(de::Address const &host, shell::ServerInfo &info, FoundMask mask) const
{
    Impl::Servers const all = d->allFound(mask);
    if (!all.contains(host)) return false;
    info = all[host];
    return true;
}

Packet *ServerLink::interpret(Message const &msg)
{
    return new BlockPacket(msg);
}

void ServerLink::initiateCommunications()
{
    if (d->state == Discovering)
    {
        // Ask for the serverinfo.
        *this << ByteRefArray("Info?", 5);

        d->state = WaitingForInfoResponse;
    }
    else if (d->state == Joining)
    {
        ClientWindow::main().glActivate();

        Demo_StopPlayback();
        BusyMode_FreezeGameForBusyMode();

        // Tell the Game that a connection is about to happen.
        // The counterpart (false) call will occur after joining is successfully done.
        gx.NetConnect(true);

        // Connect by issuing: "Join (myname)"
        String pName = (playerName? playerName : "");
        if (pName.isEmpty())
        {
            pName = "Player";
        }
        String req = String("Join %1 %2").arg(SV_VERSION, 4, 16, QChar('0')).arg(pName);
        *this << req.toUtf8();

        d->state = WaitingForJoinResponse;

        ClientWindow::main().glDone();
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
    if (d->state == Discovering)
        return;

    LOG_AS("ServerLink");
    forever
    {
        // Only BlockPackets received (see interpret()).
        QScopedPointer<BlockPacket> packet(static_cast<BlockPacket *>(nextPacket()));
        if (packet.isNull()) break;

        switch (d->state)
        {
        case WaitingForInfoResponse:
            if (!d->handleInfoResponse(*packet)) return;
            break;

        case WaitingForJoinResponse:
            if (!d->handleJoinResponse(*packet)) return;
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
