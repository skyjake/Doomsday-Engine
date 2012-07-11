/**\file net_main.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2012 Daniel Swanson <danij@dengine.net>
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

/**
 * Client/server networking.
 *
 * Player number zero is always the server.
 * In single-player games there is only the server present.
 */

// HEADER FILES ------------------------------------------------------------

#include <stdlib.h>             // for atoi()

#include "de_base.h"
#include "de_console.h"
#include "de_edit.h"
#include "de_system.h"
#include "de_network.h"
#include "de_play.h"
#include "de_graphics.h"
#include "de_misc.h"
#include "de_ui.h"

#include "rend_bias.h"
#include "rend_console.h"
#include "r_lgrid.h"
#include "blockmapvisual.h"

// MACROS ------------------------------------------------------------------

#define OBSOLETE                CVF_NO_ARCHIVE|CVF_HIDE // Old ccmds.

// The threshold is the average ack time * mul.
#define ACK_THRESHOLD_MUL       1.5f

// Never wait a too short time for acks.
#define ACK_MINIMUM_THRESHOLD   50

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

D_CMD(Login); // in cl_main.c
D_CMD(Logout); // in sv_main.c
D_CMD(Ping); // in net_ping.c

void    R_DrawLightRange(void);
int     Sv_GetRegisteredMobj(pool_t *, thid_t, mobjdelta_t *);

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

D_CMD(Connect);
D_CMD(Chat);
D_CMD(Kick);
D_CMD(MakeCamera);
D_CMD(Net);
D_CMD(SetConsole);
D_CMD(SetName);
D_CMD(SetTicks);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

char   *serverName = "Doomsday";
char   *serverInfo = "Multiplayer Host";
char   *playerName = "Player";
int     serverData[3];          // Some parameters passed to master server.

client_t clients[DDMAXPLAYERS];   // All network data for the players.

int     netGame; // true if a netGame is in progress
int     isServer; // true if this computer is an open server.
int     isClient; // true if this computer is a client

// Gotframe is true if a frame packet has been received.
int     gotFrame = false;

boolean firstNetUpdate = true;

byte    monitorMsgQueue = false;
byte    netShowLatencies = false;
byte    netDev = false;
byte    netDontSleep = false;
byte    netTicSync = true;
float   netConnectTime;
//int     netCoordTime = 17;
float   netConnectTimeout = 10;
float   netSimulatedLatencySeconds = 0;

// Local packets are stored into this buffer.
boolean reboundPacket;
netbuffer_t reboundStore;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static int coordTimer = 0;

// CODE --------------------------------------------------------------------

void Net_Register(void)
{
    // Cvars
    C_VAR_BYTE("net-queue-show", &monitorMsgQueue, 0, 0, 1);
    C_VAR_BYTE("net-dev", &netDev, 0, 0, 1);
#ifdef _DEBUG
    C_VAR_FLOAT("net-dev-latency", &netSimulatedLatencySeconds, CVF_NO_MAX, 0, 0);
#endif
    C_VAR_BYTE("net-nosleep", &netDontSleep, 0, 0, 1);
    C_VAR_CHARPTR("net-master-address", &masterAddress, 0, 0, 0);
    C_VAR_INT("net-master-port", &masterPort, 0, 0, 65535);
    C_VAR_CHARPTR("net-master-path", &masterPath, 0, 0, 0);
    C_VAR_CHARPTR("net-name", &playerName, 0, 0, 0);

    // Cvars (client)
    //C_VAR_INT("client-pos-interval", &netCoordTime, CVF_NO_MAX, 0, 0);
    C_VAR_FLOAT("client-connect-timeout", &netConnectTimeout, CVF_NO_MAX,
                0, 0);

    // Cvars (server)
    C_VAR_CHARPTR("server-name", &serverName, 0, 0, 0);
    C_VAR_CHARPTR("server-info", &serverInfo, 0, 0, 0);
    C_VAR_INT("server-public", &masterAware, 0, 0, 1);
    C_VAR_CHARPTR("server-password", &netPassword, 0, 0, 0);
    C_VAR_BYTE("server-latencies", &netShowLatencies, 0, 0, 1);
    C_VAR_INT("server-frame-interval", &frameInterval, CVF_NO_MAX, 0, 0);
    C_VAR_INT("server-player-limit", &svMaxPlayers, 0, 0, DDMAXPLAYERS);

    // Ccmds
    C_CMD_FLAGS("chat", NULL, Chat, CMDF_NO_NULLGAME);
    C_CMD_FLAGS("chatnum", NULL, Chat, CMDF_NO_NULLGAME);
    C_CMD_FLAGS("chatto", NULL, Chat, CMDF_NO_NULLGAME);
    C_CMD_FLAGS("conlocp", "i", MakeCamera, CMDF_NO_NULLGAME);
    C_CMD_FLAGS("connect", NULL, Connect, CMDF_NO_NULLGAME|CMDF_NO_DEDICATED);
    C_CMD_FLAGS("kick", "i", Kick, CMDF_NO_NULLGAME);
    C_CMD_FLAGS("login", NULL, Login, CMDF_NO_NULLGAME);
    C_CMD_FLAGS("logout", "", Logout, CMDF_NO_NULLGAME);
    C_CMD_FLAGS("net", NULL, Net, CMDF_NO_NULLGAME);
    C_CMD_FLAGS("ping", NULL, Ping, CMDF_NO_NULLGAME);
    C_CMD_FLAGS("say", NULL, Chat, CMDF_NO_NULLGAME);
    C_CMD_FLAGS("saynum", NULL, Chat, CMDF_NO_NULLGAME);
    C_CMD_FLAGS("sayto", NULL, Chat, CMDF_NO_NULLGAME);
    C_CMD("setname", "s", SetName);
    C_CMD("setcon", "i", SetConsole);
    C_CMD("settics", "i", SetTicks);

    N_Register();
}

