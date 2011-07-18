/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2009 Daniel Swanson <danij@dengine.net>
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
 * net_main.c: Client/server networking.
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

// MACROS ------------------------------------------------------------------

#define OBSOLETE                CVF_NO_ARCHIVE|CVF_HIDE // Old ccmds.

// The threshold is the average ack time * mul.
#define ACK_THRESHOLD_MUL       1.5f

// Never wait a too short time for acks.
#define ACK_MINIMUM_THRESHOLD   50

// TYPES -------------------------------------------------------------------

typedef struct connectparam_s {
    char        address[256];
    int         port;
    serverinfo_t info;
} connectparam_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

D_CMD(HuffmanStats); // in net_buf.c
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
    C_CMD("chat", NULL, Chat);
    C_CMD("chatnum", NULL, Chat);
    C_CMD("chatto", NULL, Chat);
    C_CMD("conlocp", "i", MakeCamera);
    C_CMD_FLAGS("connect", NULL, Connect, CMDF_NO_DEDICATED);
    C_CMD("huffman", "", HuffmanStats);
    C_CMD("kick", "i", Kick);
    C_CMD("login", NULL, Login);
    C_CMD("logout", "", Logout);
    C_CMD("net", NULL, Net);
    C_CMD("ping", NULL, Ping);
    C_CMD("say", NULL, Chat);
    C_CMD("saynum", NULL, Chat);
    C_CMD("sayto", NULL, Chat);
    C_CMD("setname", "s", SetName);
    C_CMD("setcon", "i", SetConsole);
    C_CMD("settics", "i", SetTicks);

    N_Register();
}

void Net_Init(void)
{
    Net_AllocArrays();
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

/**
 * Part of the Doomsday public API.
 *
 * @return              The name of the specified player.
 */
const char* Net_GetPlayerName(int player)
{
    return clients[player].name;
}

/**
 * Part of the Doomsday public API.
 *
 * @return              Client identifier for the specified player.
 */
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
    // Don't send anything during demo playback.
    if(playback)
        return;

    // Update the length of the message.
    netBuffer.length = netBuffer.cursor - netBuffer.msg.data;
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
        netBuffer.cursor = netBuffer.msg.data;
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
    netBuffer.cursor = netBuffer.msg.data;

    return true;
}

/**
 * This is the public interface of the message sender.
 */
void Net_SendPacket(int to_player, int type, void *data, size_t length)
{
    int                 flags = 0;

    // What kind of delivery to use?
    if(to_player & DDSP_CONFIRM)
        flags |= SPF_CONFIRM;
    if(to_player & DDSP_ORDERED)
        flags |= SPF_ORDERED;

    Msg_Begin(type);
    if(data)
        Msg_Write(data, length);

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
void Net_ShowChatMessage(void)
{
    // The current packet in the netBuffer is a chat message, let's unwrap
    // and show it.
    Con_FPrintf(CBLF_GREEN, "%s: %s\n", clients[netBuffer.msg.data[0]].name,
                netBuffer.msg.data + 3);
}

/**
 * After a long period with no updates (map setup), calling this will reset
 * the tictimer so that no time seems to have passed.
 */
void Net_ResetTimer(void)
{
    firstNetUpdate = true;
}

/**
 * @return @c true, if the specified player is a real, local
 *         player.
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
#if 0
    uint                i;
    byte               *msg;
    ticcmd_t           *cmd;

    if(isDedicated)
        return;

    // Send the commands of all local players.
    for(i = 0; i < DDMAXPLAYERS; ++i)
    {
        if(!Net_IsLocalPlayer(i))
            continue;

        /**
         * Clients send their ticcmds to the server at regular intervals,
         * but significantly less often than new ticcmds are built.
         * Therefore they need to send a combination of all the cmds built
         * during the wait period.
         */

        cmd = clients[i].aggregateCmd;

        /**
         * The game will pack the commands into a buffer. The returned
         * pointer points to a buffer that contains its size and the
         * packed commands.
         */

        msg = gx.NetWriteCommands(1, cmd);

        Msg_Begin(PCL_COMMANDS);
        Msg_Write(msg + 2, *(ushort *) msg);

        /**
         * Send the packet to the server, i.e. player zero.
         * Player commands are sent over TCP so their integrity and order
         * are guaranteed.
         */

        Net_SendBuffer(0, (isClient ? 0 : SPF_REBOUND) | SPF_ORDERED);

        // Clients will begin composing a new aggregate now that this one
        // has been sent.
        memset(cmd, 0, TICCMD_SIZE);
    }
