/** @file serversystem.cpp  Subsystem for tending to clients.
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

#include "serversystem.h"
#include "shellusers.h"
#include "remoteuser.h"
#include "server/sv_def.h"
#include "server/sv_frame.h"
#include "network/net_main.h"
#include "network/net_buf.h"
#include "network/net_event.h"
#include "network/monitor.h"
#include "con_main.h"
#include "dd_main.h"
#include "dd_loop.h"
#include "world/map.h"
#include "world/p_players.h"

#include <de/Address>
#include <de/Beacon>
#include <de/ByteRefArray>
#include <de/ListenSocket>
#include <de/TextApp>
#include <de/garbage.h>
#include <de/c_wrapper.h>

using namespace de;

int nptIPPort = 0; ///< Server TCP port (cvar).

static de::duint16 Server_ListenPort()
{
    return (!nptIPPort ? DEFAULT_TCP_PORT : nptIPPort);
}

static ServerSystem *serverSys; // Singleton.

DENG2_PIMPL(ServerSystem)
{
    /// Beacon for informing clients that a server is present.
    Beacon beacon;
    Time lastBeaconUpdateAt;

    ListenSocket *serverSock;

    QMap<Id, RemoteUser *> users;
    ShellUsers shellUsers;

    Instance(Public *i) : Base(i),
        beacon(DEFAULT_UDP_PORT),
        serverSock(0)
    {}

    ~Instance()
    {
        deinit();
    }

    bool isStarted() const
    {
        return serverSock != 0;
    }

    bool init(duint16 port)
    {
        LOG_INFO("Server listening on TCP port ") << port;

        deinit();

        // Open a listening TCP socket. It will accept client connections.
        if(!(serverSock = new ListenSocket(port)))
            return false;

        QObject::connect(serverSock, SIGNAL(incomingConnection()), thisPublic, SLOT(handleIncomingConnection()));

        // Update the beacon with the new port.
        beacon.start(port);
        return true;
    }

    void clearUsers()
    {
        // Clear the client nodes.
        foreach(RemoteUser *u, users.values())
        {
            delete u;
        }
        DENG2_ASSERT(users.isEmpty());
    }

    void deinit()
    {
        beacon.stop();

        // Close the listening socket.
        delete serverSock;
        serverSock = 0;

        clearUsers();
    }

    RemoteUser &findUser(Id const &id) const
    {
        DENG2_ASSERT(users.contains(id));
        return *users[id];
    }

    void updateBeacon(Clock const &clock)
    {
        if(lastBeaconUpdateAt.since() > 0.5)
        {
            lastBeaconUpdateAt = clock.time();

            // Update the status message in the server's presence beacon.
            if(serverSock && App_World().hasMap())
            {
                serverinfo_t info;
                Sv_GetInfo(&info);

                QScopedPointer<de::Record> rec(Sv_InfoToRecord(&info));
                de::Block msg;
                de::Writer(msg).withHeader() << *rec;
                beacon.setMessage(msg);
            }
        }
    }

    /**
     * The client is removed from the game immediately. This is used when
     * the server needs to terminate a client's connection abnormally.
     */
    void terminateNode(Id const &id)
    {
        if(id)
        {
            DENG2_ASSERT(users.contains(id));

            delete users[id];

            DENG2_ASSERT(!users.contains(id));
        }
    }

    void printStatus()
    {
        int i, first;

        if(serverSock)
        {
            Con_Message("SERVER: Listening on TCP port %i.", serverSock->port());
        }
        else
        {
            Con_Message("SERVER: No server socket open.");
        }

        first = true;
        for(i = 1; i < DDMAXPLAYERS; ++i)
        {
            client_t *cl = &clients[i];
            player_t *plr = &ddPlayers[i];
            if(cl->nodeID)
            {
                DENG2_ASSERT(users.contains(cl->nodeID));

                RemoteUser *user = users[cl->nodeID];
                if(first)
                {
                    Con_Message("P# Name:      Nd Jo Hs Rd Gm Age:");
                    first = false;
                }
                Con_Message("%2i %-10s %2i %c  %c  %c  %c  %f sec",
                            i, cl->name, cl->nodeID,
                            user->isJoined()? '*' : ' ',
                            cl->handshake? '*' : ' ',
                            cl->ready? '*' : ' ',
                            plr->shared.inGame? '*' : ' ',
                            Timer_RealSeconds() - cl->enterTime);
            }
        }
        if(first)
        {
            Con_Message("No clients connected.");
        }

        if(shellUsers.count())
        {
            Con_Message("%i shell user%s.",
                        shellUsers.count(),
                        shellUsers.count() == 1? "" : "s");
        }

        N_PrintBufferInfo();

        Con_Message("Configuration:");
        Con_Message("  Port for hosting games (net-ip-port): %i", Con_GetInteger("net-ip-port"));
        Con_Message("  Shell password (server-password): \"%s\"", netPassword);
    }
};

