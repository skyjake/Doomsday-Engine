/** @file sys_network.cpp Low-level network socket routines.
 * @ingroup network
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2006-2007 Jamie Jones <jamie_jones_au@yahoo.com.au>
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
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

// HEADER FILES ------------------------------------------------------------

#include <errno.h>

#ifndef WIN32
#include <signal.h>
#endif

#include "de_platform.h"
#include "de_console.h"
#include "de_system.h"
#include "de_misc.h"

#include "dd_main.h"
#include "network/net_main.h"
#include "network/net_event.h"
#ifdef __CLIENT__
#  include "network/net_demo.h"
#endif
#include "network/protocol.h"
#include "client/cl_def.h"
#include "server/sv_def.h"
#include "map/p_players.h"

#include <de/c_wrapper.h> // using LegacyNetwork

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

    // The node is owned by a client in the game.  This becomes true
    // when the client issues the JOIN request.
    boolean hasJoined;

    // This is the client's remote address.
    ipaddress_t addr;

    expectedresponder_t expectedResponder;
} netnode_t;

typedef struct foundhost_s {
    boolean         valid;
    serverinfo_t    info;
    ipaddress_t       addr;
} foundhost_t;

static void N_IPToString(char *buf, ipaddress_t *ip);

char   *nptIPAddress = (char *) "";
int     nptIPPort = 0;          // This is the port *we* use to communicate.
int     defaultTCPPort = DEFAULT_TCP_PORT;

// Operating mode of the currently active service provider.
boolean netIsActive = false;
boolean netServerMode = false;

static int serverSock;
static netnode_t netNodes[MAX_NODES];
static int sockSet;
static int joinedSockSet;
static foundhost_t located;

void N_Register(void)
{
    C_VAR_CHARPTR("net-ip-address", &nptIPAddress, 0, 0, 0);
    C_VAR_INT("net-ip-port", &nptIPPort, CVF_NO_MAX, 0, 0);

#ifdef _DEBUG
    C_CMD("netfreq", NULL, NetFreqs);
#endif
}

/**
 * Initialize the low-level network subsystem. This is called always
 * during startup (via Sys_Init()).
 */
void N_SystemInit(void)
{
    memset(netNodes, 0, sizeof(netNodes));
}

/**
 * Shut down the low-level network interface. Called during engine
 * shutdown (not before).
 */
void N_SystemShutdown(void)
{
    if(netGame)
    {
#ifdef __CLIENT__
        if(isClient) N_Disconnect();
#endif
#ifdef __SERVER__
        N_ServerClose();
#endif
    }

    N_ShutdownService();
}

/**
 * Convert an IPaddress to a string.
 */
void N_IPToString(char *buf, ipaddress_t *ip)
{
    sprintf(buf, "%s:%i", ip->host, ip->port);
}

/**
 * Initialize the chosen service provider each in server or client
 * mode.  If a service provider has already been initialized, it will
 * be shut down first.  Returns true if successful.
 */
boolean N_InitService(boolean inServerMode)
{
    uint16_t port;

    if(N_IsAvailable() && netServerMode == inServerMode)
    {
        // Nothing to change.
        return true;
    }

    // Get rid of the currently active service provider.
    N_ShutdownService();

    if(inServerMode)
    {
        port = (!nptIPPort ? defaultTCPPort : nptIPPort);

        Con_Message("Listening on TCP port %i.\n", port);

        // Open a listening TCP socket. It will accept client connections.
        if(!(serverSock = LegacyNetwork_OpenServerSocket(port)))
            return false;

        // Allocate socket sets, which we'll use for listening to the
        // client sockets.
        sockSet = LegacyNetwork_NewSocketSet();
        joinedSockSet = LegacyNetwork_NewSocketSet();
    }
    else
    {
        // Let's forget about servers found earlier.
        located.valid = false;
    }

    // Success.
    netIsActive = true;
    netServerMode = inServerMode;

    return true;
}

/**
 * Shut down the TCP/IP network services.
 */