#endif
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

#if 0
    // Build new ticcmds for console player.
    for(i = 0; i < newtics; ++i)
    {
        DD_ProcessEvents();

        /*Con_Printf("mktic:%i gt:%i newtics:%i >> %i\n",
           maketic, gametic, newtics, maketic-gametic); */

        if(playback)
        {
            numlocal = 0;
            if(availableTics < LOCALTICS)
                availableTics++;
        }
        else if(!isDedicated && !ui_active)
        {
            // Place the new ticcmd in the local ticcmds buffer.
            ticcmd_t *cmd = Net_LocalCmd();

            if(cmd)
            {
                gx.BuildTicCmd(cmd);

                // Set the time stamp. Only the lowest byte is stored.
                //cmd->time = gametic + availableTics;

                // Availabletics counts the tics that have cmds.
                availableTics++;
                if(isClient)
                {
                    // When not playing a demo, this is the last command.
                    // It is used in local movement prediction.
                    memcpy(clients[consolePlayer].lastCmd, cmd, sizeof(*cmd));
                }
            }
        }
    }
#endif

    // This is as far as dedicated servers go.
    if(isDedicated)
        return;

    /**
     * Clients will periodically send their coordinates to the server so
     * any prediction errors can be fixed. Client movement is almost
     * entirely local.
     */
#ifdef _DEBUG
    VERBOSE2( Con_Message("Net_DoUpdate: coordTimer=%i cl:%i af:%i shmo:%p\n", coordTimer,
                          isClient, allowFrames, ddPlayers[consolePlayer].shared.mo) );
#endif
    coordTimer -= newTics;
    if(isClient /*&& allowFrames*/ && coordTimer <= 0 &&
       ddPlayers[consolePlayer].shared.mo)
    {
        mobj_t *mo = ddPlayers[consolePlayer].shared.mo;

        coordTimer = 1; //netCoordTime; // 35/2
        Msg_Begin(PKT_COORDS);
        Msg_WriteFloat(mo->pos[VX]);
        Msg_WriteFloat(mo->pos[VY]);
        if(mo->pos[VZ] == mo->floorZ)
        {
            // This'll keep us on the floor even in fast moving sectors.
            Msg_WriteLong(DDMININT);
        }
        else
        {
            Msg_WriteLong(FLT2FIX(mo->pos[VZ]));
        }
        // Also include momentum and angle.
        Msg_WriteShort((short) (mo->mom[VX] * 256));
        Msg_WriteShort((short) (mo->mom[VY] * 256));
        Msg_WriteShort((short) (mo->mom[VZ] * 256));
        Msg_WriteShort(mo->angle >> 16);
        // Control state.
        Msg_WriteByte(FLT2FIX(ddPlayers[consolePlayer].shared.forwardMove) >> 13);
        Msg_WriteByte(FLT2FIX(ddPlayers[consolePlayer].shared.sideMove) >> 13);
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

    // Listen for packets. Call the correct packet handler.
    N_Listen();
    if(isClient)
        Cl_GetPackets();
    else // Single-player or server.
        Sv_GetPackets();
}

/**
 * Build a ticcmd for the local player.
 */
void Net_BuildLocalCommands(timespan_t time)
{
#if 0
    uint        i;
    ticcmd_t   *cmd;

    if(isDedicated)
        return;

    // Generate ticcmds for local players.
    for(i = 0; i < DDMAXPLAYERS; ++i)
    {
        if(!Net_IsLocalPlayer(i))
            continue;

        cmd = clients[i].lastCmd;

        // The command will stay 'empty' if no controls are active.
        memset(cmd, 0, sizeof(*cmd));
        // No actions can be undertaken during demo playback or when
        // in UI mode.
        if(!(playback || UI_IsActive()))
        {
            P_BuildCommand(cmd, i);
        }

        // Be sure to merge each built command into the aggregate that
        // will be sent periodically to the server.
        P_MergeCommand(clients[i].aggregateCmd, cmd);
    }
#endif
}