void Net_Init(void)
{
    int i;

    for(i = 0; i < DDMAXPLAYERS; ++i)
    {
        memset(clients + i, 0, sizeof(clients[i]));
        clients[i].viewConsole = -1;
        Net_AllocClientBuffers(i);
    }

    memset(&netBuffer, 0, sizeof(netBuffer));
    netBuffer.headerLength = netBuffer.msg.data - (byte *) &netBuffer.msg;
    // The game is always started in single-player mode.
    netGame = false;
}

void Net_Shutdown(void)
{
    netGame = false;
    N_Shutdown();
    Net_DestroyArrays();
}

const char* Net_GetPlayerName(int player)
{
    return clients[player].name;
}

ident_t Net_GetPlayerID(int player)
{
    if(!clients[player].connected)
        return 0;

    return clients[player].id;
}

/**
 * Sends the contents of the netBuffer.
 */
void Net_SendBuffer(int toPlayer, int spFlags)
{
    assert(!Msg_BeingWritten()); // Must finish writing before calling this.

    // Don't send anything during demo playback.
    if(playback)
        return;

    netBuffer.player = toPlayer;

    // A rebound packet?
    if(spFlags & SPF_REBOUND)
    {
        reboundStore = netBuffer;
        reboundPacket = true;
        return;
    }

    Demo_WritePacket(toPlayer);

    // Can we send the packet?
    if(spFlags & SPF_DONT_SEND)
        return;

    // Send the packet to the network.
    N_SendPacket(spFlags);
}

/**
 * @return         @C false, if there are no packets waiting.
 */
boolean Net_GetPacket(void)
{
    if(reboundPacket) // Local packets rebound.
    {
        netBuffer = reboundStore;
        netBuffer.player = consolePlayer;
        //netBuffer.cursor = netBuffer.msg.data;
        reboundPacket = false;
        return true;
    }

    if(playback)
    {   // We're playing a demo. This overrides all other packets.
        return Demo_ReadPacket();
    }

    if(!netGame)
    {   // Packets cannot be received.
        return false;
    }

    if(!N_GetPacket())
        return false;

    // Are we recording a demo?
    if(isClient && clients[consolePlayer].recording)
        Demo_WritePacket(consolePlayer);

    // Reset the cursor for Msg_* routines.
    //netBuffer.cursor = netBuffer.msg.data;

    return true;
}

Smoother* Net_PlayerSmoother(int player)
{
    if(player < 0 || player >= DDMAXPLAYERS)
        return 0;

    return clients[player].smoother;
}

void Net_SendPlayerInfo(int srcPlrNum, int destPlrNum)
{
    size_t nameLen;

    assert(srcPlrNum >= 0 && srcPlrNum < DDMAXPLAYERS);
    nameLen = strlen(clients[srcPlrNum].name);

#ifdef _DEBUG
    Con_Message("Net_SendPlayerInfo: src=%i dest=%i name=%s\n",
                srcPlrNum, destPlrNum, clients[srcPlrNum].name);
#endif

    Msg_Begin(PKT_PLAYER_INFO);
    Writer_WriteByte(msgWriter, srcPlrNum);
    Writer_WriteUInt16(msgWriter, nameLen);
    Writer_Write(msgWriter, clients[srcPlrNum].name, nameLen);
    Msg_End();
    Net_SendBuffer(destPlrNum, 0);
}

/**
 * This is the public interface of the message sender.
 */
void Net_SendPacket(int to_player, int type, const void* data, size_t length)
{
    unsigned int flags = 0;

#ifndef DENG_WRITER_TYPECHECK
    Msg_Begin(type);
    if(data) Writer_Write(msgWriter, data, length);
    Msg_End();
#else
    assert(length <= NETBUFFER_MAXSIZE);
    netBuffer.msg.type = type;
    netBuffer.length = length;
    if(data) memcpy(netBuffer.msg.data, data, length);
#endif

    if(isClient)
    {   // As a client we can only send messages to the server.
        Net_SendBuffer(0, flags);
    }
    else
    {   // The server can send packets to any player.
        // Only allow sending to the sixteen possible players.
        Net_SendBuffer(to_player & DDSP_ALL_PLAYERS ? NSP_BROADCAST
                       : (to_player & 0xf), flags);
    }
}

