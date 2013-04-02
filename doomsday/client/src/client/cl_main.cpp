/** @file cl_main.cpp Network Client
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
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

#include "map/gamemap.h"
#include "render/r_main.h"

extern int gotFrame;

ident_t clientID;
boolean handshakeReceived = false;
int gameReady = false;
int serverTime;
boolean netLoggedIn = false; // Logged in to the server.
int clientPaused = false; // Set by the server.

void Cl_InitID(void)
{
    int                 i;
    FILE*               file;

    if((i = CommandLine_CheckWith("-id", 1)) != 0)
    {
        clientID = strtoul(CommandLine_At(i + 1), 0, 0);
        Con_Message("Cl_InitID: Using custom id 0x%08x.", clientID);
        return;
    }

    // Read the client ID number file.
    srand(time(NULL));
    if((file = fopen("client.id", "rb")) != NULL)
    {
        if(fread(&clientID, sizeof(clientID), 1, file))
        {
            clientID = ULONG(clientID);
            fclose(file);
            return;
        }
        fclose(file);
    }
    // Ah-ha, we need to generate a new ID.
    clientID = (ident_t)
        ULONG(Timer_RealMilliseconds() * rand() + (rand() & 0xfff) +
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

    if(theMap)
    {
        GameMap_DestroyClMobjs(theMap);
    }

    Cl_InitPlayers();
    Cl_WorldReset();
    GL_SetFilter(false);
}

/**
 * Sends a hello packet.
 * PCL_HELLO2 includes the Game ID (16 chars).
 */
void Cl_SendHello(void)
{
    char buf[256];

    Msg_Begin(PCL_HELLO2);
    Writer_WriteUInt32(msgWriter, clientID);

    // The game mode is included in the hello packet.
    memset(buf, 0, sizeof(buf));
    strncpy(buf, Str_Text(App_CurrentGame().identityKey()), sizeof(buf) - 1);

#ifdef _DEBUG
    Con_Message("Cl_SendHello: game mode = %s", buf);
#endif

    Writer_Write(msgWriter, buf, 16);
    Msg_End();

    Net_SendBuffer(0, 0);
}

void Cl_AnswerHandshake(void)
{
    byte remoteVersion = Reader_ReadByte(msgReader);
    byte myConsole = Reader_ReadByte(msgReader);
    uint playersInGame = Reader_ReadUInt32(msgReader);
    float remoteGameTime = Reader_ReadFloat(msgReader);
    int i;

    // Immediately send an acknowledgement. This lets the server evaluate
    // an approximate ping time.
    Msg_Begin(PCL_ACK_SHAKE);
    Msg_End();
    Net_SendBuffer(0, 0);

    // Check the version number.
    if(remoteVersion != SV_VERSION)
    {
        Con_Message("Cl_AnswerHandshake: Version conflict! (you:%i, server:%i)",
                    SV_VERSION, remoteVersion);
        Con_Execute(CMDS_DDAY, "net disconnect", false, false);
        Demo_StopPlayback();
        Con_Open(true);
        return;
    }

    // Update time and player ingame status.
    gameTime = remoteGameTime;
    for(i = 0; i < DDMAXPLAYERS; ++i)
    {
        /// @todo With multiple local players, must clear only the appropriate flags.
        ddPlayers[i].shared.flags &= ~DDPF_LOCAL;

        ddPlayers[i].shared.inGame = (playersInGame & (1 << i)) != 0;
    }
    consolePlayer = displayPlayer = myConsole;
    clients[consolePlayer].viewConsole = consolePlayer;

    // Mark us as the only local player.
    ddPlayers[consolePlayer].shared.flags |= DDPF_LOCAL;

    Smoother_Clear(clients[consolePlayer].smoother);
    ddPlayers[consolePlayer].shared.flags &= ~DDPF_USE_VIEW_FILTER;

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

    Con_Message("Cl_AnswerHandshake: myConsole:%i, remoteGameTime:%f.",
                myConsole, remoteGameTime);

    /**
     * Tell the game that we have arrived. The map will be changed when the
     * game's handshake arrives (handled in the game).
     */
    gx.NetPlayerEvent(consolePlayer, DDPE_ARRIVAL, 0);

    // Prepare the client-side data.
    Cl_InitPlayers();
    Cl_WorldInit();

    // Get ready for ticking.
    DD_ResetTimer();

    Con_Executef(CMDS_DDAY, true, "setcon %i", consolePlayer);
}

