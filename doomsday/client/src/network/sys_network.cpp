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

#ifndef __CLIENT__
#  error "sys_network.cpp is only for __CLIENT__"
#endif

#include <errno.h>

#ifndef WIN32
#include <signal.h>
#endif

#include "de_platform.h"
#include "de_console.h"
#include "de_system.h"
#include "de_misc.h"

#include "dd_main.h"
#include "dd_def.h"
#include "network/net_main.h"
#include "network/net_event.h"
#include "network/net_demo.h"
#include "network/protocol.h"
#include "client/cl_def.h"
#include "map/p_players.h"

#include <de/c_wrapper.h> // using LegacyNetwork
#include <de/Socket>
#include <de/LegacyCore>
#include <de/LegacyNetwork>

#define MAX_NODES   1

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

typedef struct foundhost_s {
    boolean         valid;
    serverinfo_t    info;
    ipaddress_t       addr;
} foundhost_t;

static void N_IPToString(char *buf, ipaddress_t *ip);

char   *nptIPAddress = (char *) "";
int     nptIPPort = 0;          // This is the port *we* use to communicate.

netnode_t netNodes[MAX_NODES];

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

    // Let's forget about servers found earlier.
    located.valid = false;
}

/**
 * Shut down the low-level network interface. Called during engine
 * shutdown (not before).
 */
void N_SystemShutdown(void)
{
    if(netGame)
    {
        // We seem to be shutting down while a netGame is running.
        Con_Execute(CMDS_DDAY, "net disconnect", true, false);
    }

    // Any queued messages will be destroyed.
    N_ClearMessages();

    // Let's forget about servers found earlier.
    located.valid = false;
}

/**
 * Convert an IPaddress to a string.
 */
void N_IPToString(char *buf, ipaddress_t *ip)
{
    sprintf(buf, "%s:%i", ip->host, ip->port);
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
            Net_StringToServerInfo(Str_Text(line), &located.info);
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

    if(!port)
    {
        port = DEFAULT_TCP_PORT;
    }

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
boolean N_Connect(void)
{
    netnode_t *svNode = 0;
    foundhost_t *host;
    char buf[300], *pName;

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

/**
 * Called from "net info" (client-side).
 */
void N_PrintNetworkStatus(void)
{
    if(isClient)
    {
        char buf[80];
        N_IPToString(buf, &netNodes[0].addr);
        Con_Message("CLIENT: Connected to server at %s.\n", buf);
    }
    else
    {
        Con_Message("OFFLINE: Single-player mode.\n");
    }

    N_PrintBufferInfo();
}

void N_ListenNodes(void)
{
    N_ClientListenUnjoined();
    N_ClientListen();
}