/**
 * Prints the message in the console.
 */
void Net_ShowChatMessage(int plrNum, const char* message)
{
    const char* fromName = (plrNum > 0? clients[plrNum].name : "[sysop]");
    const char* sep      = (plrNum > 0? ":"                  : "");
    Con_FPrintf(!plrNum? SV_CONSOLE_PRINT_FLAGS : CPF_GREEN, "%s%s %s\n", fromName, sep, message);
}

/**
 * After a long period with no updates (map setup), calling this will reset
 * the tictimer so that no time seems to have passed.
 */
void Net_ResetTimer(void)
{
    int i;

    firstNetUpdate = true;

    for(i = 0; i < DDMAXPLAYERS; ++i)
    {
        if(/*!clients[i].connected ||*/ !clients[i].smoother) continue;
        Smoother_Clear(clients[i].smoother);
    }
}

/**
 * @return @c true, if the specified player is a real, local player.
 */
boolean Net_IsLocalPlayer(int plrNum)
{
    player_t *plr = &ddPlayers[plrNum];

    return plr->shared.inGame && (plr->shared.flags & DDPF_LOCAL);
}

/**
 * Send the local player(s) ticcmds to the server.
 */
void Net_SendCommands(void)
{
}

static void Net_DoUpdate(void)
{
    static int          lastTime = 0;

    int                 nowTime, newTics;

    /**
     * This timing is only used by the client when it determines if it is
     * time to send ticcmds or coordinates to the server.
     */

    // Check time.
    nowTime = Sys_GetTime();

    // Clock reset?
    if(firstNetUpdate)
    {
        firstNetUpdate = false;
        lastTime = nowTime;
    }
    newTics = nowTime - lastTime;
    if(newTics <= 0)
        return; // Nothing new to update.

    lastTime = nowTime;

    // This is as far as dedicated servers go.
    if(isDedicated)
        return;

    /**
     * Clients will periodically send their coordinates to the server so
     * any prediction errors can be fixed. Client movement is almost
     * entirely local.
     */
#ifdef _DEBUG
    if(netGame && verbose >= 2)
    {
        Con_Message("Net_DoUpdate: coordTimer=%i cl:%i af:%i shmo:%p\n", coordTimer,
                    isClient, allowFrames, ddPlayers[consolePlayer].shared.mo);
    }
#endif

    coordTimer -= newTics;
    if(isClient /*&& allowFrames*/ && coordTimer <= 0 &&
       ddPlayers[consolePlayer].shared.mo)
    {
        mobj_t *mo = ddPlayers[consolePlayer].shared.mo;

        coordTimer = 1; //netCoordTime; // 35/2

        Msg_Begin(PKT_COORDS);
        Writer_WriteFloat(msgWriter, gameTime);
        Writer_WriteFloat(msgWriter, mo->origin[VX]);
        Writer_WriteFloat(msgWriter, mo->origin[VY]);
        if(mo->origin[VZ] == mo->floorZ)
        {
            // This'll keep us on the floor even in fast moving sectors.
            Writer_WriteInt32(msgWriter, DDMININT);
        }
        else
        {
            Writer_WriteInt32(msgWriter, FLT2FIX(mo->origin[VZ]));
        }
        // Also include angles.
        Writer_WriteUInt16(msgWriter, mo->angle >> 16);
        Writer_WriteInt16(msgWriter, P_LookDirToShort(ddPlayers[consolePlayer].shared.lookDir));
        // Control state.
        Writer_WriteChar(msgWriter, FLT2FIX(ddPlayers[consolePlayer].shared.forwardMove) >> 13);
        Writer_WriteChar(msgWriter, FLT2FIX(ddPlayers[consolePlayer].shared.sideMove) >> 13);
        Msg_End();

        Net_SendBuffer(0, 0);
    }
}

/**
 * Handle incoming packets, clients send ticcmds and coordinates to
 * the server.
 */
void Net_Update(void)
{
    Net_DoUpdate();
    N_ListenNodes();

    // Check for received packets.
    if(isClient)
        Cl_GetPackets();
    else
        Sv_GetPackets();
}

/**
 * Build a ticcmd for the local player.
 */
void Net_BuildLocalCommands(timespan_t time)
{
}

void Net_AllocClientBuffers(int clientId)
{
    if(clientId < 0 || clientId >= DDMAXPLAYERS) return;

    assert(!clients[clientId].smoother);

    // Movement smoother.
    clients[clientId].smoother = Smoother_New();
}

void Net_DestroyArrays(void)
{
    int i;

    for(i = 0; i < DDMAXPLAYERS; ++i)
    {
        if(clients[i].smoother)
        {
            Smoother_Delete(clients[i].smoother);
        }
    }

    memset(clients, 0, sizeof(clients));
}

/**
 * This is the network one-time initialization (into single-player mode).
 */
