/**
 * @file sys_network.c
 * Low-level network socket routines. @ingroup network
 *
 * @see @ref sysNetwork
 *
 * @authors Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2012 Daniel Swanson <danij@dengine.net>
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

#ifdef DENG_SDLNET_DUMMY
#  include "sdlnet_dummy.h"
#else
#  include <SDL_net.h>
#endif

#include <errno.h>

#ifndef WIN32
#include <signal.h>
#endif

#include "de_base.h"
#include "de_network.h"
#include "de_console.h"
#include "de_system.h"
#include "de_misc.h"
#include "de_play.h"

#include "huffman.h"
#include "zipfile.h"
#include "protocol.h"

#define MAX_NODES                   32

/**
 * On serverside, each client has its own network node. A node
 * represents the TCP connection between the client and the server. On
 * clientside, the node zero is used always.
 */
typedef struct netnode_s {
    TCPsocket       sock;
    char            name[256];

    // The node is owned by a client in the game.  This becomes true
    // when the client issues the JOIN request.
    boolean         hasJoined;

    // This is the client's remote address.
    IPaddress       addr;
} netnode_t;

typedef struct foundhost_s {
    boolean         valid;
    serverinfo_t    info;
    IPaddress       addr;
} foundhost_t;

void N_IPToString(char *buf, IPaddress *ip);

static int C_DECL N_JoinedListenerThread(void* param);

size_t  maxDatagramSize = PROTOCOL_MAX_DATAGRAM_SIZE;

char   *nptIPAddress = "";
int     nptIPPort = 0;          // This is the port *we* use to communicate.
int     defaultTCPPort = DEFAULT_TCP_PORT;

// Operating mode of the currently active service provider.
boolean netIsActive = false;
boolean netServerMode = false;

static TCPsocket serverSock;
static netnode_t netNodes[MAX_NODES];
static SDLNet_SocketSet sockSet;
static SDLNet_SocketSet joinedSockSet;
static mutex_t mutexJoinedSockSet;
static thread_t joinedListener;
static volatile boolean stopJoinedListener;
static foundhost_t located;

void N_Register(void)
{
    C_VAR_CHARPTR("net-ip-address", &nptIPAddress, 0, 0, 0);
    C_VAR_INT("net-ip-port", &nptIPPort, CVF_NO_MAX, 0, 0);

#ifdef _DEBUG
    C_CMD("netfreq", NULL, NetFreqs);
#endif
}

static void N_StartJoinedListener(void)
{
    Con_Message("N_StartJoinedListener.\n");

    stopJoinedListener = false;
    joinedListener = Sys_StartThread(N_JoinedListenerThread, 0);
}

static void N_StopJoinedListener(void)
{
    if(joinedListener)
    {
        Con_Message("N_StopJoinedListener.\n");

        stopJoinedListener = true;
        Sys_WaitThread(joinedListener);
        joinedListener = 0;
    }
}

/**
 * Initialize the low-level network subsystem. This is called always
 * during startup (via Sys_Init()).
 */
void N_SystemInit(void)
{
#if 0 // obsolete
    // The MTU can be customized.
    if(ArgCheckWith("-mtu", 1))
    {
        maxDatagramSize = (size_t) strtol(ArgNext(), NULL, 0);
        Con_Message("N_SystemInit: Custom MTU: %lu bytes.\n", (unsigned long) maxDatagramSize);
    }
#endif

    Protocol_Init();
}

/**
 * Shut down the low-level network interface. Called during engine
 * shutdown (not before).
 */
void N_SystemShutdown(void)
{
    if(netGame)
    {
        if(isClient)
            N_Disconnect();
        else
            N_ServerClose();
    }

    Protocol_Shutdown();
    N_ShutdownService();
}

/**
 * Convert an IPaddress to a string.
 */
void N_IPToString(char *buf, IPaddress *ip)
{
    uint host = SDLNet_Read32(&ip->host);

    sprintf(buf, "%i.%i.%i.%i:%i", host >> 24, (host >> 16) & 0xff,
            (host >> 8) & 0xff, host & 0xff, SDLNet_Read16(&ip->port));
}

/**
 * Initialize the chosen service provider each in server or client
 * mode.  If a service provider has already been initialized, it will
 * be shut down first.  Returns true if successful.
 */
