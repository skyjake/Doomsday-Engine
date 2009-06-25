/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2009 Daniel Swanson <danij@dengine.net>
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
 * cl_main.c: Network Client
 */

// HEADER FILES ------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "de_base.h"
#include "de_console.h"
#include "de_system.h"
#include "de_network.h"
#include "de_graphics.h"
#include "de_misc.h"
#include "de_play.h"

#include "r_main.h"

// MACROS ------------------------------------------------------------------

// Clients send commands on every tic.
#define CLIENT_TICCMD_INTERVAL  0

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern int gotFrame;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

ident_t clientID;
boolean handshakeReceived = false;
int gameReady = false;
int serverTime;
boolean netLoggedIn = false; // Logged in to the server.
int clientPaused = false; // Set by the server.

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

void Cl_InitID(void)
{
    int                 i;
    FILE*               file;

    if((i = ArgCheckWith("-id", 1)) != 0)
    {
        clientID = strtoul(Argv(i + 1), 0, 0);
        Con_Message("Cl_InitID: Using custom id 0x%08x.\n", clientID);
        return;
    }

    // Read the client ID number file.
    srand(time(NULL));
    if((file = fopen("client.id", "rb")) != NULL)
    {
        fread(&clientID, sizeof(clientID), 1, file);
        clientID = ULONG(clientID);
        fclose(file);
        return;
    }
    // Ah-ha, we need to generate a new ID.
    clientID = (ident_t)
        ULONG(Sys_GetRealTime() * rand() + (rand() & 0xfff) +
              ((rand() & 0xfff) << 12) + ((rand() & 0xff) << 24));
    // Write it to the file.
    if((file = fopen("client.id", "wb")) != NULL)
    {
        fwrite(&clientID, sizeof(clientID), 1, file);
        fclose(file);
    }
}

int Cl_GameReady(void)
{
    return (handshakeReceived && gameReady);
}

void Cl_CleanUp(void)
{
    Con_Printf("Cl_CleanUp.\n");

    clientPaused = false;
    handshakeReceived = false;

    Cl_DestroyClientMobjs();
    Cl_InitPlayers();
    Cl_RemoveMovers();
    GL_SetFilter(false);
}

/**
 * Sends a hello packet.
 * PCL_HELLO2 includes the Game ID (16 chars).
 */
void Cl_SendHello(void)
{
    char                buf[256];

    Msg_Begin(PCL_HELLO2);
    Msg_WriteLong(clientID);

    // The game mode is included in the hello packet.
    memset(buf, 0, sizeof(buf));
    strncpy(buf, (char *) gx.GetVariable(DD_GAME_MODE), sizeof(buf) - 1);

#ifdef _DEBUG
Con_Message("Cl_SendHello: game mode = %s\n", buf);
#endif

    Msg_Write(buf, 16);
    Net_SendBuffer(0, SPF_ORDERED);
}

void Cl_AnswerHandshake(handshake_packet_t* pShake)
{
    int                 i;
    handshake_packet_t  shake;

    // Copy the data to a buffer of our own.
    memcpy(&shake, pShake, sizeof(shake));
    shake.playerMask = USHORT(shake.playerMask);
    shake.gameTime = LONG(shake.gameTime);

    // Immediately send an acknowledgement.
    Msg_Begin(PCL_ACK_SHAKE);
    Net_SendBuffer(0, SPF_ORDERED);

    // Check the version number.
    if(shake.version != SV_VERSION)
    {
        Con_Message
            ("Cl_AnswerHandshake: Version conflict! (you:%i, server:%i)\n",
             SV_VERSION, shake.version);
        Con_Execute(CMDS_DDAY, "net disconnect", false, false);
        Demo_StopPlayback();
        Con_Open(true);
        return;
    }

    // Update time and player ingame status.
    gameTime = shake.gameTime / 100.0;
    for(i = 0; i < DDMAXPLAYERS; ++i)
    {
        ddPlayers[i].shared.inGame = (shake.playerMask & (1 << i)) != 0;
    }
    consolePlayer = displayPlayer = shake.yourConsole;
    clients[consolePlayer].numTics = 0;
    clients[consolePlayer].firstTic = 0;

    isClient = true;
    isServer = false;
    netLoggedIn = false;
    clientPaused = false;

    if(handshakeReceived)
        return;

    // This prevents redundant re-initialization.
    handshakeReceived = true;

    // Soon after this packet will follow the game's handshake.
    gameReady = false;
    Cl_InitFrame();

    Con_Printf("Cl_AnswerHandshake: myConsole:%i, gameTime:%i.\n",
               shake.yourConsole, shake.gameTime);

    /**
     * Tell the game that we have arrived. The map will be changed when the
     * game's handshake arrives (handled in the game).
     */
    gx.NetPlayerEvent(consolePlayer, DDPE_ARRIVAL, 0);

    // Prepare the client-side data.
    Cl_InitClientMobjs();
    Cl_InitMovers();

    // Get ready for ticking.
    DD_ResetTimer();

    Con_Executef(CMDS_DDAY, true, "setcon %i", consolePlayer);
}

