/** @file cl_main.cpp  Network client.
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

#include "de_base.h"
#include "client/cl_def.h"

#include "api_client.h"
#include "client/cl_frame.h"
#include "client/cl_infine.h"
#include "client/cl_player.h"
#include "client/cl_sound.h"
#include "client/cl_world.h"

#include "con_main.h"

#include "network/net_demo.h"

#include "world/map.h"
#include "world/p_players.h"

#include <de/timer.h>
#include <cstdio>
#include <cstdlib>
#include <ctime>

using namespace de;

ident_t clientID;
bool handshakeReceived;
int gameReady;
int serverTime;
bool netLoggedIn; // Logged in to the server.
int clientPaused; // Set by the server.

void Cl_InitID()
{
    if(int i = CommandLine_CheckWith("-id", 1))
    {
        clientID = strtoul(CommandLine_At(i + 1), 0, 0);
        LOG_NET_NOTE("Using custom client ID: 0x%08x") << clientID;
        return;
    }

    // Read the client ID number file.
    srand(time(NULL));
    if(FILE *file = fopen("client.id", "rb"))
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
    if(FILE *file = fopen("client.id", "wb"))
    {
        fwrite(&clientID, sizeof(clientID), 1, file);
        fclose(file);
    }
}

int Cl_GameReady()
{
    return (handshakeReceived && gameReady);
}

void Cl_CleanUp()
{
    LOG_NET_MSG("Cleaning up client state");

    clientPaused = false;
    handshakeReceived = false;

    if(App_World().hasMap())
    {
        Cl_ResetFrame();
        App_World().map().clearClMobjs();
    }

    Cl_InitPlayers();
    Cl_ResetTransTables();

    if(App_World().hasMap())
    {
        App_World().map().clearClMovers();
    }

    GL_SetFilter(false);
}

void Cl_SendHello()
{
    LOG_AS("Cl_SendHello");

    Msg_Begin(PCL_HELLO2);
    Writer_WriteUInt32(msgWriter, clientID);

    // The game mode is included in the hello packet.
    char buf[256]; zap(buf);
    strncpy(buf, App_CurrentGame().identityKey().toUtf8().constData(), sizeof(buf) - 1);

    LOGDEV_NET_VERBOSE("game mode = %s") << buf;

    Writer_Write(msgWriter, buf, 16);
    Msg_End();

    Net_SendBuffer(0, 0);
}

void Cl_AnswerHandshake()
{
    LOG_AS("Cl_AnswerHandshake");

    byte remoteVersion   = Reader_ReadByte(msgReader);
    byte myConsole       = Reader_ReadByte(msgReader);
    uint playersInGame   = Reader_ReadUInt32(msgReader);
    float remoteGameTime = Reader_ReadFloat(msgReader);

    // Immediately send an acknowledgement. This lets the server evaluate
    // an approximate ping time.
    Msg_Begin(PCL_ACK_SHAKE);
    Msg_End();
    Net_SendBuffer(0, 0);

    // Check the version number.
    if(remoteVersion != SV_VERSION)
    {
        LOG_NET_ERROR("Version conflict! (you:%i, server:%i)")
                << SV_VERSION << remoteVersion;
        Con_Execute(CMDS_DDAY, "net disconnect", false, false);
        Demo_StopPlayback();
        Con_Open(true);
        return;
    }

    // Update time and player ingame status.
    gameTime = remoteGameTime;
    for(int i = 0; i < DDMAXPLAYERS; ++i)
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

    LOGDEV_NET_MSG("Answering handshake: myConsole:%i, remoteGameTime:%.2f")
            << myConsole << remoteGameTime;

    /*
     * Tell the game that we have arrived. The map will be changed when the
     * game's handshake arrives (handled in the game).
     */
    gx.NetPlayerEvent(consolePlayer, DDPE_ARRIVAL, 0);

    // Prepare the client-side data.
    Cl_InitPlayers();
    Cl_InitTransTables();

    // Get ready for ticking.
    DD_ResetTimer();

    Con_Executef(CMDS_DDAY, true, "setcon %i", consolePlayer);
}

void Cl_HandlePlayerInfo()
{
    byte console = Reader_ReadByte(msgReader);

    size_t len = Reader_ReadUInt16(msgReader);
    len = MIN_OF(PLAYERNAMELEN - 1, len);

    char name[PLAYERNAMELEN]; zap(name);
    Reader_Read(msgReader, name, len);

    LOG_NET_VERBOSE("Player %i named \"%s\"") << console << name;

    // Is the console number valid?
    if(console >= DDMAXPLAYERS)
        return;

    player_t *plr = &ddPlayers[console];
    bool present = plr->shared.inGame;
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
    LOG_NET_NOTE("Player %i has left the game") << plrNum;
    ddPlayers[plrNum].shared.inGame = false;
    gx.NetPlayerEvent(plrNum, DDPE_EXIT, 0);
}