void Net_InitGame(void)
{
    Cl_InitID();

    // In single-player mode there is only player number zero.
    consolePlayer = displayPlayer = 0;

    // We're in server mode if we aren't a client.
    isServer = true;

    // Netgame is true when we're aware of the network (i.e. other players).
    netGame = false;

    ddPlayers[0].shared.inGame = true;
    ddPlayers[0].shared.flags |= DDPF_LOCAL;
    clients[0].id = clientID;
    clients[0].ready = true;
    clients[0].connected = true;
    clients[0].viewConsole = 0;
    clients[0].lastTransmit = -1;

    // Are we timing a demo here?
    if(CommandLine_Check("-timedemo"))
        netTicSync = false;
}

void Net_StopGame(void)
{
    int     i;

    if(isServer)
    {   // We are an open server.
        // This means we should inform all the connected clients that the
        // server is about to close.
        Msg_Begin(PSV_SERVER_CLOSE);
        Msg_End();
        Net_SendBuffer(NSP_BROADCAST, 0);
#if 0
        N_FlushOutgoing();
#endif
    }
    else
    {
#ifdef _DEBUG
        Con_Message("Net_StopGame: Sending PCL_GOODBYE.\n");
#endif
        // We are a connected client.
        Msg_Begin(PCL_GOODBYE);
        Msg_End();
        Net_SendBuffer(0, 0);

        // Must stop recording, we're disconnecting.
        Demo_StopRecording(consolePlayer);
        Cl_CleanUp();
        isClient = false;
    }

    // Netgame has ended.
    netGame = false;
    isServer = true;
    allowSending = false;

    // No more remote users.
    netRemoteUser = 0;
    netLoggedIn = false;

    // All remote players are forgotten.
    for(i = 0; i < DDMAXPLAYERS; ++i)
    {
        player_t           *plr = &ddPlayers[i];
        client_t           *cl = &clients[i];

        plr->shared.inGame = false;
        cl->ready = cl->connected = false;
        cl->id = 0;
        cl->nodeID = 0;
        cl->viewConsole = -1;
        plr->shared.flags &= ~(DDPF_CAMERA | DDPF_CHASECAM | DDPF_LOCAL);
    }

    // We're about to become player zero, so update it's view angles to
    // match our current ones.
    if(ddPlayers[0].shared.mo)
    {
        /* $unifiedangles */
        ddPlayers[0].shared.mo->angle =
            ddPlayers[consolePlayer].shared.mo->angle;
        ddPlayers[0].shared.lookDir =
            ddPlayers[consolePlayer].shared.lookDir;
    }

#ifdef _DEBUG
    Con_Message("Net_StopGame: Reseting console & view player to zero.\n");
#endif
    consolePlayer = displayPlayer = 0;
    ddPlayers[0].shared.inGame = true;
    clients[0].ready = true;
    clients[0].connected = true;
    clients[0].viewConsole = 0;
    ddPlayers[0].shared.flags |= DDPF_LOCAL;
}

/**
 * @return              Delta based on 'now' (- future, + past).
 */
int Net_TimeDelta(byte now, byte then)
{
    int                 delta;

    if(now >= then)
    {   // Simple case.
        delta = now - then;
    }
    else
    {   // There's a wraparound.
        delta = 256 - then + now;
    }

    // The time can be in the future. We'll allow one second.
    if(delta > 220)
        delta -= 256;

    return delta;
}

/// @return  @c true iff a demo is currently being recorded.
static boolean recordingDemo(void)
{
    int i;
    for(i = 0; i < DDMAXPLAYERS; ++i)
    {
        if(ddPlayers[i].shared.inGame && clients[i].recording)
            return true;
    }
    return false;
}

void Net_DrawDemoOverlay(void)
{
    char buf[160], tmp[40];
    int x = Window_Width(theWindow) - 10, y = 10;

    if(!recordingDemo() || !(SECONDS_TO_TICKS(gameTime) & 8))
        return;

    strcpy(buf, "[");
    { int i, c;
    for(i = c = 0; i < DDMAXPLAYERS; ++i)
    {
        if(!(!ddPlayers[i].shared.inGame || !clients[i].recording))
        {
            // This is a "real" player (or camera).
            if(c++)
                strcat(buf, ",");

            sprintf(tmp, "%i:%s", i, clients[i].recordPaused ? "-P-" : "REC");
            strcat(buf, tmp);
        }
    }}
    strcat(buf, "]");

    LIBDENG_ASSERT_IN_MAIN_THREAD();
    LIBDENG_ASSERT_GL_CONTEXT_ACTIVE();

    // Go into screen projection mode.
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, Window_Width(theWindow), Window_Height(theWindow), 0, -1, 1);

    glEnable(GL_TEXTURE_2D);

    FR_SetFont(fontFixed);
    FR_LoadDefaultAttrib();
    FR_SetColorAndAlpha(1, 1, 1, 1);
    FR_DrawTextXY3(buf, x, y, ALIGN_TOPRIGHT, DTF_NO_EFFECTS);

    glDisable(GL_TEXTURE_2D);

    // Restore original matrix.
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
}

