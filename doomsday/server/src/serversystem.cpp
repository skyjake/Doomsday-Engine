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
#include "server/sv_def.h"
#include "network/net_main.h"
#include "network/net_event.h"
#include "network/monitor.h"
#include "con_main.h"
#include "map/gamemap.h"
#include "map/p_players.h"

#include <de/Beacon>
#include <de/ByteRefArray>
#include <de/c_wrapper.h>
#include <de/LegacyCore>
#include <de/LegacyNetwork>

#include <QCryptographicHash>

using namespace de;

/// @todo Get rid of these:

#define MAX_NODES                   32

typedef struct ipaddress_s {
    char host[256];
    unsigned short port;
} ipaddress_t;

/**
 * On serverside, each client has its own network node. A node
 * represents the TCP connection between the client and the server. On
 * clientside, the node zero is used always.
 */
typedef struct netnode_s {
    int sock;
    char name[256];

    /// The node is owned by a client in the game.  This becomes true
    // when the client issues the JOIN request.
    boolean hasJoined;

    /// This is the client's remote address.
    ipaddress_t addr;

    /// The node is connecting from the local host.
    boolean isFromLocalHost;

    expectedresponder_t expectedResponder;
} netnode_t;

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

    ShellUsers shellUsers;

    int serverSock;
    int sockSet;
    int joinedSockSet;
    netnode_t netNodes[MAX_NODES];

    Time lastBeaconUpdateAt;

    Instance(Public *i) : Base(i),
        beacon(DEFAULT_UDP_PORT),
        serverSock(0),
        sockSet(0),
        joinedSockSet(0)
    {
        memset(netNodes, 0, sizeof(netNodes));
    }

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
        if(!(serverSock = LegacyNetwork_OpenServerSocket(port)))
            return false;

        // Allocate socket sets, which we'll use for listening to the
        // client sockets.
        sockSet = LegacyNetwork_NewSocketSet();
        joinedSockSet = LegacyNetwork_NewSocketSet();

        // Update the beacon with the new port.
        beacon.start(port);
        return true;
    }

    void deinit()
    {
        beacon.stop();

        if(serverSock)
        {
            // Close the listening socket.
            LegacyNetwork_Close(serverSock);
            serverSock = 0;
        }

        // Clear the client nodes.
        for(int i = 1; i < MAX_NODES; ++i)
            terminateNode(i);

        // Free the socket set.
        LegacyNetwork_DeleteSocketSet(sockSet);
        LegacyNetwork_DeleteSocketSet(joinedSockSet);
        sockSet = 0;
        joinedSockSet = 0;
    }

    /**
     * Registers a new TCP socket as a client node. There can only be a limited
     * number of nodes at a time. This is only used by a server.
     */
    bool registerNewSocket(int sock)
    {
        uint        i;
        netnode_t  *node;

        // Find a free node.
        for(i = 1, node = netNodes + 1; i < MAX_NODES; ++i, node++)
        {
            if(!node->sock)
            {
                // This'll do.
                node->sock = sock;

                // We don't know the name yet.
                memset(node->name, 0, sizeof(node->name));

                // Add this socket to the set of client sockets.
                LegacyNetwork_SocketSet_Add(sockSet, sock);

                // The address where we should be sending data.
                LegacyNetwork_GetPeerAddress(sock, node->addr.host, sizeof(node->addr.host), &node->addr.port);

                node->isFromLocalHost = LegacyNetwork_IsLocal(sock);

                LOG_VERBOSE("Socket #%i from %s registered as node %i (local:%b)")
                        << sock << node->addr.host << i << node->isFromLocalHost;
                return true;
            }
        }
        return false;
    }

    /**
     * A network node wishes to become a real client. @return @c true if we allow
     * this.
     */
    bool joinNode(nodeid_t id, int clientProtocol, const char *name)
    {
        netnode_t *node;
        netevent_t netEvent;

        if(clientProtocol != SV_VERSION)
            return false; // Incompatible.

        // If the server is full, attempts to connect are canceled.
        if(Sv_GetNumConnected() >= svMaxPlayers)
            return false;

        node = &netNodes[id];

        // Convert the network node into a real client node.
        node->hasJoined = true;

        // Move it to the joined socket set.
        LegacyNetwork_SocketSet_Remove(sockSet, node->sock);
        LegacyNetwork_SocketSet_Add(joinedSockSet, node->sock);

        // @todo We should use more discretion with the name. It has
        // been provided by an untrusted source.
        strncpy(node->name, name, sizeof(node->name) - 1);

        // Inform the higher levels of this occurence.
        netEvent.type = NE_CLIENT_ENTRY;
        netEvent.id = id;
        N_NEPost(&netEvent);

        return true;
    }

    void switchNodeToShellMode(nodeid_t node)
    {
        Socket *socket = de::LegacyCore::instance().network()
                .takeSocket(netNodes[node].sock);

        // The socket is no longer maintained by the legacy network.
        memset(&netNodes[node], 0, sizeof(netnode_t));

        shellUsers.add(new ShellUser(socket));
    }

    /**
     * Validate and process the command, which has been sent by a remote agent.
     *
     * If the command is invalid, the node is immediately closed. We don't have
     * time to fool around with badly behaving clients.
     */
    bool handleNodeRequest(nodeid_t node, const char *command, int length)
    {
        LOG_AS("handleNodeRequest");

        int sock = netNodes[node].sock;
        serverinfo_t info;
        ddstring_t msg;

        // If the command is too long, it'll be considered invalid.
        if(length >= 256)
        {
            terminateNode(node);
            return false;
        }

        // Status query?
        if(length == 5 && !strncmp(command, "Info?", 5))
        {
            Sv_GetInfo(&info);
            Str_Init(&msg);
            Str_Appendf(&msg, "Info\n");
            Sv_InfoToString(&info, &msg);

            LOG_DEBUG("Info reply:\n%s") << Str_Text(&msg);

            LegacyNetwork_Send(sock, Str_Text(&msg), Str_Length(&msg));
            Str_Free(&msg);
        }
        else if(length >= 5 && !strncmp(command, "Shell", 5))
        {
            if(length == 5)
            {
                // Password is not required for connections from the local computer.
                if(strlen(netPassword) > 0 && !netNodes[node].isFromLocalHost)
                {
                    // Need to ask for a password, too.
                    LegacyNetwork_Send(sock, "Psw?", 4);
                    return true;
                }
            }
            else if(length > 5)
            {
                // A password was included.
                QByteArray supplied(command + 5, length - 5);
                QByteArray pwd(netPassword, strlen(netPassword));
                if(supplied != QCryptographicHash::hash(pwd, QCryptographicHash::Sha1))
                {
                    // Wrong!
                    terminateNode(node);
                    return false;
                }
            }

            // This node will switch to shell mode: ownership of the socket is
            // passed to a ShellUser.
            switchNodeToShellMode(node);
        }
        else if(length >= 10 && !strncmp(command, "Join ", 5) && command[9] == ' ')
        {
            ddstring_t name;
            int protocol = 0;
            char protoStr[5];

            strncpy(protoStr, command + 5, 4);
            protocol = strtol(protoStr, 0, 16);

            // Read the client's name and convert the network node into an actual
            // client. Here we also decide if the client's protocol is compatible
            // with ours.
            Str_Init(&name);
            Str_PartAppend(&name, command, 10, length - 10);
            if(joinNode(node, protocol, Str_Text(&name)))
            {
                // Successful! Send a reply.
                LegacyNetwork_Send(sock, "Enter", 5);
            }
            else
            {
                // Couldn't join the game, so close the connection.
                LegacyNetwork_Send(sock, "Bye", 3);
                terminateNode(node);
            }
            Str_Free(&name);
        }
        else
        {
            // Too bad, scoundrel! Goodbye.
            LOG_WARNING("Received an invalid request from node %i.") << node;
            terminateNode(node);
            return false;
        }

        // Everything was OK.
        return true;
    }

    void listenUnjoinedNodes(void)
    {
        /// @todo Refactor this to respond to incoming messages rather than polling.

        int sock;

        if(!serverSock) return;

        // Any incoming connections on the listening socket?
        while((sock = LegacyNetwork_Accept(serverSock)) != 0)
        {
            // A new client is attempting to connect. Let's try to
            // register the new socket as a network node.
            if(!registerNewSocket(sock))
            {
                // There was a failure, close the socket.
                LegacyNetwork_Close(sock);
            }
        }

        // Any activity on the client sockets? (Don't wait.)
        while(LegacyNetwork_SocketSet_Activity(sockSet))
        {
            int i;
            for(i = 0; i < MAX_NODES; ++i)
            {
                netnode_t* node = netNodes + i;
                if(node->hasJoined || !node->sock) continue;

                // Does this socket have incoming messages?
                while(node->sock && LegacyNetwork_BytesReady(node->sock))
                {
                    int size = 0;
                    byte* message = LegacyNetwork_Receive(node->sock, &size);
                    handleNodeRequest(i, (const char*) message, size);
                    LegacyNetwork_FreeBuffer(message);
                }

                if(node->sock && LegacyNetwork_IsDisconnected(node->sock))
                {
                    // Close this socket & node.
                    LOG_INFO("Connection to client closed on node %i.") << i;
                    terminateNode(i);
                    continue;
                }
            }
        }
    }

    void listenJoinedNodes(void)
    {
        /// @todo Refactor this to respond to incoming messages rather than polling.

        if(!joinedSockSet) return;

        // Any activity on the client sockets?
        while(LegacyNetwork_SocketSet_Activity(joinedSockSet))
        {
            int i;
            for(i = 0; i < MAX_NODES; ++i)
            {
                netnode_t* node = netNodes + i;

                // Does this socket have got any activity?
                if(node->hasJoined)
                {
                    if(LegacyNetwork_IsDisconnected(node->sock) ||
                       (LegacyNetwork_BytesReady(node->sock) &&
                        !Protocol_Receive(i)))
                    {
                        netevent_t nev;
                        nev.type = NE_TERMINATE_NODE;
                        nev.id = i;
                        N_NEPost(&nev);
                    }
                }
            }

            // Should we go take care of the events?
            if(N_NEPending()) break;
        }
    }

    void updateBeacon(Clock const &clock)
    {
        if(lastBeaconUpdateAt.since() > 0.5)
        {
            lastBeaconUpdateAt = clock.time();

            // Update the status message in the server's presence beacon.
            if(serverSock && theMap)
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
    void terminateNode(nodeid_t id)
    {
        netnode_t *node = &netNodes[id];
        netevent_t netEvent;

        if(!node->sock)
            return;  // There is nothing here...

        if(node->hasJoined)
        {
            // Let the client know.
            Msg_Begin(PSV_SERVER_CLOSE);
            Msg_End();
            Net_SendBuffer(N_IdentifyPlayer(id), 0);

            // This causes a network event.
            netEvent.type = NE_CLIENT_EXIT;
            netEvent.id = id;
            N_NEPost(&netEvent);

            LegacyNetwork_SocketSet_Remove(joinedSockSet, node->sock);
        }

        // Remove the node from the set of active sockets.
        LegacyNetwork_SocketSet_Remove(sockSet, node->sock);

        // Close the socket and forget everything about the node.
        LegacyNetwork_Close(node->sock);
        memset(node, 0, sizeof(*node));
    }

    void printStatus()
    {
        int i, first;

        Con_Message("SERVER: ");
        if(serverSock)
        {
            char buf[80];
            sprintf(buf, "%s:%i", netNodes[0].addr.host, netNodes[0].addr.port);
            Con_Message("Open at %s.\n", buf);
        }
        else
        {
            Con_Message("No server socket open.\n");
        }
        first = true;
        for(i = 1; i < DDMAXPLAYERS; ++i)
        {
            client_t *cl = &clients[i];
            player_t *plr = &ddPlayers[i];
            netnode_t* node = &netNodes[cl->nodeID];
            if(cl->nodeID)
            {
                if(first)
                {
                    Con_Message("P# Name:      Nd Jo Hs Rd Gm Age:\n");
                    first = false;
                }
                Con_Message("%2i %-10s %2i %c  %c  %c  %c  %f sec\n",
                            i, cl->name, cl->nodeID,
                            node->hasJoined? '*' : ' ',
                            cl->handshake? '*' : ' ',
                            cl->ready? '*' : ' ',
                            plr->shared.inGame? '*' : ' ',
                            Timer_RealSeconds() - cl->enterTime);
            }
        }
        if(first)
        {
            Con_Message("No clients connected.\n");
        }

        if(shellUsers.count())
        {
            Con_Message("%i connected shell user%s.\n",
                        shellUsers.count(),
                        shellUsers.count() == 1? "" : "s");
        }

        N_PrintBufferInfo();

        Con_Message("Configuration:\n");
        Con_Message("  port for hosting games (net-ip-port): %i\n", Con_GetInteger("net-ip-port"));
        Con_Message("  shell password (server-password): \"%s\"\n", netPassword);
    }
};

ServerSystem::ServerSystem() : d(new Instance(this))
{
    serverSys = this;
}

ServerSystem::~ServerSystem()
{
    delete d;
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

bool ServerSystem::isRunning() const
{
    return d->isStarted();
}

void ServerSystem::terminateNode(nodeid_t id)
{
    d->terminateNode(id);
}

void ServerSystem::timeChanged(Clock const &clock)
{
    d->updateBeacon(clock);

    d->listenUnjoinedNodes();
    d->listenJoinedNodes();

    Sv_GetPackets();

    /// @todo Kick unjoined nodes who are silent for too long.
}

void ServerSystem::printStatus()
{
    d->printStatus();
}

String ServerSystem::nodeName(nodeid_t node) const
{
    if(!d->netNodes[node].sock)
    {
        return "-unknown-";
    }
    return d->netNodes[node].name;
}

int ServerSystem::nodeLegacySocket(nodeid_t node) const
{
    if(node >= MAX_NODES) return 0;
    return d->netNodes[node].sock;
}

bool ServerSystem::hasNodeJoined(nodeid_t node) const
{
    if(node >= MAX_NODES) return 0;
    return d->netNodes[node].hasJoined;
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
    if(!serverSys->isRunning()) return true;

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

/**
 * Called from "net info" (server-side).
 */
void N_PrintNetworkStatus(void)
{
    serverSys->printStatus();
}

/// @todo Get rid of this.
void N_TerminateNode(nodeid_t id)
{
    serverSys->terminateNode(id);
}

String N_GetNodeName(nodeid_t id)
{
    return serverSys->nodeName(id);
}

int N_GetNodeSocket(nodeid_t id)
{
    return serverSys->nodeLegacySocket(id);
}

boolean N_HasNodeJoined(nodeid_t id)
{
    return serverSys->hasNodeJoined(id);
}