void Cl_HandlePlayerInfo(void)
{
    player_t* plr;
    boolean present;
    byte console = Reader_ReadByte(msgReader);
    size_t len = Reader_ReadUInt16(msgReader);
    char name[PLAYERNAMELEN];

    len = MIN_OF(PLAYERNAMELEN - 1, len);
    memset(name, 0, sizeof(name));
    Reader_Read(msgReader, name, len);

#ifdef _DEBUG
    Con_Message("Cl_HandlePlayerInfo: console:%i name:%s", console, name);
#endif

    // Is the console number valid?
    if(console >= DDMAXPLAYERS)
        return;

    plr = &ddPlayers[console];
    present = plr->shared.inGame;
    plr->shared.inGame = true;

    strcpy(clients[console].name, name);

    if(!present)
    {
        // This is a new player! Let the game know about this.
        gx.NetPlayerEvent(console, DDPE_ARRIVAL, 0);

        Smoother_Clear(clients[console].smoother);
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
    // All messages come from the server.
    while(Net_GetPacket())
    {
        Msg_BeginRead();

        // First check for packets that are only valid when
        // a game is in progress.
        if(Cl_GameReady())
        {
            boolean handled = true;

            switch(netBuffer.msg.type)
            {
            case PSV_FIRST_FRAME2:
            case PSV_FRAME2:
                Cl_Frame2Received(netBuffer.msg.type);
                break;

            case PSV_SOUND:
                Cl_Sound();
                break;

            default:
                handled = false;
            }

            if(handled)
            {
                Msg_EndRead();
                continue; // Get the next packet.
            }
        }

        // How about the rest?
        switch(netBuffer.msg.type)
        {
        case PSV_PLAYER_FIX:
            ClPlayer_HandleFix();
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
            gameTime = Reader_ReadFloat(msgReader);
            Con_Printf("PSV_SYNC: gameTime=%.3f\n", gameTime);
            DD_ResetTimer();
            break;

        case PSV_HANDSHAKE:
            Cl_AnswerHandshake();
            break;

        case PSV_MATERIAL_ARCHIVE:
            Cl_ReadServerMaterials();
            break;

        case PSV_MOBJ_TYPE_ID_LIST:
            Cl_ReadServerMobjTypeIDs();
            break;

        case PSV_MOBJ_STATE_ID_LIST:
            Cl_ReadServerMobjStateIDs();
            break;

        case PKT_PLAYER_INFO:
            Cl_HandlePlayerInfo();
            break;

        case PSV_PLAYER_EXIT:
            Cl_PlayerLeaves(Reader_ReadByte(msgReader));
            break;

        case PKT_CHAT:
        {
            int msgfrom = Reader_ReadByte(msgReader);
            int mask = Reader_ReadUInt32(msgReader);
            DENG_UNUSED(mask);
            size_t len = Reader_ReadUInt16(msgReader);
            char *msg = (char *) M_Malloc(len + 1);
            Reader_Read(msgReader, msg, len);
            msg[len] = 0;
            Net_ShowChatMessage(msgfrom, msg);
            gx.NetPlayerEvent(msgfrom, DDPE_CHAT_MESSAGE, msg);
            M_Free(msg);
            break;
        }

        case PSV_SERVER_CLOSE:  // We should quit?
            netLoggedIn = false;
            Con_Execute(CMDS_DDAY, "net disconnect", true, false);
            break;

        case PSV_CONSOLE_TEXT:
        {
            uint32_t conFlags = Reader_ReadUInt32(msgReader);
            uint16_t textLen = Reader_ReadUInt16(msgReader);
            char *text = (char *) M_Malloc(textLen + 1);
            Reader_Read(msgReader, text, textLen);
            text[textLen] = 0;
            Con_FPrintf(conFlags, "%s", text);
            M_Free(text);
            break;
        }

        case PKT_LOGIN:
            // Server responds to our login request. Let's see if we
            // were successful.
            netLoggedIn = Reader_ReadByte(msgReader);
            break;

        case PSV_FINALE:
            Cl_Finale(msgReader);
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
                Con_Message("Cl_GetPackets: Packet (type %i) was discarded!", netBuffer.msg.type);
#endif
            }
        }

        Msg_EndRead();
    }
}