/**
 * Does drawing for the engine's HUD, not just the net.
 */
void Net_Drawer(void)
{
    // Draw the Shadow Bias Editor HUD (if it is active).
    SBE_DrawHUD();

    // Draw lightgrid debug display.
    LG_Debug();

    // Draw the blockmap debug display.
    Rend_BlockmapDebug();

    // Draw the light range debug display.
    R_DrawLightRange();

    // Draw the input device debug display.
    Rend_AllInputDeviceStateVisuals();

    // Draw the demo recording overlay.
    Net_DrawDemoOverlay();

#ifdef _DEBUG
    Z_DebugDrawer();
#endif
}

/**
 * Maintain the ack threshold average.
 */
void Net_SetAckTime(int clientNumber, uint period)
{
    /*
    client_t *client = &clients[clientNumber];

    // Add the new time into the array.
    client->ackTimes[client->ackIdx++] = period;
    client->ackIdx %= NUM_ACK_TIMES;

#ifdef _DEBUG
    VERBOSE( Con_Printf("Net_SetAckTime: Client %i, new ack sample of %05u ms.\n",
                        clientNumber, period) );
#endif
    */
}

/**
 * @return              The average ack time of the client.
 */
uint Net_GetAckTime(int clientNumber)
{
    /*
    client_t   *client = &clients[clientNumber];
    uint        average = 0;
    int         i, count = 0;
    uint        smallest = 0, largest = 0;

    // Find the smallest and largest so that they can be ignored.
    smallest = largest = client->ackTimes[0];
    for(i = 0; i < NUM_ACK_TIMES; ++i)
    {
        if(client->ackTimes[i] < smallest)
            smallest = client->ackTimes[i];

        if(client->ackTimes[i] > largest)
            largest = client->ackTimes[i];
    }

    // Calculate the average.
    for(i = 0; i < NUM_ACK_TIMES; ++i)
    {
        if(client->ackTimes[i] != largest && client->ackTimes[i] != smallest)
        {
            average += client->ackTimes[i];
            count++;
        }
    }

    if(count > 0)
    {
        return average / count;
    }
    else
    {
        return client->ackTimes[0];
    }*/
    return 0;
}

/**
 * Sets all the ack times. Used to initial the ack times for new clients.
 */
void Net_SetInitialAckTime(int clientNumber, uint period)
{
    /*
    int         i;

    for(i = 0; i < NUM_ACK_TIMES; ++i)
    {
        clients[clientNumber].ackTimes[i] = period;
    }
    */
}

/**
 * The ack threshold is the maximum period of time to wait before
 * deciding an ack is not coming. The minimum threshold is 50 ms.
 */
uint Net_GetAckThreshold(int clientNumber)
{
    /*
    uint        threshold =
        Net_GetAckTime(clientNumber) * ACK_THRESHOLD_MUL;

    if(threshold < ACK_MINIMUM_THRESHOLD)
    {
        threshold = ACK_MINIMUM_THRESHOLD;
    }

    return threshold;*/
    return 0;
}

void Net_Ticker(timespan_t time)
{
    int         i;
    client_t   *cl;

    // Network event ticker.
    N_NETicker(time);

    if(netDev)
    {
        static int printTimer = 0;

        if(printTimer++ > TICSPERSEC)
        {
            printTimer = 0;
            for(i = 0; i < DDMAXPLAYERS; ++i)
            {
                if(Sv_IsFrameTarget(i))
                {
                    Con_Message("%i(rdy%i): avg=%05ims thres=%05ims "
                                "bwr=%05i maxfs=%05lub unakd=%05i\n", i,
                                clients[i].ready, Net_GetAckTime(i),
                                Net_GetAckThreshold(i),
                                clients[i].bandwidthRating,
                                /*clients[i].bwrAdjustTime,*/
                                (unsigned long) Sv_GetMaxFrameSize(i),
                                Sv_CountUnackedDeltas(i));
                }
                /*if(ddPlayers[i].inGame)
                    Con_Message("%i: cmds=%i\n", i, clients[i].numTics);*/
            }
        }
    }

    // The following stuff is only for netgames.
    if(!netGame)
        return;

    // Check the pingers.
    for(i = 0, cl = clients; i < DDMAXPLAYERS; ++i, cl++)
    {
        // Clients can only ping the server.
        if(!(isClient && i) && i != consolePlayer)
        {
            if(cl->ping.sent)
            {
                // The pinger is active.
                if(Sys_GetRealTime() - cl->ping.sent > PING_TIMEOUT)    // Timed out?
                {
                    cl->ping.times[cl->ping.current] = -1;
                    Net_SendPing(i, 0);
                }
            }
        }
    }
}

/**
 * Prints server/host information into the console. The header line is
 * printed if 'info' is NULL.
 */