void N_ShutdownService(void)
{
    uint        i;

    if(!N_IsAvailable())
        return;                 // Nothing to do.

    if(netGame)
    {
        // We seem to be shutting down while a netGame is running.
        Con_Execute(CMDS_DDAY, isServer ? "net server close" : "net disconnect", true, false);
    }

    // Any queued messages will be destroyed.
    N_ClearMessages();

    if(netServerMode)
    {
        // Close the listening socket.
        LegacyNetwork_Close(serverSock);
        serverSock = 0;

        // Clear the client nodes.
        for(i = 1; i < MAX_NODES; ++i)
            N_TerminateNode(i);

        // Free the socket set.
        LegacyNetwork_DeleteSocketSet(sockSet);
        LegacyNetwork_DeleteSocketSet(joinedSockSet);
        sockSet = 0;
        joinedSockSet = 0;
    }
    else
    {
        // Let's forget about servers found earlier.
        located.valid = false;
    }

    netIsActive = false;
    netServerMode = false;
}

/**
 * Returns true if the low-level network routines have been initialized
 * and are expected to be working.
 */
boolean N_IsAvailable(void)
{
    return netIsActive;
}

/**
 * Returns true if the internet is available.
 */
boolean N_UsingInternet(void)
{
    return netIsActive;
}

boolean N_GetHostInfo(int index, struct serverinfo_s *info)
{
    if(!located.valid || index != 0)
    {
        return false;
    }
    memcpy(info, &located.info, sizeof(*info));
    return true;
}

int N_GetHostCount(void)
{
    return located.valid ? 1 : 0;
}

const char *N_GetProtocolName(void)
{
    return "TCP/IP";
}

int N_GetNodeSocket(nodeid_t id)
{
    if(id >= MAX_NODES) return 0;
    return netNodes[id].sock;
}

boolean N_HasNodeJoined(nodeid_t id)
{
    if(id >= MAX_NODES) return 0;
    return netNodes[id].hasJoined;
}

/**
 * Returns the player name associated with the given network node.
 */
boolean N_GetNodeName(nodeid_t id, char *name)
{
    if(!netNodes[id].sock)
    {
        strcpy(name, "-unknown-");
        return false;
    }
    strcpy(name, netNodes[id].name);
    return true;
}

/**
 * The client is removed from the game immediately. This is used when
 * the server needs to terminate a client's connection abnormally.
 */
void N_TerminateNode(nodeid_t id)
{
    netnode_t *node = &netNodes[id];
    netevent_t netEvent;

    if(!node->sock)
        return;  // There is nothing here...

    if(netServerMode && node->hasJoined)
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

/**
 * Registers a new TCP socket as a client node. There can only be a limited
 * number of nodes at a time. This is only used by a server.
 */
static boolean registerNewSocket(int sock)
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

            DEBUG_VERBOSE2_Message(("N_RegisterNewSocket: Socket #%i registered as node %i.\n", sock, i));

            return true;
        }
    }
    return false;
}

/**
 * A network node wishes to become a real client. @return @c true if we allow
 * this.
 */