/**
 * Called from Net_Init to initialize the ticcmd arrays.
 */
void Net_AllocArrays(void)
{
    int     i;

#if 0
    // Local ticcmds are stored into this array before they're copied
    // to netplayer[0]'s ticcmds buffer.
    localticcmds = M_Calloc(LOCALTICS * TICCMD_SIZE);
    numlocal = 0;               // Nothing in the buffer.
#endif

    for(i = 0; i < DDMAXPLAYERS; ++i)
    {
        memset(clients + i, 0, sizeof(clients[i]));
        // The server stores ticcmds sent by the clients to these
        // buffers.
        //clients[i].ticCmds = M_Calloc(BACKUPTICS * TICCMD_SIZE);
        // The last cmd that was executed is stored here.
        //clients[i].lastCmd = M_Calloc(TICCMD_SIZE);
        //clients[i].aggregateCmd = M_Calloc(TICCMD_SIZE);
        clients[i].runTime = -1;
    }
}

void Net_DestroyArrays(void)
{
    int     i;

#if 0
    M_Free(localticcmds);
    localticcmds = NULL;
#endif

    for(i = 0; i < DDMAXPLAYERS; ++i)
    {
        //M_Free(clients[i].ticCmds);
        //M_Free(clients[i].lastCmd);
        //M_Free(clients[i].aggregateCmd);
        //clients[i].ticCmds = NULL;
        //clients[i].lastCmd = NULL;
        //clients[i].aggregateCmd = NULL;
    }
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
    if(ArgCheck("-timedemo"))
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
        Net_SendBuffer(NSP_BROADCAST, SPF_CONFIRM);
#if 0
        N_FlushOutgoing();
#endif
    }
    else
    {   // We are a connected client.
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
        cl->nodeID = 0;
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

/**
 * This is a bit complicated and quite possibly unnecessarily so. The
 * idea is, however, that because the ticcmds sent by clients arrive in
 * bursts, we'll preserve the motion by 'executing' the commands in the
 * same order in which they were generated. If the client's connection
 * lags a lot, the difference between the serverside and clientside
 * positions will be *large*, especially when the client is running.
 * If too many commands are buffered, the client's coord announcements
 * will be processed before the actual movement commands, resulting in
 * serverside warping (which is perceived by all other clients).
 */
int Net_GetTicCmd(void *pCmd, int player)
{
    return false;

#if 0

    client_t *client = &clients[player];
    ticcmd_t *cmd = pCmd;

    /*  int doMerge = false;
       int future; */

    if(client->numTics <= 0)
    {
        // No more commands for this player.
        return false;
    }

    // Return the next ticcmd from the buffer.
    // There will be one less tic in the buffer after this.
    client->numTics--;
    memcpy(cmd, &client->ticCmds[TICCMD_IDX(client->firstTic++)], TICCMD_SIZE);

    // This is the new last command.
    memcpy(client->lastCmd, cmd, TICCMD_SIZE);

    // Make sure the firsttic index is in range.
    client->firstTic %= BACKUPTICS;
    return true;
#endif
}

/**
 * Does drawing for the engine's HUD, not just the net.
 */
void Net_Drawer(void)
{
    char                buf[160], tmp[40];
    int                 i, c;
    boolean             showBlinkR = false;

    for(i = 0; i < DDMAXPLAYERS; ++i)
    {
        if(ddPlayers[i].shared.inGame && clients[i].recording)
            showBlinkR = true;
    }

    // Draw the Shadow Bias Editor HUD (if it is active).
    SBE_DrawHUD();

    // Draw lightgrid debug display.
    LG_Debug();

    // Draw the blockmap debug display.
    P_BlockmapDebug();

    // Draw the light range debug display.
    R_DrawLightRange();

    if(!netDev && !showBlinkR && !consoleShowFPS)
        return;

    // Go into screen projection mode.
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, theWindow->width, theWindow->height, 0, -1, 1);

    if(showBlinkR && SECONDS_TO_TICKS(gameTime) & 8)
    {
        strcpy(buf, "[");
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
        }

        strcat(buf, "]");
        i = theWindow->width - FR_TextWidth(buf);
        //glColor3f(0, 0, 0);
        //FR_TextOut(buf, i - 8, 12);
        glColor3f(1, 1, 1);
        FR_ShadowTextOut(buf, i - 10, 10);
    }

    if(netDev)
    {
        /*      glColor3f(1, 1, 1);
           sprintf(buf, "G%i", gametic);
           FR_TextOut(buf, 10, 10);
           for(i = 0, cl = clients; i<DDMAXPLAYERS; ++i, cl++)
           if(ddPlayers[i].inGame)
           {
           sprintf(buf, "%02i:%+04i[%+03i](%02d/%03i) pf:%x", i, cl->lag,
           cl->lagStress, cl->numtics, cl->runTime,
           ddPlayers[i].flags);
           FR_TextOut(buf, 10, 10+10*(i+1));
           } */
    }
    Rend_ConsoleFPS(theWindow->width - 10, 30);

    // Restore original matrix.
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
}