void Net_PrintServerInfo(int index, serverinfo_t *info)
{
    if(!info)
    {
        Con_Printf("    %-20s P/M  L Ver:  Game:            Location:\n",
                   "Name:");
    }
    else
    {
        Con_Printf("%-2i: %-20s %i/%-2i %c %-5i %-16s %s:%i\n", index,
                   info->name, info->numPlayers, info->maxPlayers,
                   info->canJoin ? ' ' : '*', info->version, info->plugin,
                   info->address, info->port);
        Con_Printf("    %s p:%ims %-40s\n", info->map, info->ping, info->description);
        Con_Printf("    %s (crc:%x) %s\n", info->gameIdentityKey, info->wadNumber, info->gameConfig);

        // Optional: PWADs in use.
        if(info->pwads[0])
            Con_Printf("    PWADs: %s\n", info->pwads);

        // Optional: names of players.
        if(info->clientNames[0])
            Con_Printf("    Players: %s\n", info->clientNames);

        // Optional: data values.
        if(info->data[0] || info->data[1] || info->data[2])
        {
            Con_Printf("    Data: (%08x, %08x, %08x)\n", info->data[0],
                       info->data[1], info->data[2]);
        }
    }
}

/**
 * Composes a PKT_CHAT network message.
 */
void Net_WriteChatMessage(int from, int toMask, const char* message)
{
    size_t len = strlen(message);
    len = MIN_OF(len, 0xffff);

    Msg_Begin(PKT_CHAT);
    Writer_WriteByte(msgWriter, from);
    Writer_WriteUInt32(msgWriter, toMask);
    Writer_WriteUInt16(msgWriter, len);
    Writer_Write(msgWriter, message, len);
    Msg_End();
}

/**
 * All arguments are sent out as a chat message.
 */
D_CMD(Chat)
{
    char    buffer[100];
    int     i, mode = !stricmp(argv[0], "chat") ||
        !stricmp(argv[0], "say") ? 0 : !stricmp(argv[0], "chatNum") ||
        !stricmp(argv[0], "sayNum") ? 1 : 2;
    unsigned short mask = 0;

    if(argc == 1)
    {
        Con_Printf("Usage: %s %s(text)\n", argv[0],
                   !mode ? "" : mode == 1 ? "(plr#) " : "(name) ");
        Con_Printf("Chat messages are max. 80 characters long.\n");
        Con_Printf("Use quotes to get around arg processing.\n");
        return true;
    }

    // Chatting is only possible when connected.
    if(!netGame)
        return false;

    // Too few arguments?
    if(mode && argc < 3)
        return false;

    // Assemble the chat message.
    strcpy(buffer, argv[!mode ? 1 : 2]);
    for(i = (!mode ? 2 : 3); i < argc; ++i)
    {
        strcat(buffer, " ");
        strncat(buffer, argv[i], 80 - (strlen(buffer) + strlen(argv[i]) + 1));
    }
    buffer[80] = 0;

    // Send the message.
    switch(mode)
    {
    case 0: // chat
        mask = ~0;
        break;

    case 1: // chatNum
        mask = 1 << atoi(argv[1]);
        break;

    case 2: // chatTo
        {
        boolean     found = false;

        for(i = 0; i < DDMAXPLAYERS && !found; ++i)
        {
            if(!stricmp(clients[i].name, argv[1]))
            {
                mask = 1 << i;
                found = true;
            }
        }
        break;
        }

    default:
        Con_Error("CCMD_Chat: Invalid value, mode = %i.", mode);
        break;
    }

    Net_WriteChatMessage(consolePlayer, mask, buffer);

    if(!isClient)
    {
        if(mask == (unsigned short) ~0)
        {
            Net_SendBuffer(NSP_BROADCAST, 0);
        }
        else
        {
            for(i = 1; i < DDMAXPLAYERS; ++i)
                if(ddPlayers[i].shared.inGame && (mask & (1 << i)))
                    Net_SendBuffer(i, 0);
        }
    }
    else
    {
        Net_SendBuffer(0, 0);
    }

    // Show the message locally.
    Net_ShowChatMessage(consolePlayer, buffer);

    // Inform the game, too.
    gx.NetPlayerEvent(consolePlayer, DDPE_CHAT_MESSAGE, buffer);
    return true;
}

D_CMD(Kick)
{
    int     num;

    if(!netGame)
    {
        Con_Printf("This is not a netGame.\n");
        return false;
    }

    if(!isServer)
    {
        Con_Printf("This command is for the server only.\n");
        return false;
    }

    num = atoi(argv[1]);
    if(num < 1 || num >= DDMAXPLAYERS)
    {
        Con_Printf("Invalid client number.\n");
        return false;
    }

    if(netRemoteUser == num)
    {
        Con_Printf("Can't kick the client who's logged in.\n");
        return false;
    }

    Sv_Kick(num);
    return true;
}

D_CMD(SetName)
{
    Con_SetString("net-name", argv[1]);

    if(!netGame)
        return true;

    // The server does not have a name.
    if(!isClient) return false;

    memset(clients[consolePlayer].name, 0, sizeof(clients[consolePlayer].name));
    strncpy(clients[consolePlayer].name, argv[1], PLAYERNAMELEN - 1);

    Net_SendPlayerInfo(consolePlayer, 0);
    return true;
}

