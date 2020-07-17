/** @file serverlink.cpp  Network connection to a server.
 *
 * @authors Copyright (c) 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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
#include "network/net_main.h"
#include "network/net_buf.h"
#include "network/net_demo.h"
#include "network/net_event.h"
#include "client/cl_def.h"
#include "ui/clientwindow.h"
#include "ui/widgets/taskbarwidget.h"
#include "ui/dialogs/filedownloaddialog.h"
#include "dd_def.h"
#include "dd_main.h"
#include "clientapp.h"

#include <doomsday/doomsdayapp.h>
#include <doomsday/games.h>
#include <doomsday/network/masterserver.h>
#include <doomsday/network/protocol.h>

#include <de/async.h>
#include <de/blockpacket.h>
#include <de/byterefarray.h>
#include <de/bytesubarray.h>
#include <de/elapsedtimer.h>
#include <de/guiapp.h>
#include <de/message.h>
#include <de/messagedialog.h>
#include <de/packageloader.h>
#include <de/recordvalue.h>
#include <de/serverfinder.h>
#include <de/socket.h>
#include <de/timer.h>
#include <de/json.h>
#include <de/legacy/memory.h>

using namespace de;

enum LinkState {
    None,
    Discovering,
    Pinging,
    WaitingForPong,
    QueryingMapOutline,
    WaitingForInfoResponse,
    Joining,
    WaitingForJoinResponse,
    InGame
};

static const int NUM_PINGS = 5;

DE_PIMPL(ServerLink)
{
    std::unique_ptr<ServerFinder> finder; ///< Finding local servers.
    LinkState state;
    bool fetching;
    typedef KeyMap<Address, ServerInfo> Servers;
    Servers discovered;
    Servers fromMaster;
    ElapsedTimer pingTimer;
    List<TimeSpan> pings;
    int pingCounter;
    std::unique_ptr<GameProfile> serverProfile; ///< Profile used when joining.
    std::function<void (const GameProfile *)> profileResultCallback;
    std::function<void (Address, const GameProfile *)> profileResultCallbackWithAddress;
    PackageDownloader downloader;
    Dispatch deferred; // for deferred actions

    Impl(Public *i, Flags flags)
        : Base(i)
        , state(None)
        , fetching(false)
    {
        if (flags & DiscoverLocalServers)
        {
            finder.reset(new ServerFinder);
        }
    }

    void notifyDiscoveryUpdate()
    {
        DE_NOTIFY_PUBLIC(Discovery, i) i->serversDiscovered(self());
    }

    bool handleInfoResponse(const Block &reply)
    {
        DE_ASSERT(state == WaitingForInfoResponse);

        // Address of the server where the info was received.
        Address svAddress = self().address();

//         Local addresses are all represented as "localhost".
//        if (svAddress.isLocal()) svAddress.setHost(QHostAddress::LocalHost);

        // Close the connection; that was all the information we need.
        self().disconnect();

        // Did we receive what we expected to receive?
        if (reply.size() >= 5 && reply.beginsWith("Info\n"))
        {
            try
            {
                const Record response = parseJSON(reply.mid(5));
                /*std::unique_ptr<Value> rec(Value::constructFrom(response.toMap()));
                if (!is<RecordValue>(*rec))
                {
                    throw Error("ServerLink::handleInfoResponse", "Failed to parse response contents");
                }*/
                ServerInfo svInfo(response); //*rec->as<RecordValue>().record());

                LOG_NET_VERBOSE("Discovered server at ") << svAddress;

                // Update with the correct address.
                svAddress = Address(svAddress.hostName(), svInfo.port());
                svInfo.setAddress(svAddress);
                svInfo.printToLog(0, true);

                discovered.insert(svAddress, svInfo);

                // Show the information in the console.
                LOG_NET_NOTE("%i server%s been found")
                        << discovered.size()
                        << (discovered.size() != 1 ? "s have" : " has");

                notifyDiscoveryUpdate();

                // If the server's profile is being acquired, do the callback now.
                if (profileResultCallback ||
                    profileResultCallbackWithAddress)
                {
                    if (prepareServerProfile(svAddress))
                    {
                        LOG_NET_MSG("Received server's game profile from ") << svAddress;
                        deferred.enqueue([this, svAddress] ()
                        {
                            if (profileResultCallback)
                            {
                                profileResultCallback(serverProfile.get());
                                profileResultCallback = nullptr;
                            }
                            if (profileResultCallbackWithAddress)
                            {
                                profileResultCallbackWithAddress(svAddress, serverProfile.get());
                                profileResultCallbackWithAddress = nullptr;
                            }
                        });
                    }
                }
                return true;
            }
            catch (const Error &er)
            {
                LOG_NET_WARNING("Info reply from %s was invalid: %s") << svAddress << er.asText();
            }
        }
        else if (reply.size() >= 11 && reply.beginsWith("MapOutline\n"))
        {
            try
            {
                network::MapOutlinePacket outline;
                {
                    const Block data = Block(ByteSubArray(reply, 11)).decompressed();
                    Reader src(data);
                    src.withHeader() >> outline;
                }
                DE_NOTIFY_PUBLIC(MapOutline, i)
                {
                    i->mapOutlineReceived(svAddress, outline);
                }
            }
            catch (const Error &er)
            {
                LOG_NET_WARNING("MapOutline reply from %s was invalid: %s") << svAddress << er.asText();
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
    bool handleJoinResponse(const Block &reply)
    {
        if (reply.size() < 5 || reply != "Enter")
        {
            LOG_NET_WARNING("Server refused connection");
            LOGDEV_NET_WARNING("Received %i bytes instead of \"Enter\")")
                    << reply.size();
            self().disconnect();
            return false;
        }

        // We'll switch to joined mode.
        // Clients are allowed to send packets to the server.
        state = InGame;

        handshakeReceived = false;
        allowSending = true;
        netState.netGame = true;             // Allow sending/receiving of packets.
        netState.isServer = false;
        netState.isClient = true;

        // Call game's NetConnect.
        gx.NetConnect(false);

        DE_NOTIFY_PUBLIC(Join, i) i->networkGameJoined();

        // G'day mate!  The client is responsible for beginning the handshake.
        Cl_SendHello();

        return true;
    }

    void fetchFromMaster()
    {
        if (fetching) return;

        fetching = true;
        N_MAPost(MAC_REQUEST);
        N_MAPost(MAC_WAIT);
        deferred.enqueue([this] () { checkMasterReply(); });
    }

    void checkMasterReply()
    {
        DE_ASSERT(fetching);

        if (N_MADone())
        {
            fetching = false;

            fromMaster.clear();
            const int count = N_MasterGet(0, nullptr);
            for (int i = 0; i < count; i++)
            {
                ServerInfo info;
                N_MasterGet(i, &info);
                fromMaster.insert(info.address(), info);
            }

            notifyDiscoveryUpdate();
        }
        else
        {
            // Check again later.
            deferred.enqueue([this] () { checkMasterReply(); });
        }
    }

    bool prepareServerProfile(const Address &host)
    {
        serverProfile.reset();

        // Do we have information about this host?
        ServerInfo info;
        if (!self().foundServerInfo(host, info))
        {
            return false;
        }
        if (info.gameId().isEmpty() || info.packages().isEmpty())
        {
            // There isn't enough information here.
            return false;
        }

        serverProfile.reset(new GameProfile(info.name()));
        serverProfile->setGame(info.gameId());
        serverProfile->setUseGameRequirements(false); // server lists all packages (that affect gameplay)
        serverProfile->setPackages(info.packages());

        return true; // profile available
    }

    Servers allFound(const FoundMask &mask) const
    {
        Servers all;
        if (finder && mask.testFlag(LocalNetwork))
        {
            // Append the ones from the server finder.
            for (const Address &sv : finder->foundServers())
            {
                all.insert(sv, finder->messageFromServer(sv));
            }
        }
        if (mask.testFlag(MasterServer))
        {
            // Append from master (if available).
            for (auto i = fromMaster.begin(); i != fromMaster.end(); ++i)
            {
                all.insert(i->first, i->second);
            }
        }
        if (mask.testFlag(Direct))
        {
            for (auto i = discovered.begin(); i != discovered.end(); ++i)
            {
                all.insert(i->first, i->second);
            }
        }
        return all;
    }

    void reportError(const String &msg)
    {
        // Show the error message in a dialog box.
        MessageDialog *dlg = new MessageDialog;
        dlg->setDeleteAfterDismissed(true);
        dlg->title().setText("Cannot Join Game");
        dlg->message().setText(msg);
        dlg->buttons() << new DialogButtonItem(DialogWidget::Default | DialogWidget::Accept);
        ClientWindow::main().root().addOnTop(dlg);
        dlg->open(MessageDialog::Modal);
    }

    DE_PIMPL_AUDIENCES(Discovery, PingResponse, MapOutline, Join, Leave)
};

DE_AUDIENCE_METHODS(ServerLink, Discovery, PingResponse, MapOutline, Join, Leave)

ServerLink::ServerLink(Flags flags) : d(new Impl(this, flags))
{
    if (d->finder)
    {
        d->finder->audienceForUpdate() += [this](){ localServersFound(); };
    }
    audienceForPacketsReady() += [this](){ handleIncomingPackets(); };
    audienceForDisconnected() += [this](){ linkDisconnected(); };
}

PackageDownloader &ServerLink::packageDownloader()
{
    return d->downloader;
}

void ServerLink::clear()
{
    d->finder->clear();
    // TODO: clear all found servers
}

void ServerLink::connectToServerAndChangeGameAsync(const ServerInfo& info)
{
    // Automatically leave the current MP game.
    if (netState.netGame && netState.isClient)
    {
        disconnect();
    }

    // Forming the connection is a multi-step asynchronous procedure. The app's event
    // loop is running between each of these steps so the UI is never frozen. The first
    // step is to acquire the server's game profile.

    acquireServerProfileAsync(info.address(),
                              [this, info] (const GameProfile *serverProfile)
    {
        auto &win = ClientWindow::main();
        win.glActivate();

        if (!serverProfile)
        {
            // Hmm, oopsie?
            const String errorMsg =
                "Not enough information known about server " + info.address().asText();
            LOG_NET_ERROR("Failed to join: ") << errorMsg;
            d->reportError(errorMsg);
            return;
        }

        // Profile acquired! Figure out if the profile can be started locally.
        LOG_NET_MSG("Received server's game profile");
        LOG_NET_VERBOSE(serverProfile->toInfoSource());

        // If additional packages are configured, set up the ad-hoc profile with the
        // local additions.
        const GameProfile *joinProfile = serverProfile;
        const auto localPackages = serverProfile->game().localMultiplayerPackages();
        if (localPackages.count())
        {
            // Make sure the packages are available.
            for (const String &pkg : localPackages)
            {
                if (!PackageLoader::get().select(pkg))
                {
                    const String errorMsg =
                        Stringf("The configured local multiplayer "
                                       "package %s is unavailable.",
                                       Package::splitToHumanReadable(pkg).c_str());
                    LOG_NET_ERROR("Failed to join %s: ") << info.address() << errorMsg;
                    d->reportError(errorMsg);
                    return;
                }
            }

            auto &adhoc = DoomsdayApp::app().adhocProfile();
            adhoc = *serverProfile;
            adhoc.setPackages(serverProfile->packages() + localPackages);
            joinProfile = &adhoc;
        }

        // The server makes certain packages available for clients to download.
        d->downloader.mountServerRepository(info, [this, joinProfile, info] (const filesys::Link *)
        {
            // Now we know all the files that the server will be providing.
            // If we are missing any of the packages, download a copy from the server.

            LOG_RES_MSG("Received metadata about server files");

            const StringList neededPackages = joinProfile->unavailablePackages();
            LOG_RES_MSG("Packages needed to join: ")
                    << String::join(neededPackages, " ");

            // Show the download popup.
            auto *dlPopup = new FileDownloadDialog(d->downloader);
            dlPopup->setDeleteAfterDismissed(true);
            ClientWindow::main().root().addOnTop(dlPopup);
            dlPopup->open(DialogWidget::Modal);

            // Request contents of missing packages.
            d->downloader.download(neededPackages,
                                   [this, dlPopup, joinProfile, info] ()
            {
                auto &win = ClientWindow::main();
                win.glActivate();

                dlPopup->close();
                if (d->downloader.isCancelled())
                {
                    d->downloader.unmountServerRepository();
                    return;
                }

                if (!joinProfile->isPlayable())
                {
                    const String errorMsg =
                        Stringf("Server's game \"%s\" is not playable on this system. "
                                       "The following packages are unavailable:\n\n",
                                       info.gameId().c_str()) +
                        String::join(de::map(joinProfile->unavailablePackages(),
                                             Package::splitToHumanReadable),
                                     "\n");
                    LOG_NET_ERROR("Failed to join %s: ") << info.address() << errorMsg;
                    d->downloader.unmountServerRepository();
                    d->reportError(errorMsg);
                    return;
                }

                // Everything is finally good to go.

                BusyMode_FreezeGameForBusyMode();
                win.taskBar().close();
                DoomsdayApp::app().changeGame(*joinProfile, DD_ActivateGameWorker);
                connectHost(info.address());

                win.glDone();
            });

            // We must wait after the downloads have finished.
            // The user sees a popup while downloads are progressing, and has the
            // option of cancelling.
        });
    });
}

void ServerLink::acquireServerProfileAsync(const Address &address,
                                           const std::function<void (const GameProfile *)>& resultHandler)
{
    if (d->prepareServerProfile(address))
    {
        // We already know everything that is needed for the profile.
        d->deferred.enqueue([this, resultHandler] ()
        {
            DE_ASSERT(d->serverProfile.get() != nullptr);
            resultHandler(d->serverProfile.get());
        });
    }
    else
    {
        AbstractLink::connectHost(address);
        d->profileResultCallback = resultHandler;
        d->state = Discovering;
        LOG_NET_MSG("Querying server %s for full status") << address;
    }
}

void ServerLink::acquireServerProfileAsync(const String &domain,
                                           std::function<void (Address, const GameProfile *)> resultHandler)
{
    d->profileResultCallbackWithAddress = std::move(resultHandler);
    discover(domain);
    LOG_NET_MSG("Querying server %s for full status") << domain;
}

void ServerLink::requestMapOutline(const Address &address)
{
    AbstractLink::connectHost(address);
    d->state = QueryingMapOutline;
    LOG_NET_VERBOSE("Querying server %s for map outline") << address;
}

void ServerLink::ping(const Address &address)
{
    if (d->state != Pinging &&
        d->state != WaitingForPong)
    {
        AbstractLink::connectHost(address);
        d->state = Pinging;
    }
}

void ServerLink::connectDomain(const String &domain, TimeSpan timeout)
{
    LOG_AS("ServerLink::connectDomain");

    AbstractLink::connectDomain(domain, timeout);
    d->state = Joining;
}

void ServerLink::connectHost(const Address &address)
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

        DE_NOTIFY(Leave, i) i->networkGameLeft();

        LOG_NET_NOTE("Link to server %s disconnected") << address();
        d->downloader.unmountServerRepository();
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

void ServerLink::discover(const String &domain)
{
    AbstractLink::connectDomain(domain, 5.0 /*timeout*/);

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
    return d->allFound(mask).sizei();
}

List<Address> ServerLink::foundServers(FoundMask mask) const
{
    return map<List<Address>>(d->allFound(mask),
                              [](const Impl::Servers::value_type &v) { return v.first; });
}

bool ServerLink::isFound(const Address &host, FoundMask mask) const
{
    const Address addr = checkPort(host);
    return d->allFound(mask).contains(addr);
}

bool ServerLink::foundServerInfo(int index, ServerInfo &info, FoundMask mask) const
{
    const Impl::Servers all = d->allFound(mask);
    const auto          listed =
        map<List<Address>>(all, [](const Impl::Servers::value_type &v) { return v.first; });
    if (index < 0 || index >= listed.sizei()) return false;
    info = all[listed[index]];
    return true;
}

bool ServerLink::isServerOnLocalNetwork(const Address &host) const
{
    if (!d->finder) return host.isLocal(); // Best guess...
    const Address addr = checkPort(host);
    return d->finder->foundServers().contains(addr);
}

bool ServerLink::foundServerInfo(const de::Address &host, ServerInfo &info, FoundMask mask) const
{
    const Address addr = checkPort(host);
    const Impl::Servers all = d->allFound(mask);
    if (!all.contains(addr)) return false;
    info = all[addr];
    return true;
}

Packet *ServerLink::interpret(const Message &msg)
{
    return new BlockPacket(msg);
}

void ServerLink::initiateCommunications()
{
    switch (d->state)
    {
    case Discovering:
        // Ask for the serverinfo.
        *this << ByteRefArray("Info?", 5);
        d->state = WaitingForInfoResponse;
        break;

    case Pinging:
        *this << ByteRefArray("Ping?", 5);
        d->state = WaitingForPong;
        d->pingCounter = NUM_PINGS;
        d->pings.clear();
        d->pingTimer.start();
        break;

    case QueryingMapOutline:
        *this << ByteRefArray("MapOutline?", 11);
        d->state = WaitingForInfoResponse;
        break;

    case Joining: {
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
        *this << Stringf("Join %04x %s", SV_VERSION, pName.c_str());

        d->state = WaitingForJoinResponse;

        ClientWindow::main().glDone();
        break; }

    default:
        DE_ASSERT_FAIL("Initiate");
        break;
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
    for (;;)
    {
        // Only BlockPackets received (see interpret()).
        std::unique_ptr<BlockPacket> packet(static_cast<BlockPacket *>(nextPacket()));
        if (!packet) break;

        const Block &packetData = packet->block();

        switch (d->state)
        {
        case WaitingForInfoResponse:
            if (!d->handleInfoResponse(packetData)) return;
            break;

        case WaitingForJoinResponse:
            if (!d->handleJoinResponse(packetData)) return;
            break;

        case WaitingForPong:
            if (packetData.size() == 4 && packetData == "Pong" &&
                d->pingCounter-- > 0)
            {
                d->pings << TimeSpan(d->pingTimer.elapsedSeconds());
                *this << ByteRefArray("Ping?", 5);
                d->pingTimer.restart();
            }
            else
            {
                const Address svAddress = address();

                disconnect();

                // Notify about the average ping time.
                if (d->pings.count())
                {
                    TimeSpan average;
                    for (const TimeSpan i : d->pings) average += i;
                    average /= d->pings.count();

                    DE_NOTIFY(PingResponse, i)
                    {
                        i->pingResponse(svAddress, average);
                    }
                }
            }
            break;

        case InGame: {
            /// @todo The incoming packets should be handled immediately.

            // Post the data into the queue.
            netmessage_t *msg = reinterpret_cast<netmessage_t *>(M_Calloc(sizeof(netmessage_t)));

            msg->sender = 0; // the server
            msg->data = new byte[packetData.size()];
            memcpy(msg->data, packetData.data(), packetData.size());
            msg->size = packetData.size();
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

ServerLink &ServerLink::get() // static
{
    return ClientApp::serverLink();
}