void Cl_GetPackets()
{
    // All messages come from the server.
    while(Net_GetPacket())
    {
        Msg_BeginRead();

        // First check for packets that are only valid when
        // a game is in progress.
        if(Cl_GameReady())
        {
            switch(netBuffer.msg.type)
            {
            case PSV_FIRST_FRAME2:
            case PSV_FRAME2:
                Cl_Frame2Received(netBuffer.msg.type);
                Msg_EndRead();
                continue; // Get the next packet.

            case PSV_SOUND:
                Cl_Sound();
                Msg_EndRead();
                continue; // Get the next packet.

            default: break;
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
            LOGDEV_NET_VERBOSE("PSV_SYNC: gameTime=%.3f") << gameTime;
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

        case PKT_CHAT: {
            int msgfrom = Reader_ReadByte(msgReader);
            int mask = Reader_ReadUInt32(msgReader);
            DENG2_UNUSED(mask);
            size_t len = Reader_ReadUInt16(msgReader);
            char *msg = (char *) M_Malloc(len + 1);
            Reader_Read(msgReader, msg, len);
            msg[len] = 0;
            Net_ShowChatMessage(msgfrom, msg);
            gx.NetPlayerEvent(msgfrom, DDPE_CHAT_MESSAGE, msg);
            M_Free(msg);
            break; }

        case PSV_SERVER_CLOSE:  // We should quit?
            netLoggedIn = false;
            Con_Execute(CMDS_DDAY, "net disconnect", true, false);
            break;

        case PSV_CONSOLE_TEXT: {
            uint32_t conFlags = Reader_ReadUInt32(msgReader);
            uint16_t textLen = Reader_ReadUInt16(msgReader);
            char *text = (char *) M_Malloc(textLen + 1);
            Reader_Read(msgReader, text, textLen);
            text[textLen] = 0;
            Con_FPrintf(conFlags, "%s", text);
            M_Free(text);
            break; }

        case PKT_LOGIN:
            // Server responds to our login request. Let's see if we were successful.
            netLoggedIn = CPP_BOOL(Reader_ReadByte(msgReader));
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
                LOG_NET_WARNING("Packet was discarded (unknown type %i)") << netBuffer.msg.type;
            }
        }

        Msg_EndRead();
    }
}

/**
 * Check the state of the client on engineside. This is a debugging utility
 * and only gets called when _DEBUG is defined.
 */
static void assertPlayerIsValid(int plrNum)
{
    LOG_AS("Client.assertPlayerIsValid");

    if(!isClient || !Cl_GameReady() || clientPaused) return;
    if(plrNum < 0 || plrNum >= DDMAXPLAYERS) return;

    player_t *plr = &ddPlayers[plrNum];
    clplayerstate_t *s = ClPlayer_State(plrNum);

    // Must have a mobj!
    if(!s->clMobjId || !plr->shared.mo)
        return;

    mobj_t *clmo = ClMobj_Find(s->clMobjId);
    if(!clmo)
    {
        LOGDEV_NET_NOTE("Player %i does not have a clmobj yet [%i]") << plrNum << s->clMobjId;
        return;
    }
    mobj_t *mo = plr->shared.mo;

    /*
    ("Assert: client %i, clmo %i (flags 0x%x)", plrNum, clmo->thinker.id, clmo->ddFlags);
    */

    // Make sure the flags are correctly set for a client.
    if(mo->ddFlags & DDMF_REMOTE)
    {
        LOGDEV_NET_NOTE("Player %i's mobj should not be remote") << plrNum;
    }
    if(clmo->ddFlags & DDMF_SOLID)
    {
        LOGDEV_NET_NOTE("Player %i's clmobj should not be solid (when player is alive)") << plrNum;
    }
}

void Cl_Ticker(timespan_t ticLength)
{
    if(!isClient || !Cl_GameReady() || clientPaused)
        return;

    // On clientside, players are represented by two mobjs: the real mobj,
    // created by the Game, is the one that is visible and modified by game
    // logic. We'll need to sync the hidden client mobj (that receives all
    // the changes from the server) to match the changes. The game ticker
    // has already been run when Cl_Ticker() is called, so let's update the
    // player's clmobj to its updated state.
    for(int i = 0; i < DDMAXPLAYERS; ++i)
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

#ifdef DENG_DEBUG
        assertPlayerIsValid(i);
#endif
    }

    if(App_World().hasMap())
    {
        App_World().map().expireClMobjs();
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