D_CMD(SetTicks)
{
//  extern double lastSharpFrameTime;

    firstNetUpdate = true;
    Sys_TicksPerSecond(strtod(argv[1], 0));
//  lastSharpFrameTime = Sys_GetTimef();
    return true;
}

D_CMD(MakeCamera)
{
    /*  int cp;
       mobj_t *mo;
       ddplayer_t *conp = players + consolePlayer;

       if(argc < 2) return true;
       cp = atoi(argv[1]);
       clients[cp].connected = true;
       clients[cp].ready = true;
       clients[cp].updateCount = UPDATECOUNT;
       ddPlayers[cp].flags |= DDPF_CAMERA;
       ddPlayers[cp].inGame = true; // !!!
       Sv_InitPoolForClient(cp);
       mo = Z_Malloc(sizeof(mobj_t), PU_MAP, 0);
       memset(mo, 0, sizeof(*mo));
       mo->origin[VX] = conp->mo->origin[VX];
       mo->origin[VY] = conp->mo->origin[VY];
       mo->origin[VZ] = conp->mo->origin[VZ];
       mo->bspLeaf = conp->mo->bspLeaf;
       ddPlayers[cp].mo = mo;
       displayPlayer = cp; */

    // Create a new local player.
    int cp;

    cp = atoi(argv[1]);
    if(cp < 0 || cp >= DDMAXPLAYERS)
        return false;

    if(clients[cp].connected)
    {
        Con_Printf("Client %i already connected.\n", cp);
        return false;
    }

    clients[cp].connected = true;
    clients[cp].ready = true;
    clients[cp].viewConsole = cp;
    ddPlayers[cp].shared.flags |= DDPF_LOCAL;
    Smoother_Clear(clients[cp].smoother);
    Sv_InitPoolForClient(cp);

    R_SetupDefaultViewWindow(cp);
    // Update the viewports.
    R_SetViewGrid(0, 0);

    return true;
}

D_CMD(SetConsole)
{
    int cp;

    cp = atoi(argv[1]);
    if(ddPlayers[cp].shared.inGame)
    {
        consolePlayer = displayPlayer = cp;
    }

    // Update the viewports.
    R_SetViewGrid(0, 0);

    return true;
}

void Net_FinishConnection(int nodeId, const byte* data, int size)
{
    serverinfo_t info;

    Con_Message("Net_FinishConnection: Got reply with %i bytes.\n", size);

    // Parse the response for server info.
    N_ClientHandleResponseToInfoQuery(nodeId, data, size);

    if(N_GetHostInfo(0, &info))
    {
        // Found something!
        //Net_PrintServerInfo(0, NULL);
        //Net_PrintServerInfo(0, &info);
        Con_Execute(CMDS_CONSOLE, "net connect 0", false, false);
    }
    else
    {
        Con_Message("Net_FinishConnection: Failed to retrieve server info.\n");
    }
}

int Net_StartConnection(const char* address, int port)
{
    // Make sure TCP/IP is active.
    if(!N_InitService(false))
    {
        Con_Message("TCP/IP not available.\n");
        return false;
    }

    Con_Message("Net_StartConnection: Connecting to %s...\n", address);

    // Start searching at the specified location.
    return N_LookForHosts(address, port, Net_FinishConnection);


/*
            if(N_GetHostInfo(0, &param->info))
            {   // Found something!
                Con_Execute(CMDS_CONSOLE, "net connect 0", false, false);

                returnValue = true;
                isDone = true;
            }
            else
            {   // Nothing yet, should we wait a while longer?
                if(Sys_GetSeconds() - startTime >= netConnectTimeout)
                    isDone = true;
                else
                    Sys_Sleep(250); // Wait a while.
            }
        }

        if(!returnValue)
            Con_Message("No response from %s.\n", param->address);
    }
    else
    {
        Con_Message("TCP/IP not available.\n");
    }

    BusyMode_WorkerEnd();
    return returnValue;*/
}

/**
 * Intelligently connect to a server. Just provide an IP address and the
 * rest is automatic.
 */
D_CMD(Connect)
{
    char *ptr;
    int port = 0;

    if(argc < 2 || argc > 3)
    {
        Con_Printf("Usage: %s (ip-address) [port]\n", argv[0]);
        Con_Printf("A TCP/IP connection is created to the given server.\n");
        Con_Printf("If a port is not specified port zero will be used.\n");
        return true;
    }

    if(netGame)
    {
        Con_Printf("Already connected.\n");
        return false;
    }

    // If there is a port specified in the address, use it.
    port = 0;
    if((ptr = strrchr(argv[1], ':')))
    {
        port = strtol(ptr + 1, 0, 0);
        *ptr = 0;
    }
    if(argc == 3)
    {
        port = strtol(argv[2], 0, 0);
    }

    return Net_StartConnection(argv[1], port);
}