/**
 * Check the state of the client on engineside. This is a debugging utility
 * and only gets called when _DEBUG is defined.
 */
void Cl_Assertions(int plrNum)
{
    player_t           *plr;
    mobj_t             *clmo, *mo;
    clplayerstate_t    *s;

    if(!isClient || !Cl_GameReady() || clientPaused) return;
    if(plrNum < 0 || plrNum >= DDMAXPLAYERS) return;

    plr = &ddPlayers[plrNum];
    s = ClPlayer_State(plrNum);

    // Must have a mobj!
    if(!s->clMobjId || !plr->shared.mo)
        return;

    clmo = ClMobj_Find(s->clMobjId);
    if(!clmo)
    {
        Con_Message("Cl_Assertions: client %i does not have a clmobj yet [%i].", plrNum, s->clMobjId);
        return;
    }
    mo = plr->shared.mo;

    /*
    Con_Message("Assert: client %i, clmo %i (flags 0x%x)",
                plrNum, clmo->thinker.id, clmo->ddFlags);
                */

    // Make sure the flags are correctly set for a client.
    if(mo->ddFlags & DDMF_REMOTE)
    {
        Con_Message("Cl_Assertions: client %i, mobj should not be remote!", plrNum);
    }
    if(clmo->ddFlags & DDMF_SOLID)
    {
        Con_Message("Cl_Assertions: client %i, clmobj should not be solid (when player is alive)!", plrNum);
    }
}

/**
 * Client-side game ticker.
 */
void Cl_Ticker(timespan_t ticLength)
{
    int i;

    if(!isClient || !Cl_GameReady() || clientPaused)
        return;

    // On clientside, players are represented by two mobjs: the real mobj,
    // created by the Game, is the one that is visible and modified by game
    // logic. We'll need to sync the hidden client mobj (that receives all
    // the changes from the server) to match the changes. The game ticker
    // has already been run when Cl_Ticker() is called, so let's update the
    // player's clmobj to its updated state.
    for(i = 0; i < DDMAXPLAYERS; ++i)
    {
        if(!ddPlayers[i].shared.inGame) continue;

        if(i != consolePlayer)
        {
            if(ddPlayers[i].shared.mo)
            {
                Smoother_AddPos(clients[i].smoother, Cl_FrameGameTime(),
                                ddPlayers[i].shared.mo->origin[VX],
                                ddPlayers[i].shared.mo->origin[VY],
                                ddPlayers[i].shared.mo->origin[VZ],
                                false);
            }

            // Update the smoother.
            Smoother_Advance(clients[i].smoother, ticLength);
        }

        ClPlayer_ApplyPendingFixes(i);
        ClPlayer_UpdateOrigin(i);

#ifdef _DEBUG
        Cl_Assertions(i);
#endif
    }

    if(theMap)
    {
        GameMap_ExpireClMobjs(theMap);
    }
}

/**
 * Clients use this to establish a remote connection to the server.
 */
D_CMD(Login)
{
    DENG2_UNUSED(src);

    // Only clients can log in.
    if(!isClient)
        return false;

    Msg_Begin(PKT_LOGIN);
    // Write the password.
    if(argc == 1)
    {
        Writer_WriteByte(msgWriter, 0); // No password given!
    }
    else if(strlen(argv[1]) <= 255)
    {
        Writer_WriteByte(msgWriter, strlen(argv[1]));
        Writer_Write(msgWriter, argv[1], strlen(argv[1]));
    }
    else
    {
        Msg_End();
        return false;
    }
    Msg_End();
    Net_SendBuffer(0, 0);
    return true;
}