static boolean joinNode(nodeid_t id, int clientProtocol, const char *name)
{
    netnode_t *node;
    netevent_t netEvent;

    if(clientProtocol != SV_VERSION)
        return false; // Incompatible.

    // If the server is full, attempts to connect are canceled.
    if(Sv_GetNumConnected() >= svMaxPlayers)
        return false;

    node = &netNodes[id];

    // The address where we should be sending data.
    LegacyNetwork_GetPeerAddress(node->sock, node->addr.host, sizeof(node->addr.host), &node->addr.port);

    if(verbose)
    {
        char buf[80];
        N_IPToString(buf, &node->addr);
        Con_Message("Node %i listens at %s.\n", id, buf);
    }

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

void N_ClientHandleResponseToInfoQuery(int nodeId, const byte *data, int size)
{
    netnode_t* svNode = &netNodes[nodeId];

    // Close the connection; that was all the information we need.
    LegacyNetwork_Close(svNode->sock);
    svNode->sock = 0;

    // Did we receive what we expected to receive?
    if(size >= 5 && !strncmp((const char*)data, "Info\n", 5))
    {
        const char *ch;
        ddstring_t *line;
        ddstring_t* response = Str_New();

        // Make a null-terminated copy of the response text.
        Str_PartAppend(response, (const char*)data, 0, size);

        located.valid = true;

        // Convert the string into a serverinfo_s.
        line = Str_New();
        ch = Str_Text(response);
        do
        {
            ch = Str_GetLine(line, ch);
            Sv_StringToInfo(Str_Text(line), &located.info);
        }
        while(*ch);

        Str_Delete(line);
        Str_Delete(response);

        // Show the information in the console.
        Con_Message("%i server%s been found.\n", N_GetHostCount(), N_GetHostCount() != 1 ? "s have" : " has");
        Net_PrintServerInfo(0, NULL);
        Net_PrintServerInfo(0, &located.info);
    }
    else
    {
        located.valid = false;
        Con_Message("Reply from %s (port %i) was invalid.\n", svNode->addr.host, svNode->addr.port);
    }

    memset(svNode, 0, sizeof(*svNode));
}

/**
 * @todo Socket connection shouldn't block while forming the connection.
 * Instead, there should be a notification after the connection has opened or
 * failed.
 */
boolean N_LookForHosts(const char *address, int port, expectedresponder_t responder)
{
    netnode_t* svNode = &netNodes[0];

    // We must be a client.
    if(!N_IsAvailable() || netServerMode)
        return false;

    if(!port)
        port = DEFAULT_TCP_PORT;

    memset(svNode, 0, sizeof(*svNode));

    // Get rid of previous findings.
    memset(&located, 0, sizeof(located));
    strncpy(located.addr.host, address, sizeof(located.addr.host) - 1);
    located.addr.port = port;
    memcpy(&svNode->addr, &located.addr, sizeof(located.addr));

    // I say, anyone there?
    svNode->sock = LegacyNetwork_Open(located.addr.host, located.addr.port);
    if(!svNode->sock)
    {
        Con_Message("No reply from %s (port %i).\n", address, port);
        memset(svNode, 0, sizeof(*svNode));
        return false;
    }

    // Send an INFO query.
    LegacyNetwork_Send(svNode->sock, "Info?", 5);
    Con_Message("Sent info query to %s (port %i).\n", address, port);

    svNode->expectedResponder = (responder? responder : N_ClientHandleResponseToInfoQuery);
    return true;
}

#ifdef __CLIENT__

/**
 * Handles the server's response to a client's join request.
 */
void N_ClientHandleResponseToJoin(int nodeId, const byte* data, int size)
{
    struct netnode_s* svNode = &netNodes[nodeId];
    const char* buf = (const char*) data;

    if(size < 5 || strncmp(buf, "Enter", 5))
    {
        Con_Message("Server refused connection (received %i bytes).\n", size);
        N_Disconnect();
        return;
    }

    // Put the server's socket in a socket set so we may listen to it.
    joinedSockSet = LegacyNetwork_NewSocketSet();
    LegacyNetwork_SocketSet_Add(joinedSockSet, svNode->sock);

    // We'll switch to joined mode.
    svNode->expectedResponder = NULL;

    // Clients are allowed to send packets to the server.
    svNode->hasJoined = true;

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
}

/**
 * Connect a client to the server identified with 'index'.  We enter
 * clientside mode during this routine.
 */
boolean N_Connect(int index)
{
    netnode_t *svNode = 0;
    foundhost_t *host;
    char buf[300], *pName;

    if(!N_IsAvailable() || netServerMode || index != 0)
        return false;

    Demo_StopPlayback();

    // Tell the Game that a connection is about to happen.
    gx.NetConnect(true);

    host = &located;

    // We'll use node number zero for all communications.
    svNode = &netNodes[0];
    svNode->expectedResponder = NULL;
    if(!(svNode->sock = LegacyNetwork_Open(host->addr.host, host->addr.port)))
    {
        N_IPToString(buf, &host->addr);
        Con_Message("No reply from %s.\n", buf);
        return false;
    }
    memcpy(&svNode->addr, &located.addr, sizeof(ipaddress_t));

    // Connect by issuing: "Join (myname)"
    pName = playerName;
    if(!pName || !pName[0])
    {
        pName = (char *) "Anonymous";
    }
    sprintf(buf, "Join %04x %s", SV_VERSION, pName);
    LegacyNetwork_Send(svNode->sock, buf, (int) strlen(buf));

    svNode->expectedResponder = N_ClientHandleResponseToJoin;
    return true;
}

/**
 * Disconnect from the server.
 */
boolean N_Disconnect(void)
{
    netnode_t *svNode = &netNodes[0];

    if(!N_IsAvailable())
        return false;

    // Tell the Game that a disconnection is about to happen.
    if(gx.NetDisconnect)
        gx.NetDisconnect(true);

    // Close the control connection.  This will let the server know
    // that we are no more.
    LegacyNetwork_Close(svNode->sock);
    memset(svNode, 0, sizeof(*svNode));

    LegacyNetwork_DeleteSocketSet(joinedSockSet);
    joinedSockSet = 0;

    Net_StopGame();

    // Tell the Game that the disconnection is now complete.
    if(gx.NetDisconnect)
        gx.NetDisconnect(false);

    return true;
}

#endif // __CLIENT__

boolean N_ServerOpen(void)
{
    if(!isDedicated)
    {
        Con_Message("Server can only be started in dedicated mode! (run with -dedicated)\n");
        return false;
    }

#ifdef __CLIENT__
    Demo_StopPlayback();
#endif

    // Let's make sure the correct service provider is initialized
    // in server mode.
    if(!N_InitService(true))
    {
        Con_Message("Failed to initialize server mode.\n");
        return false;
    }

    // The game module may have something that needs doing before we
    // actually begin.
    if(gx.NetServerStart)
        gx.NetServerStart(true);

    Sv_StartNetGame();

    // The game DLL might want to do something now that the
    // server is started.
    if(gx.NetServerStart)
        gx.NetServerStart(false);

    if(masterAware && N_UsingInternet())
    {
        // Let the master server know that we are running a public server.
        N_MasterAnnounceServer(true);
    }
    return true;
}

boolean N_ServerClose(void)
{
    if(!N_IsAvailable())
        return false;

    if(masterAware && N_UsingInternet())
    {
        // Bye-bye, master server.
        N_MAClear();
        N_MasterAnnounceServer(false);
    }
    if(gx.NetServerStop)
        gx.NetServerStop(true);
    Net_StopGame();
    Sv_StopNetGame();

    // Exit server mode.
    N_InitService(false);

    if(gx.NetServerStop)
        gx.NetServerStop(false);
    return true;
}

/**
 * Validate and process the command, which has been sent by a remote agent.
 *
 * If the command is invalid, the node is immediately closed. We don't have
 * time to fool around with badly behaving clients.
 */
static boolean serverHandleNodeRequest(nodeid_t node, const char *command, int length)
{
    int sock = netNodes[node].sock;
    serverinfo_t info;
    ddstring_t msg;

    // If the command is too long, it'll be considered invalid.
    if(length >= 256)
    {
        N_TerminateNode(node);
        return false;
    }

    // Status query?
    if(length == 5 && !strncmp(command, "Info?", 5))
    {
        Sv_GetInfo(&info);
        Str_Init(&msg);
        Str_Appendf(&msg, "Info\n");
        Sv_InfoToString(&info, &msg);

        DEBUG_VERBOSE_Message(("serverHandleNodeRequest: Sending: %s\n", Str_Text(&msg)));

        LegacyNetwork_Send(sock, Str_Text(&msg), (int) Str_Length(&msg));
        Str_Free(&msg);
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
            N_TerminateNode(node);
        }
        Str_Free(&name);
    }
    /*
    else if(length == 3 && !strcmp(command, "Bye"))
    {
        // Request for the server to terminate the connection.
        N_TerminateNode(node);
    }*/
    else
    {
        // Too bad, scoundrel! Goodbye.
        N_TerminateNode(node);
        return false;
    }

    // Everything was OK.
    return true;
}