/**
 * Maintain the ack threshold average.
 */
void Net_SetAckTime(int clientNumber, uint period)
{
    client_t *client = &clients[clientNumber];

    // Add the new time into the array.
    client->ackTimes[client->ackIdx++] = period;
    client->ackIdx %= NUM_ACK_TIMES;

#ifdef _DEBUG
    VERBOSE( Con_Printf("Net_SetAckTime: Client %i, new ack sample of %05u ms.\n",
                        clientNumber, period) );
#endif
}

/**
 * @return              The average ack time of the client.
 */
uint Net_GetAckTime(int clientNumber)
{
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
    }
}

/**
 * Sets all the ack times. Used to initial the ack times for new clients.
 */
void Net_SetInitialAckTime(int clientNumber, uint period)
{
    int         i;

    for(i = 0; i < NUM_ACK_TIMES; ++i)
    {
        clients[clientNumber].ackTimes[i] = period;
    }
}

/**
 * The ack threshold is the maximum period of time to wait before
 * deciding an ack is not coming. The minimum threshold is 50 ms.
 */
uint Net_GetAckThreshold(int clientNumber)
{
    uint        threshold =
        Net_GetAckTime(clientNumber) * ACK_THRESHOLD_MUL;

    if(threshold < ACK_MINIMUM_THRESHOLD)
    {
        threshold = ACK_MINIMUM_THRESHOLD;
    }

    return threshold;
}

void Net_Ticker(void /*timespan_t time*/)
{
    int         i;
    client_t   *cl;

    // Network event ticker.
    N_NETicker();

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
                                "bwr=%05i (adj:%i) maxfs=%05lub unakd=%05i\n", i,
                                clients[i].ready, Net_GetAckTime(i),
                                Net_GetAckThreshold(i),
                                clients[i].bandwidthRating,
                                clients[i].bwrAdjustTime,
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
                   info->canJoin ? ' ' : '*', info->version, info->game,
                   info->address, info->port);
        Con_Printf("    %s (%s:%x) p:%ims %-40s\n", info->map, info->iwad,
                   info->wadNumber, info->ping, info->description);

        Con_Printf("    %s %s\n", info->gameMode, info->gameConfig);

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
    Msg_Begin(PKT_CHAT);
    Msg_WriteByte(consolePlayer);
    Msg_WriteShort(mask);
    Msg_Write(buffer, strlen(buffer) + 1);

    if(!isClient)
    {
        if(mask == (unsigned short) ~0)
        {
            Net_SendBuffer(NSP_BROADCAST, SPF_ORDERED);
        }
        else
        {
            for(i = 1; i < DDMAXPLAYERS; ++i)
                if(ddPlayers[i].shared.inGame && (mask & (1 << i)))
                    Net_SendBuffer(i, SPF_ORDERED);
        }
    }
    else
    {
        Net_SendBuffer(0, SPF_ORDERED);
    }

    // Show the message locally.
    Net_ShowChatMessage();

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
    playerinfo_packet_t info;

    Con_SetString("net-name", argv[1], false);
    if(!netGame)
        return true;

    // In netgames, a notification is sent to other players.
    memset(&info, 0, sizeof(info));
    info.console = consolePlayer;
    strncpy(info.name, argv[1], PLAYERNAMELEN - 1);

    // Serverplayers can update their name right away.
    if(!isClient)
        strcpy(clients[consolePlayer].name, info.name);

    Net_SendPacket(DDSP_CONFIRM | (isClient ? consolePlayer : DDSP_ALL_PLAYERS),
                   PKT_PLAYER_INFO, &info, sizeof(info));
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
       mo->pos[VX] = conp->mo->pos[VX];
       mo->pos[VY] = conp->mo->pos[VY];
       mo->pos[VZ] = conp->mo->pos[VZ];
       mo->subsector = conp->mo->subsector;
       ddPlayers[cp].mo = mo;
       displayPlayer = cp; */

    // Create a new local player.
    int                 cp;

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
    //clients[cp].updateCount = UPDATECOUNT;
    ddPlayers[cp].shared.flags |= DDPF_LOCAL;
    Sv_InitPoolForClient(cp);

    // Update the viewports.
    R_SetViewGrid(0, 0);

    return true;
}