/**
 * The 'net' console command.
 */
D_CMD(Net)
{
    boolean success = true;

    if(argc == 1) // No args?
    {
        Con_Printf("Usage: %s (cmd/args)\n", argv[0]);
        Con_Printf("Commands:\n");
        Con_Printf("  init\n");
        Con_Printf("  shutdown\n");
        Con_Printf("  setup client\n");
        Con_Printf("  setup server\n");
        Con_Printf("  info\n");
        Con_Printf("  announce\n");
        Con_Printf("  request\n");
        Con_Printf("  search (address) [port]   (local or targeted query)\n");
        Con_Printf("  servers   (asks the master server)\n");
        Con_Printf("  connect (idx)\n");
        Con_Printf("  mconnect (m-idx)\n");
        Con_Printf("  disconnect\n");
        Con_Printf("  server go/start\n");
        Con_Printf("  server close/stop\n");
        return true;
    }

    if(argc == 2 || argc == 3)
    {
        if(!stricmp(argv[1], "init"))
        {
            // Init the service (assume client mode).
            if((success = N_InitService(false)))
                Con_Message("Network initialization OK.\n");
            else
                Con_Message("Network initialization failed!\n");

            // Let everybody know of this.
            return CmdReturnValue = success;
        }
    }

    if(argc == 2) // One argument?
    {
        if(!stricmp(argv[1], "shutdown"))
        {
            if(N_IsAvailable())
            {
                Con_Printf("Shutting down %s.\n", N_GetProtocolName());
                N_ShutdownService();
            }
            else
            {
                success = false;
            }
        }
        else if(!stricmp(argv[1], "announce"))
        {
            N_MasterAnnounceServer(true);
        }
        else if(!stricmp(argv[1], "request"))
        {
            N_MasterRequestList();
        }
        else if(!stricmp(argv[1], "servers"))
        {
            N_MAPost(MAC_REQUEST);
            N_MAPost(MAC_WAIT);
            N_MAPost(MAC_LIST);
        }
        else if(!stricmp(argv[1], "info"))
        {
            N_PrintNetworkStatus();
            Con_Message("Network game: %s\n", netGame ? "yes" : "no");
            Con_Message("This is console %i (local player %i).\n", consolePlayer, P_ConsoleToLocal(consolePlayer));
        }
        else if(!stricmp(argv[1], "disconnect"))
        {
            if(!netGame)
            {
                Con_Printf("This client is not connected to a server.\n");
                return false;
            }

            if(!isClient)
            {
                Con_Printf("This is not a client.\n");
                return false;
            }

            if((success = N_Disconnect()) != false)
            {
                Con_Message("Disconnected.\n");
            }
        }
        else
        {
            Con_Printf("Bad arguments.\n");
            return false; // Bad args.
        }
    }

    if(argc == 3) // Two arguments?
    {
        if(!stricmp(argv[1], "server"))
        {
            if(!stricmp(argv[2], "go") || !stricmp(argv[2], "start"))
            {
                if(netGame)
                {
                    Con_Printf("Already in a netGame.\n");
                    return false;
                }

                CmdReturnValue = success = N_ServerOpen();

                if(success)
                {
                    Con_Message("Server \"%s\" started.\n", serverName);
                }
            }
            else if(!stricmp(argv[2], "close") || !stricmp(argv[2], "stop"))
            {
                if(!isServer)
                {
                    Con_Printf("This is not a server!\n");
                    return false;
                }

                // Close the server and kick everybody out.
                if((success = N_ServerClose()) != false)
                {
                    Con_Message("Server \"%s\" closed.\n", serverName);
                }
            }
            else
            {
                Con_Printf("Bad arguments.\n");
                return false;
            }
        }
        else if(!stricmp(argv[1], "search"))
        {
            success = N_LookForHosts(argv[2], 0, 0);
        }
        else if(!stricmp(argv[1], "connect"))
        {
            int             idx;

            if(netGame)
            {
                Con_Printf("Already connected.\n");
                return false;
            }

            idx = strtol(argv[2], NULL, 10);
            CmdReturnValue = success = N_Connect(idx);

            if(success)
            {
                Con_Message("Connected.\n");
            }
        }
        else if(!stricmp(argv[1], "mconnect"))
        {
            serverinfo_t    info;

            if(N_MasterGet(strtol(argv[2], 0, 0), &info))
            {
                // Connect using TCP/IP.
                return Con_Executef(CMDS_CONSOLE, false, "connect %s %i",
                                    info.address, info.port);
            }
            else
                return false;
        }
        else if(!stricmp(argv[1], "setup"))
        {
            // Start network setup.
            DD_NetSetup(!stricmp(argv[2], "server"));
            CmdReturnValue = true;
        }
    }

    if(argc == 4)
    {
        if(!stricmp(argv[1], "search"))
        {
            success = N_LookForHosts(argv[2], strtol(argv[3], 0, 0), 0);
        }
    }

    return success;
}