void N_ServerListenUnjoinedNodes(void)
{
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
                serverHandleNodeRequest(i, (const char*) message, size);
                LegacyNetwork_FreeBuffer(message);
            }

            if(node->sock && LegacyNetwork_IsDisconnected(node->sock))
            {
                // Close this socket & node.
                Con_Message("Connection to client closed on node %i.\n", i);
                N_TerminateNode(i);
                continue;
            }
        }
    }
}

#ifdef __CLIENT__

/**
 * Handles messages from the server when the client is connected but has not
 * yet joined to game.
 */
void N_ClientListenUnjoined(void)
{
    netnode_t *svNode = &netNodes[0];

    if(!svNode->sock || svNode->hasJoined)
        return;

    if(LegacyNetwork_IsDisconnected(svNode->sock))
    {
        // Connection to the server has been closed.
        N_Disconnect();
        return;
    }

    // Any incoming messages?
    while(LegacyNetwork_BytesReady(svNode->sock))
    {
        int size = 0;
        byte* data = LegacyNetwork_Receive(svNode->sock, &size);
        if(data)
        {
            if(svNode->expectedResponder)
            {
                // The responder may change during the execution of this.
                svNode->expectedResponder(0, data, size);
            }
            else
            {
                Con_Message("Unexpected message from server (%i bytes), ignoring.\n", size);
            }
            LegacyNetwork_FreeBuffer(data);
        }
        else
        {
            // An error!
            Con_Message("Connection closed.\n");
            N_Disconnect();
        }
    }
}