void Cl_HandlePlayerInfo(playerinfo_packet_t* info)
{
    player_t*           plr;
    boolean             present;

    Con_Printf("Cl_HandlePlayerInfo: console:%i name:%s\n", info->console,
               info->name);

    // Is the console number valid?
    if(info->console >= DDMAXPLAYERS)
        return;

    plr = &ddPlayers[info->console];
    present = plr->shared.inGame;
    plr->shared.inGame = true;

    strcpy(clients[info->console].name, info->name);

    if(!present)
    {
        // This is a new player! Let the game know about this.
        gx.NetPlayerEvent(info->console, DDPE_ARRIVAL, 0);
    }
}

void Cl_PlayerLeaves(int plrNum)
{
    Con_Printf("Cl_PlayerLeaves: player %i has left.\n", plrNum);
    ddPlayers[plrNum].shared.inGame = false;
    gx.NetPlayerEvent(plrNum, DDPE_EXIT, 0);
}

/**
 * Client's packet handler.
 * Handles all the events the server sends.
 */
void Cl_GetPackets(void)
{
    int                 i;

    // All messages come from the server.
    while(Net_GetPacket())
    {
        // First check for packets that are only valid when
        // a game is in progress.
        if(Cl_GameReady())
        {
            boolean             handled = true;

            switch(netBuffer.msg.type)
            {
            case PSV_FRAME:
                Cl_FrameReceived();
                break;

            case PSV_FIRST_FRAME2:
            case PSV_FRAME2:
                Cl_Frame2Received(netBuffer.msg.type);
                break;

            case PKT_COORDS:
                Cl_CoordsReceived();
                break;

            case PSV_SOUND:
                Cl_Sound();
                break;

            case PSV_FILTER:
                {
                player_t*           plr = &ddPlayers[consolePlayer];
                int                 filter = Msg_ReadLong();

                if(filter)
                    plr->shared.flags |= DDPF_VIEW_FILTER;
                else
                    plr->shared.flags &= ~DDPF_VIEW_FILTER;

                plr->shared.filterColor[CR] = filter & 0xff;
                plr->shared.filterColor[CG] = (filter >> 8) & 0xff;
                plr->shared.filterColor[CB] = (filter >> 16) & 0xff;
                plr->shared.filterColor[CA] = (filter >> 24) & 0xff;
                break;
                }
            default:
                handled = false;
            }
            if(handled)
                continue; // Get the next packet.
        }

        // How about the rest?
        switch(netBuffer.msg.type)
        {
        case PSV_PLAYER_FIX:
            Cl_HandlePlayerFix();
            break;

        case PKT_DEMOCAM:
        case PKT_DEMOCAM_RESUME:
            Demo_ReadLocalCamera();
            break;

        case PKT_PING:
            Net_PingResponse();
            break;

        case PSV_SYNC:
            // The server updates our time. Latency has been taken into
            // account, so...
            gameTime = Msg_ReadLong() / 100.0;
            Con_Printf("PSV_SYNC: gameTime=%.3f\n", gameTime);
            DD_ResetTimer();
            break;

        case PSV_HANDSHAKE:
            Cl_AnswerHandshake((handshake_packet_t *) netBuffer.msg.data);
            break;

        case PKT_PLAYER_INFO:
            Cl_HandlePlayerInfo((playerinfo_packet_t *) netBuffer.msg.data);
            break;

        case PSV_PLAYER_EXIT:
            Cl_PlayerLeaves(Msg_ReadByte());
            break;

        case PKT_CHAT:
            Net_ShowChatMessage();
            gx.NetPlayerEvent(netBuffer.msg.data[0], DDPE_CHAT_MESSAGE,
                              netBuffer.msg.data + 3);
            break;

        case PSV_SERVER_CLOSE:  // We should quit?
            netLoggedIn = false;
            Con_Execute(CMDS_DDAY, "net disconnect", true, false);
            break;

        case PSV_CONSOLE_TEXT:
            i = Msg_ReadLong();
            Con_FPrintf(i, "%s", (char*)netBuffer.cursor);
            break;

        case PKT_LOGIN:
            // Server responds to our login request. Let's see if we
            // were successful.
            netLoggedIn = Msg_ReadByte();
            break;

        default:
            if(netBuffer.msg.type >= PKT_GAME_MARKER)
            {
                gx.HandlePacket(netBuffer.player, netBuffer.msg.type,
                                netBuffer.msg.data, netBuffer.length);
            }
            else
            {
#ifdef _DEBUG
Con_Printf("Cl_GetPackets: Packet (type %i) was discarded!\n",
           netBuffer.msg.type);
#endif
            }
        }
    }
}

/**
 * Client-side game ticker.
 */
void Cl_Ticker(void)
{
    //static trigger_t fixed = { 1.0 / 35 };
    static int  ticSendTimer = 0;

    if(!isClient || !Cl_GameReady() || clientPaused)
        return;

    //if(!M_RunTrigger(&fixed, time)) return;

    Cl_LocalCommand();
    Cl_PredictMovement();
    //Cl_MovePsprites();

    // Clients don't send commands on every tic (over the network).
    if(++ticSendTimer > CLIENT_TICCMD_INTERVAL)
    {
        ticSendTimer = 0;
        Net_SendCommands();
    }
}

/**
 * Clients use this to establish a remote connection to the server.
 */
D_CMD(Login)
{
    // Only clients can log in.
    if(!isClient)
        return false;

    Msg_Begin(PKT_LOGIN);
    // Write the password.
    if(argc == 1)
        Msg_WriteByte(0); // No password given!
    else
        Msg_Write(argv[1], strlen(argv[1]) + 1);
    Net_SendBuffer(0, SPF_ORDERED);
    return true;
}