ServerSystem::ServerSystem() : d(new Instance(this))
{
    serverSys = this;
}

ServerSystem::~ServerSystem()
{
    d.reset();
    serverSys = 0;
}

void ServerSystem::start(duint16 port)
{
    d->init(port);
}

void ServerSystem::stop()
{
    d->deinit();
}

bool ServerSystem::isListening() const
{
    return d->isStarted();
}

void ServerSystem::terminateNode(Id const &id)
{
    d->terminateNode(id);
}

RemoteUser &ServerSystem::user(Id const &id) const
{
    if(!d->users.contains(id))
    {
        throw IdError("ServerSystem::user", "User " + id.asText() + " does not exist");
    }
    return *d->users[id];
}

bool ServerSystem::isUserAllowedToJoin(RemoteUser &/*user*/) const
{
    // If the server is full, attempts to connect are canceled.
    if(Sv_GetNumConnected() >= svMaxPlayers)
        return false;

    return true;
}

void ServerSystem::convertToShellUser(RemoteUser *user)
{
    LOG_AS("convertToShellUser");

    Socket *socket = user->takeSocket();

    LOG_DEBUG("Remote user %s converted to shell user") << user->id();
    user->deleteLater();

    d->shellUsers.add(new ShellUser(socket));
}

void ServerSystem::timeChanged(Clock const &clock)
{
    if(Sys_IsShuttingDown())
        return; // Shouldn't run this while shutting down.

    Garbage_Recycle();

    // Adjust loop rate depending on whether players are in game.
    int i, count = 0;
    for(i = 1; i < DDMAXPLAYERS; ++i)
        if(ddPlayers[i].shared.inGame) count++;

    DENG2_TEXT_APP->loop().setRate(count? 35 : 3);

    Loop_RunTics();

    // Update clients at regular intervals.
    Sv_TransmitFrame();

    d->updateBeacon(clock);

    /// @todo There's no need to queue packets via net_buf, just handle
    /// them right away.
    Sv_GetPackets();

    /// @todo Kick unjoined nodes who are silent for too long.
}

void ServerSystem::handleIncomingConnection()
{
    LOG_AS("ServerSystem");
    forever
    {
        Socket *sock = d->serverSock->accept();
        if(!sock) break;

        RemoteUser *user = new RemoteUser(sock);
        connect(user, SIGNAL(userDestroyed()), this, SLOT(userDestroyed()));
        d->users.insert(user->id(), user);

        // Immediately handle pending messages, if there are any.
        user->handleIncomingPackets();
    }
}

void ServerSystem::userDestroyed()
{
    RemoteUser *u = static_cast<RemoteUser *>(sender());

    LOG_AS("ServerSystem");
    LOG_VERBOSE("Removing user %s") << u->id();

    d->users.remove(u->id());

    LOG_DEBUG("%i remote users and %i shell users remain")
            << d->users.size() << d->shellUsers.count();
}

void ServerSystem::printStatus()
{
    d->printStatus();
}

ServerSystem &App_ServerSystem()
{
    DENG2_ASSERT(serverSys != 0);
    return *serverSys;
}

//---------------------------------------------------------------------------

void Server_Register(void)
{
    C_VAR_INT("net-ip-port", &nptIPPort, CVF_NO_MAX, 0, 0);

#ifdef _DEBUG
    C_CMD("netfreq", NULL, NetFreqs);
#endif
}

boolean N_ServerOpen(void)
{
    serverSys->start(Server_ListenPort());

    // The game module may have something that needs doing before we
    // actually begin.
    if(gx.NetServerStart)
        gx.NetServerStart(true);

    Sv_StartNetGame();

    // The game DLL might want to do something now that the
    // server is started.
    if(gx.NetServerStart)
        gx.NetServerStart(false);

    if(masterAware)
    {
        // Let the master server know that we are running a public server.
        N_MasterAnnounceServer(true);
    }

    return true;
}

boolean N_ServerClose(void)
{
    if(!serverSys->isListening()) return true;

    if(masterAware)
    {
        // Bye-bye, master server.
        N_MAClear();
        N_MasterAnnounceServer(false);
    }

    if(gx.NetServerStop)
        gx.NetServerStop(true);

    Net_StopGame();
    Sv_StopNetGame();

    if(gx.NetServerStop)
        gx.NetServerStop(false);

    serverSys->stop();
    return true;
}

void N_PrintNetworkStatus(void)
{
    serverSys->printStatus();
}