#endif // __CLIENT__

#ifdef __SERVER__

void N_ServerListenJoinedNodes(void)
{
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
                   (LegacyNetwork_BytesReady(node->sock) && !Protocol_Receive(i)))
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

#endif // __SERVER__

#ifdef __CLIENT__

void N_ClientListen(void)
{
    if(!joinedSockSet) return;

    // Clientside listening.  On clientside, the socket set only
    // includes the server's socket.
    while(LegacyNetwork_SocketSet_Activity(joinedSockSet))
    {
        if(!Protocol_Receive(0))
        {
            netevent_t nev;
            nev.id = 0;
            nev.type = NE_END_CONNECTION;
            N_NEPost(&nev);

            // No point in continuing with the listener.
            break;
        }
    }
}

#endif

void N_ListenNodes(void)
{
    if(netServerMode)
    {
#ifdef __SERVER__
        // This is only for the server.
        N_ServerListenUnjoinedNodes();
        N_ServerListenJoinedNodes();
#endif
    }
    else
    {
#ifdef __CLIENT__
        N_ClientListenUnjoined();
        N_ClientListen();
#endif
    }
}

/**
 * Called from "net info".
 */
void N_PrintNetworkStatus(void)
{
    int i, first;

    if(isServer && !serverSock)
    {
        Con_Message("OFFLINE: Single-player mode.\n");
    }
    else if(isServer)
    {
        if(isDedicated) Con_Message("DEDICATED ");
        Con_Message("SERVER: ");
        if(serverSock)
        {
            char buf[80];
            N_IPToString(buf, &netNodes[0].addr);
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
    }
    if(isClient)
    {
        char buf[80];
        N_IPToString(buf, &netNodes[0].addr);
        Con_Message("CLIENT: Connected to server at %s.\n", buf);
    }
    N_PrintBufferInfo();

    Con_Message("Configuration:\n");
    Con_Message("  port for hosting games (net-ip-port): %i\n", Con_GetInteger("net-ip-port"));
}