boolean N_InitService(boolean inServerMode)
{
    IPaddress ip;
    uint16_t  port;

    if(N_IsAvailable() && netServerMode == inServerMode)
    {
        // Nothing to change.
        return true;
    }

    // Get rid of the currently active service provider.
    N_ShutdownService();

    if(!SDLNet_Init())
    {
        VERBOSE(Con_Message("N_InitService: SDLNet_Init OK\n"));
    }
    else
    {
        Con_Message("N_InitService: SDLNet_Init %s\n", SDLNet_GetError());
    }

    // Mutex for the socket set.
    mutexJoinedSockSet = Sys_CreateMutex("sockSet");

    if(inServerMode)
    {
        port = (!nptIPPort ? defaultTCPPort : nptIPPort);

        Con_Message("N_InitService: Listening TCP socket on port %i.\n", port);

        // Open a listening TCP socket. It will accept client
        // connections.
        if(SDLNet_ResolveHost(&ip, NULL, port))
        {
            Con_Message("N_InitService: %s\n", SDLNet_GetError());
            return false;
        }
        if(!(serverSock = SDLNet_TCP_Open(&ip)))
        {
            Con_Message("N_InitService: %s\n", SDLNet_GetError());
            return false;
        }
        memcpy(&netNodes[0].addr, &ip, sizeof(ip));

        // Allocate socket sets, which we'll use for listening to the
        // client sockets.
        if(!(sockSet = SDLNet_AllocSocketSet(MAX_NODES)))
        {
            Con_Message("N_InitService: %s\n", SDLNet_GetError());
            return false;
        }
        if(!(joinedSockSet = SDLNet_AllocSocketSet(MAX_NODES)))
        {
            Con_Message("N_InitService: %s\n", SDLNet_GetError());
            return false;
        }

        // We can start the listener immediately.
        N_StartJoinedListener();
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

    N_StopJoinedListener();

    if(netServerMode)
    {
        // Close the listening socket.
        SDLNet_TCP_Close(serverSock);
        serverSock = NULL;

        // Clear the client nodes.
        for(i = 0; i < MAX_NODES; ++i)
            N_TerminateNode(i);

        // Free the socket set.
        SDLNet_FreeSocketSet(sockSet);
        SDLNet_FreeSocketSet(joinedSockSet);
        sockSet = NULL;
        joinedSockSet = NULL;
    }
    else
    {
        // Let's forget about servers found earlier.
        located.valid = false;
    }

    Sys_DestroyMutex(mutexJoinedSockSet);
    mutexJoinedSockSet = 0;

    SDLNet_Quit();

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
        return false;
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

void* N_GetNodeSocket(nodeid_t id)
{
    if(id >= MAX_NODES) return 0;
    return (void*) netNodes[id].sock;
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

        Sys_Lock(mutexJoinedSockSet);
        SDLNet_TCP_DelSocket(joinedSockSet, node->sock);
        Sys_Unlock(mutexJoinedSockSet);
    }

    // Remove the node from the set of active sockets.
    SDLNet_TCP_DelSocket(sockSet, node->sock);

    // Close the socket and forget everything about the node.
    SDLNet_TCP_Close(node->sock);

    memset(node, 0, sizeof(*node));
}

/**
 * Registers a new TCP socket as a client node.  There can only be a
 * limited number of nodes at a time.  This is only used by a server.
 */
static boolean N_RegisterNewSocket(TCPsocket sock)
{
    uint        i;
    netnode_t  *node;
    boolean     found;

    // Find a free node.
    for(i = 1, node = netNodes + 1, found = false; i < MAX_NODES && !found; ++i, node++)
    {
        if(!node->sock)
        {
            // This'll do.
            node->sock = sock;

            // We don't know the name yet.
            memset(node->name, 0, sizeof(node->name));

            // Add this socket to the set of client sockets.
            SDLNet_TCP_AddSocket(sockSet, sock);

            found = true;
        }
    }
    return found;
}

/**
 * A network node wishes to become a real client. Returns true if we
 * allow this.
 */
static boolean N_JoinNode(nodeid_t id, /*Uint16 port,*/ const char *name)
{
    netnode_t *node;
    netevent_t netEvent;
    IPaddress *ip;

    // If the server is full, attempts to connect are canceled.
    if(Sv_GetNumConnected() >= svMaxPlayers)
        return false;

    node = &netNodes[id];

    // The address where we should be sending data.
    if(!(ip = SDLNet_TCP_GetPeerAddress(node->sock)))
    {
        // This is a strange situation...
        return false;
    }
    memcpy(&node->addr, ip, sizeof(IPaddress));
    //SDLNet_Write16(port, &node->addr.port);

    if(verbose)
    {
        char buf[80];
        N_IPToString(buf, &node->addr);
        Con_Message("N_JoinNode: Node %i listens at %s.\n", id, buf);
    }

    // Convert the network node into a real client node.
    node->hasJoined = true;

    // Move it to the joined socket set.
    SDLNet_TCP_DelSocket(sockSet, node->sock);
    Sys_Lock(mutexJoinedSockSet);
    SDLNet_TCP_AddSocket(joinedSockSet, node->sock);
    Sys_Unlock(mutexJoinedSockSet);

    // \fixme We should use more discretion with the name. It has
    // been provided by an untrusted source.
    strncpy(node->name, name, sizeof(node->name) - 1);

    // Inform the higher levels of this occurence.
    netEvent.type = NE_CLIENT_ENTRY;
    netEvent.id = id;
    N_NEPost(&netEvent);

    return true;
}

/**
 * Maybe it would be wisest to run this in a separate thread?
 */
boolean N_LookForHosts(const char *address, int port)
{
    TCPsocket   sock;
    char        buf[256];
    ddstring_t *response;
    boolean     isDone;

    // We must be a client.
    if(!N_IsAvailable() || netServerMode)
        return false;

    if(!port)
        port = DEFAULT_TCP_PORT;

    // Get rid of previous findings.
    memset(&located, 0, sizeof(located));

    // Let's determine the address we will be looking into.
    SDLNet_ResolveHost(&located.addr, address, port);

    // I say, anyone there?
    sock = SDLNet_TCP_Open(&located.addr);
    if(!sock)
    {
        Con_Message("N_LookForHosts: No reply from %s (port %i).\n", address, port);
        return false;
    }

    // Send an INFO query.
    SDLNet_TCP_Send(sock, "INFO\n", 5);

    Con_Message("Send INFO query.\n");

    // Let's listen to the reply.
    memset(buf, 0, sizeof(buf));
    response = Str_New();
    isDone = false;
    while(!isDone)
    {
        int result;

        if(!strstr(Str_Text(response), "END\n"))
        {
            memset(buf, 0, sizeof(buf));
            Con_Message("Waiting for response.\n");
            result = SDLNet_TCP_Recv(sock, buf, sizeof(buf) - 1);

            if(result > 0)
            {
                Str_Appendf(response, "%s", buf);
                Con_Message("Append to response: %s.\n", buf);
            }
            else // Terminated.
            {
                isDone = true;
                Con_Message("result <= 0 (%i)\n", result);
            }
        }
        else
            isDone = true;
    }

    // Close the connection; that was all the information we need.
    SDLNet_TCP_Close(sock);

    // Did we receive what we expected to receive?
    if(strstr(Str_Text(response), "BEGIN\n"))
    {
        const char *ch;
        ddstring_t *line;

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
        Con_Message("%i server%s been found.\n", N_GetHostCount(),
                   N_GetHostCount() != 1 ? "s have" : " has");
        Net_PrintServerInfo(0, NULL);
        Net_PrintServerInfo(0, &located.info);
        return true;
    }
    else
    {
        Str_Delete(response);
        Con_Message("N_LookForHosts: Reply from %s (port %i) was invalid.\n",
                    address, port);
        return false;
    }
}

/**
 * Connect a client to the server identified with 'index'.  We enter
 * clientside mode during this routine.
 */
boolean N_Connect(int index)
{
    netnode_t *svNode;
    foundhost_t *host;
    char    buf[128], *pName;

    if(!N_IsAvailable() || netServerMode || index != 0)
        return false;

    Demo_StopPlayback();

    // Call game DLL's NetConnect.
    gx.NetConnect(true);

    host = &located;

    // We'll use node number zero for all communications.
    svNode = &netNodes[0];
    if(!(svNode->sock = SDLNet_TCP_Open(&host->addr)))
    {
        N_IPToString(buf, &host->addr);
        Con_Message("N_Connect: No reply from %s.\n", buf);
        return false;
    }
    memcpy(&svNode->addr, &located.addr, sizeof(IPaddress));

    // Connect by issuing: "JOIN (my-protocol-version) (myname)"
    pName = playerName;
    if(!pName || !pName[0])
        pName = "Anonymous";
    sprintf(buf, "JOIN %04x %s\n", SV_VERSION, pName);
    SDLNet_TCP_Send(svNode->sock, buf, (int) strlen(buf));

    VERBOSE(Con_Message("N_Connect: %s", buf));

    // What is the reply?
    memset(buf, 0, sizeof(buf));
    if(SDLNet_TCP_Recv(svNode->sock, buf, 64) <= 0 || strncmp(buf, "ENTER ", 6))
    {
        SDLNet_TCP_Close(svNode->sock);
        memset(svNode, 0, sizeof(svNode));
        Con_Message("N_Connect: Server refused connection.\n");
        if(buf[0])
            Con_Message("  Reply: %s", buf);
        return false;
    }

    VERBOSE(Con_Message("  Server responds: %s", buf));

    // Put the server's socket in a socket set so we may listen to it.
    joinedSockSet = SDLNet_AllocSocketSet(1);
    SDLNet_TCP_AddSocket(joinedSockSet, svNode->sock);

    // Clients are allowed to send packets to the server.
    svNode->hasJoined = true;

    // Start the TCP receiver thread.
    N_StartJoinedListener();

    allowSending = true;
    handshakeReceived = false;
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

/**
 * Disconnect from the server.
 */
boolean N_Disconnect(void)
{
    netnode_t *svNode;

    if(!N_IsAvailable())
        return false;

    // Tell the Game that a disconnection is about to happen.
    if(gx.NetDisconnect)
        gx.NetDisconnect(true);

    Net_StopGame();

    // Tell the Game that the disconnection is now complete.
    if(gx.NetDisconnect)
        gx.NetDisconnect(false);

    // This'll prevent the sending of further packets.
    svNode = &netNodes[0];
    svNode->hasJoined = false;

    // Stop the TCP receiver thread.
    N_StopJoinedListener();

    // Close the control connection.  This will let the server know
    // that we are no more.
    SDLNet_TCP_Close(svNode->sock);

    SDLNet_FreeSocketSet(joinedSockSet);
    joinedSockSet = NULL;

    N_ClearMessages();

    return true;
}

boolean N_ServerOpen(void)
{
    if(!isDedicated)
    {
        Con_Message("N_ServerOpen: Server can only be started in dedicated mode! (run with -dedicated)\n");
        return false;
    }

    if(!N_IsAvailable())
        return false;

    Demo_StopPlayback();

    // Let's make sure the correct service provider is initialized
    // in server mode.
    if(!N_InitService(true))
    {
        Con_Message("N_ServerOpen: Failed to initialize server mode.\n");
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
 * Validate and process the command, which has been sent by a remote
 * agent. Anyone is free to connect to a server using telnet and issue
 * queries.
 *
 * If the command is invalid, the node is immediately closed. We don't
 * have time to fool around with badly behaving clients.
 */
static boolean N_DoNodeCommand(nodeid_t node, const char *input, int length)
{
    char command[80], *ch, buf[256];
    const char *in;
    TCPsocket sock = netNodes[node].sock;
    serverinfo_t info;
    ddstring_t msg;
    int protocolVersion;

    // If the command is too long, it'll be considered invalid.
    if(length >= 80)
    {
        N_TerminateNode(node);
        return false;
    }

    // Make a copy of the command.
    memset(command, 0, sizeof(command));
    for(ch = command, in = input;
        *in && *in != '\r' && *in != '\n' && in - input < length;)
        *ch++ = *in++;

    Con_Message("N_DoNodeCommand: %s\n", command);

    // Status query?
    if(!strcmp(command, "INFO"))
    {
        int result = 0;
        Sv_GetInfo(&info);
        Str_Init(&msg);
        Str_Appendf(&msg, "BEGIN\n");
        Sv_InfoToString(&info, &msg);
        Str_Appendf(&msg, "END\n");
        Con_Message("Sending: %s\n", Str_Text(&msg));
        result = SDLNet_TCP_Send(sock, Str_Text(&msg), (int) Str_Length(&msg));
        Con_Message("Result = %i\n", result);
        Str_Free(&msg);
    }
    else if(!strncmp(command, "JOIN ", 5) && length > 10)
    {
        // Which protocol version does the client use?
        memset(buf, 0, 5);
        strncpy(buf, command + 5, 4);
        protocolVersion = strtol(buf, &ch, 16);
        if(*ch || protocolVersion != SV_VERSION)
        {
            Con_Message("Connection denied: Server protocol v%i differs from client protocol v%i.\n",
                        SV_VERSION, protocolVersion);
            N_TerminateNode(node);
            return false;
        }

        // Read the client's name and convert the network node into a
        // real client network node (which has a transmitter).
        if(N_JoinNode(node, command + 10))
        {
            // Successful! Send a reply.
            sprintf(buf, "ENTER %04x\n", SV_VERSION);
            SDLNet_TCP_Send(sock, buf, (int) strlen(buf));
        }
        else
        {
            // Couldn't join the game, so close the connection.
            SDLNet_TCP_Send(sock, "BYE\n", 4);
            N_TerminateNode(node);
        }
    }
    else if(!strcmp(command, "TIME"))
    {
        sprintf(buf, "%.3f\n", Sys_GetSeconds());
        SDLNet_TCP_Send(sock, buf, (int) strlen(buf));
    }
    else if(!strcmp(command, "BYE"))
    {
        // Request for the server to terminate the connection.
        N_TerminateNode(node);
    }
    else
    {
        // Too bad, scoundrel! Goodbye.
        SDLNet_TCP_Send(sock, "Huh?\n", 5);
        N_TerminateNode(node);
        return false;
    }

    // Everything was OK.
    return true;
}

void N_ListenUnjoinedNodes(void)
{
    TCPsocket sock;

    if(!netServerMode)
    {
        // This is only for the server.
        return;
    }

    // Any incoming connections on the listening socket?
    while((sock = SDLNet_TCP_Accept(serverSock)) != NULL)
    {
        // A new client is attempting to connect. Let's try to
        // register the new socket as a network node.
        if(!N_RegisterNewSocket(sock))
        {
            // There was a failure, close the socket.
            SDLNet_TCP_Close(sock);
        }
    }

    // Any activity on the client sockets? (Don't wait.)
    if(SDLNet_CheckSockets(sockSet, 0) > 0)
    {
        char buf[256];
        int i, result;
        for(i = 0; i < MAX_NODES; ++i)
        {
            netnode_t* node = netNodes + i;
            if(node->hasJoined || !node->sock) continue;

            // Does this socket have got any activity?
            if(SDLNet_SocketReady(node->sock))
            {
                result = SDLNet_TCP_Recv(node->sock, buf, sizeof(buf));
                if(result <= 0)
                {
                    // Close this socket & node.
                    Con_Message("N_ListenUnjoinedNodes: Connection closed on node %i.\n", i);
                    N_TerminateNode(i);
                }
                else
                {
                    /** \fixme Read into a buffer, execute when newline
                    * received.
                    *
                    * Process the command; we will need to answer, or
                    * do something else.
                    */
                    N_DoNodeCommand(i, buf, result);
                }
            }
        }
    }
}

/**
 * TCP sockets receiver thread for joined nodes.
 */
static int C_DECL N_JoinedListenerThread(void* param)
{
    while(!stopJoinedListener || !joinedSockSet)
    {
        if(netServerMode)
        {
            Sys_Lock(mutexJoinedSockSet);

            // Any activity on the client sockets?
            if(SDLNet_CheckSockets(joinedSockSet, 10) > 0)
            {
                int i;
                Sys_Unlock(mutexJoinedSockSet);
                for(i = 0; i < MAX_NODES; ++i)
                {
                    netnode_t* node = netNodes + i;

                    // Does this socket have got any activity?
                    if(node->hasJoined && SDLNet_SocketReady(node->sock))
                    {
                        if(!Protocol_Receive(i))
                        {
                            netevent_t nev;
                            nev.type = NE_TERMINATE_NODE;
                            nev.id = i;
                            N_NEPost(&nev);
                        }
                    }
                }
            }
            else
            {
                Sys_Unlock(mutexJoinedSockSet);
            }
        }
        else
        {
            Sys_Lock(mutexJoinedSockSet);

            // Clientside listening.  On clientside, the socket set only
            // includes the server's socket.
            if(SDLNet_CheckSockets(joinedSockSet, 10) > 0)
            {   
                Sys_Unlock(mutexJoinedSockSet);

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
            else
            {
                Sys_Unlock(mutexJoinedSockSet);
            }
        }

        Sys_Sleep(1);
    }
    return 0;
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
                            Sys_GetRealSeconds() - cl->enterTime);
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