D_CMD(SetConsole)
{
    int                 cp;

    cp = atoi(argv[1]);
    if(ddPlayers[cp].shared.inGame)
    {
        consolePlayer = displayPlayer = cp;
    }

    // Update the viewports.
    R_SetViewGrid(0, 0);

    return true;
}

int Net_ConnectWorker(void *ptr)
{
    connectparam_t     *param = ptr;
    double              startTime = 0;
    boolean             isDone = false;
    int                 returnValue = false;

    // Make sure TCP/IP is active.
    if(N_InitService(false))
    {
        Con_Message("Connecting to %s...\n", param->address);

        // Start searching at the specified location.
        N_LookForHosts(param->address, param->port);

        startTime = Sys_GetSeconds();
        isDone = false;
        while(!isDone)
        {
            if(N_GetHostInfo(0, &param->info))
            {   // Found something!
                Net_PrintServerInfo(0, NULL);
                Net_PrintServerInfo(0, &param->info);
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
            Con_Printf("No response from %s.\n", param->address);
    }
    else
    {
        Con_Message("TCP/IP not available.\n");
    }

    Con_BusyWorkerEnd();
    return returnValue;
}

/**
 * Intelligently connect to a server. Just provide an IP address and the
 * rest is automatic.
 */
D_CMD(Connect)
{
    connectparam_t      param;
    char               *ptr;

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

    strcpy(param.address, argv[1]);

    // If there is a port specified in the address, use it.
    if((ptr = strrchr(param.address, ':')))
    {
        param.port = strtol(ptr + 1, 0, 0);
        *ptr = 0;
    }
    if(argc == 3)
    {
        param.port = strtol(argv[2], 0, 0);
    }

    return Con_Busy(BUSYF_ACTIVITY | (verbose? BUSYF_CONSOLE_OUTPUT : 0),
                    NULL, Net_ConnectWorker, &param);
}

/**
 * The 'net' console command.
 */
D_CMD(Net)
{
    int                 i;
    boolean             success = true;

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
            if(isServer)
            {
                Con_Printf("Clients:\n");
                for(i = 0; i < DDMAXPLAYERS; ++i)
                {
                    client_t           *cl = &clients[i];
                    player_t           *plr = &ddPlayers[i];

                    if(cl->connected)
                        Con_Printf("%2i: %10s node %2x, entered at %07i (ingame:%i, handshake:%i)\n",
                                   i, cl->name, cl->nodeID, cl->enterTime,
                                   plr->shared.inGame, cl->handshake);
                }
            }

            Con_Printf("Network game: %s\n", netGame ? "yes" : "no");
            Con_Printf("Server: %s\n", isServer ? "yes" : "no");
            Con_Printf("Client: %s\n", isClient ? "yes" : "no");
            Con_Printf("Console number: %i\n", consolePlayer);
            Con_Printf("Local player #: %i\n", P_ConsoleToLocal(consolePlayer));

            N_PrintInfo();
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
            success = N_LookForHosts(argv[2], 0);
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
            success = N_LookForHosts(argv[2], strtol(argv[3], 0, 0));
        }
    }

    return success;
}
