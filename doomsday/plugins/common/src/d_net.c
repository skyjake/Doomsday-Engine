/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2009 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 2007 Jamie Jones <jamie_jones_au@yahoo.com.au>
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
 * d_net.c : Common code related to net games.
 *
 * Connecting to/from a netgame server.
 * Netgame events (player and world) and netgame commands.
 */

// HEADER FILES ------------------------------------------------------------

#if __JDOOM__
#  include "jdoom.h"
#elif __JDOOM64__
#  include "jdoom64.h"
#elif __JHERETIC__
#  include "jheretic.h"
#elif __JHEXEN__
#  include "jhexen.h"
#elif __JSTRIFE__
#  include "jstrife.h"
#endif

#include "g_common.h"
#include "d_net.h"
#include "p_player.h"
#include "hu_menu.h"
#include "p_start.h"

// MACROS ------------------------------------------------------------------

// TYPES --------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

DEFCC(CCmdSetColor);
DEFCC(CCmdSetMap);

#if __JHEXEN__
DEFCC(CCmdSetClass);
#endif

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void D_NetMessageEx(int player, const char* msg, boolean playSound);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern int netSvAllowSendMsg;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

char    msgBuff[NETBUFFER_MAXMESSAGE + 1];
float   netJumpPower = 9;

// Net code related console commands
ccmd_t netCCmds[] = {
    {"setcolor", "i", CCmdSetColor},
#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
    {"setmap", "ii", CCmdSetMap},
#else
    {"setmap", "i", CCmdSetMap},
#endif
#if __JHEXEN__
    {"setclass", "i", CCmdSetClass, CMDF_NO_DEDICATED},
#endif
    {"startcycle", "", CCmdStartMapCycle},
    {"endcycle", "", CCmdEndMapCycle},
    {NULL}
};

// Net code related console variables
cvar_t netCVars[] = {
    {"mapcycle", CVF_HIDE | CVF_NO_ARCHIVE, CVT_CHARPTR, &mapCycle},
    {"server-game-mapcycle", 0, CVT_CHARPTR, &mapCycle},
    {"server-game-mapcycle-noexit", 0, CVT_BYTE, &mapCycleNoExit, 0, 1},
    {"server-game-cheat", 0, CVT_INT, &netSvAllowCheats, 0, 1},
    {NULL}
};

// PRIVATE DATA -------------------------------------------------------------

// CODE ---------------------------------------------------------------------

/**
 * Register the console commands and variables of the common netcode.
 */
void D_NetConsoleRegistration(void)
{
    int         i;

    for(i = 0; netCCmds[i].name; ++i)
        Con_AddCommand(netCCmds + i);
    for(i = 0; netCVars[i].name; ++i)
        Con_AddVariable(netCVars + i);
}

/**
 * Called when the network server starts.
 *
 * Duties include:
 * Updating global state variables and initializing all players' settings
 */
int D_NetServerStarted(int before)
{
    int             netMap, netEpisode;

    if(before)
        return true;

    G_StopDemo();

    // We're the server, so...
    gs.players[0].color = PLR_COLOR(0, PLRPROFILE.color);

#if __JHEXEN__
    gs.players[0].pClass = PLRPROFILE.pClass;
#elif __JHERETIC__
    gs.players[0].pClass = PCLASS_PLAYER;
#endif

    // Set the game parameters.
/*  deathmatch = GAMERULES.deathmatch;
    noMonstersParm = GAMERULES.noMonsters;
    gs.cfg.jumpEnabled = GAMERULES.jumpAllow;

#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
    respawnMonsters = GAMERULES.respawn;
#endif
#if __JHEXEN__
    randomClassParm = GAMERULES.randomClass;
#endif*/

    // Hexen has translated map numbers.
#if __JHEXEN__
    netMap = P_TranslateMap(gs.netMap);
#else
    netMap = gs.netMap;
#endif

#if __JDOOM64__
    netEpisode = 1;
#else
    netEpisode = gs.netEpisode;
#endif

    G_InitNew(gs.netSkill, netEpisode, netMap);

    // Close the menu, the game begins!!
    Hu_MenuCommand(MCMD_CLOSE);
    return true;
}

/**
 * Called when a network server closes.
 *
 * Duties include:
 * Restoring global state variables
 */
int D_NetServerClose(int before)
{
    if(!before)
    {
        // Restore normal game state.
        GAMERULES.deathmatch = false;
        GAMERULES.noMonsters = false;
#if __JHEXEN__
        GAMERULES.randomClass = false;
#endif
        D_NetMessage(CONSOLEPLAYER, "NETGAME ENDS");
    }
    return true;
}

int D_NetConnect(int before)
{
    // We do nothing before the actual connection is made.
    if(before)
        return true;

    // After connecting we tell the server a bit about ourselves.
    NetCl_SendPlayerInfo();

    // Close the menu, the game begins!!
    //  advancedemo = false;
    Hu_MenuCommand(MCMD_CLOSE);
    return true;
}

int D_NetDisconnect(int before)
{
    if(before)
        return true;

    // Restore normal game state.
    GAMERULES.deathmatch = false;
    GAMERULES.noMonsters = false;
#if __JHEXEN__
    GAMERULES.randomClass = false;
#endif

    // Start demo.
    G_StartTitle();
    return true;
}

void* D_NetWriteCommands(int numCommands, void* data)
{
    // It's time to send ticcmds to the server.
    // 'plrNumber' contains the number of commands.
    return NetCl_WriteCommands(data, numCommands);
}
void* D_NetReadCommands(size_t pktLength, void* data)
{
    // Read ticcmds sent by a client.
    // 'plrNumber' is the length of the packet.
    return NetSv_ReadCommands(data, pktLength);
}

long int D_NetPlayerEvent(int plrNumber, int peType, void *data)
{
    // If this isn't a netgame, we won't react.
    if(!IS_NETGAME)
        return true;

    if(peType == DDPE_ARRIVAL)
    {
        boolean showmsg = true;

        if(IS_SERVER)
        {
            NetSv_NewPlayerEnters(plrNumber);
        }
        else if(plrNumber == CONSOLEPLAYER)
        {
            // We have arrived, the game should be begun.
            Con_Message("PE: (client) arrived in netgame.\n");
            G_ChangeGameState(GS_WAITING);
            showmsg = false;
        }
        else
        {
            // Client responds to new player?
            Con_Message("PE: (client) player %i has arrived.\n", plrNumber);
            G_DoReborn(plrNumber);
            //players[plrNumber].pState = PST_REBORN;
        }
        if(showmsg)
        {
            // Print a notification.
            snprintf(msgBuff,  NETBUFFER_MAXMESSAGE, "%s joined the game",
                    Net_GetPlayerName(plrNumber));
            D_NetMessage(CONSOLEPLAYER, msgBuff);
        }
    }
    else if(peType == DDPE_EXIT)
    {
        Con_Message("PE: player %i has left.\n", plrNumber);

        players[plrNumber].pState = PST_GONE;

        // Print a notification.
        snprintf(msgBuff,  NETBUFFER_MAXMESSAGE, "%s left the game", Net_GetPlayerName(plrNumber));
        D_NetMessage(CONSOLEPLAYER, msgBuff);

        if(IS_SERVER)
            P_DealPlayerStarts(0);
    }
    // DDPE_CHAT_MESSAGE occurs when a PKT_CHAT is received.
    // Here we will only display the message (if not a local message).
    else if(peType == DDPE_CHAT_MESSAGE && plrNumber != CONSOLEPLAYER)
    {
        int     i, num, oldecho = gs.cfg.echoMsg;

        // Count the number of players.
        for(i = num = 0; i < MAXPLAYERS; ++i)
            if(players[i].plr->inGame)
                num++;

        snprintf(msgBuff, NETBUFFER_MAXMESSAGE, "%s: %s", Net_GetPlayerName(plrNumber),
                (const char *) data);

        // The chat message is already echoed by the console.
        gs.cfg.echoMsg = false;
        D_NetMessageEx(CONSOLEPLAYER, msgBuff, (PLRPROFILE.chat.playBeep? true : false));
        gs.cfg.echoMsg = oldecho;
    }

    return true;
}

int D_NetWorldEvent(int type, int parm, void *data)
{
    int         i;

    switch (type)
    {
        //
        // Server events:
        //
    case DDWE_HANDSHAKE:
        {
        boolean             newPlayer = *((boolean*) data);

        // A new player is entering the game. We as a server should send him
        // the handshake packet(s) to update his world.
        // If 'data' is zero, this is a re-handshake that's used to
        // begin demos.
        Con_Message("D_NetWorldEvent: Sending a %shandshake to player %i.\n",
                    newPlayer ? "" : "(re)", parm);

        // Mark new player for update.
        players[parm].update |= PSF_REBORN;

        // First, the game state.
        NetSv_SendGameState(GSF_CHANGE_MAP | GSF_CAMERA_INIT |
                            (newPlayer ? 0 : GSF_DEMO), parm);

        // Send info about all players to the new one.
        for(i = 0; i < MAXPLAYERS; ++i)
            if(players[i].plr->inGame && i != parm)
                NetSv_SendPlayerInfo(i, parm);

        // Send info about our jump power.
        NetSv_SendJumpPower(parm, GAMERULES.jumpAllow ? GAMERULES.jumpPower : 0);
        NetSv_Paused(gs.paused);
        break;
        }

        //
        // Client events:
        //
#if 0
    case DDWE_PROJECTILE:
#  ifdef _DEBUG
        if(parm > 32)           // Too many?
            gi.Error("D_NetWorldEvent: Too many missiles (%i).\n", parm);
#  endif
        // Projectile data consists of shorts.
        for(ptr = *(short **) data, i = 0; i < parm; ++i)
        {
            flags = *(unsigned short *) ptr & DDMS_FLAG_MASK;
            mtype = *(unsigned short *) ptr & ~DDMS_FLAG_MASK;
            ptr++;
            x = *ptr++ << 16;
            y = *ptr++ << 16;
            z = *ptr++ << 16;
            momx = momy = momz = 0;
            if(flags & DDMS_MOVEMENT_XY)
            {
                momx = *ptr++ << 8;
                momy = *ptr++ << 8;
            }
            if(flags & DDMS_MOVEMENT_Z)
            {
                momz = *ptr++ << 8;
            }
            NetCl_SpawnMissile(mtype, x, y, z, momx, momy, momz);
        }
        // Update pointer.
        *(short **) data = ptr;
        break;
#endif

    case DDWE_SECTOR_SOUND:
        // High word: sector number, low word: sound id.
        if(parm & 0xffff)
            S_StartSound(parm & 0xffff,
                         (mobj_t *) P_GetPtr(DMU_SECTOR, parm >> 16,
                                             DMU_SOUND_ORIGIN));
        else
            S_StopSound(0, (mobj_t *) P_GetPtr(DMU_SECTOR, parm >> 16,
                                               DMU_SOUND_ORIGIN));

        break;

    case DDWE_DEMO_END:
        // Demo playback has ended. Advance demo sequence.
        if(parm)                // Playback was aborted.
            G_DemoAborted();
        else                    // Playback ended normally.
            G_DemoEnds();

        // Restore normal game state.
        GAMERULES.deathmatch = false;
        GAMERULES.noMonsters = false;
#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
        GAMERULES.respawn = false;
#endif

#if __JHEXEN__
        GAMERULES.randomClass = false;
#endif
        break;

    default:
        return false;
    }
    return true;
}

void D_HandlePacket(int fromplayer, int type, void *data, size_t length)
{
    byte           *bData = data;

    //
    // Server events.
    //
    if(IS_SERVER)
    {
        switch (type)
        {
        case GPT_PLAYER_INFO:
            // A player has changed color or other settings.
            NetSv_ChangePlayerInfo(fromplayer, data);
            break;

        case GPT_CHEAT_REQUEST:
            NetSv_DoCheat(fromplayer, data);
            break;

        case GPT_ACTION_REQUEST:
            NetSv_DoAction(fromplayer, data);
            break;
        }
        return;
    }
    //
    // Client events.
    //
    switch (type)
    {
    case GPT_GAME_STATE:
        Con_Printf("Received GTP_GAME_STATE\n");
        NetCl_UpdateGameState(data);

        // Tell the engine we're ready to proceed. It'll start handling
        // the world updates after this variable is set.
        Set(DD_GAME_READY, true);
        break;

    case GPT_MESSAGE:
        snprintf(msgBuff,  NETBUFFER_MAXMESSAGE, "%s", (char*) data);
        P_SetMessage(&players[CONSOLEPLAYER], msgBuff, false);
        break;

#if __JHEXEN__ || __JSTRIFE__
    case GPT_YELLOW_MESSAGE:
        snprintf(msgBuff,  NETBUFFER_MAXMESSAGE, "%s", (char*) data);
        P_SetYellowMessage(&players[CONSOLEPLAYER], msgBuff, false);
        break;
#endif

    case GPT_CONSOLEPLAYER_STATE:
        NetCl_UpdatePlayerState(data, CONSOLEPLAYER);
        break;

    case GPT_CONSOLEPLAYER_STATE2:
        NetCl_UpdatePlayerState2(data, CONSOLEPLAYER);
        break;

    case GPT_PLAYER_STATE:
        NetCl_UpdatePlayerState(bData + 1, bData[0]);
        break;

    case GPT_PLAYER_STATE2:
        NetCl_UpdatePlayerState2(bData + 1, bData[0]);
        break;

    case GPT_PSPRITE_STATE:
        NetCl_UpdatePSpriteState(data);
        break;

    case GPT_INTERMISSION:
        NetCl_Intermission(data);
        break;

    case GPT_FINALE:
    case GPT_FINALE2:
        NetCl_Finale(type, data);
        break;

    case GPT_PLAYER_INFO:
        NetCl_UpdatePlayerInfo(data);
        break;

#if __JHERETIC__ || __JHEXEN__ || __JSTRIFE__
    case GPT_CLASS:
        players[CONSOLEPLAYER].pClass = bData[0];
        break;
#endif

    case GPT_SAVE:
        NetCl_SaveGame(data);
        break;

    case GPT_LOAD:
        NetCl_LoadGame(data);
        break;

    case GPT_PAUSE:
        NetCl_Paused(bData[0]);
        break;

    case GPT_JUMP_POWER:
        NetCl_UpdateJumpPower(data);
        break;

    default:
        Con_Message("H_HandlePacket: Received unknown packet, " "type=%i.\n",
                    type);
    }
}

/**
 * Plays a (local) chat sound.
 */
void D_ChatSound(void)
{
#if __JHEXEN__
    S_LocalSound(SFX_CHAT, NULL);
#elif __JSTRIFE__
    S_LocalSound(SFX_CHAT, NULL);
#elif __JHERETIC__
    S_LocalSound(SFX_CHAT, NULL);
#else
# if __JDOOM__
    if(gs.gameMode == commercial)
        S_LocalSound(SFX_RADIO, NULL);
    else
# endif
        S_LocalSound(SFX_TINK, NULL);
#endif
}

/**
 * Show a message on screen, optionally accompanied by Chat sound effect.
 *
 * @param player        Player number to send the message to.
 * @param playSound     @c true = play the chat sound.
 */
static void D_NetMessageEx(int player, const char* msg, boolean playSound)
{
    player_t*           plr;

    if(player < 0 || player > MAXPLAYERS)
        return;
    plr = &players[player];

    if(!((plr->plr->flags & DDPF_LOCAL) && plr->plr->inGame))
        return;

    snprintf(msgBuff,  NETBUFFER_MAXMESSAGE, "%s", msg);

    // This is intended to be a local message.
    // Let's make sure P_SetMessage doesn't forward it anywhere.
    netSvAllowSendMsg = false;
    P_SetMessage(plr, msgBuff, false);

    if(playSound)
        D_ChatSound();

    netSvAllowSendMsg = true;
}

/**
 * Show message on screen and play chat sound.
 *
 * @param msg           Ptr to the message to print.
 */
void D_NetMessage(int player, const char* msg)
{
    D_NetMessageEx(player, msg, true);
}

/**
 * Show message on screen.
 *
 * @param msg
 */
void D_NetMessageNoSound(int player, const char* msg)
{
    D_NetMessageEx(player, msg, false);
}

/**
 * Issues a damage request when a client is trying to damage another player's
 * mobj.
 *
 * @return                  @c true = no further processing of the damage
 *                          should be done else, process the damage as
 *                          normally.
 */
boolean D_NetDamageMobj(mobj_t *target, mobj_t *inflictor, mobj_t *source,
                        int damage)
{
    if(!source || !source->player)
    {
        return false;
    }

    if(IS_SERVER && source->player - players > 0)
    {
        // A client is trying to do damage.
#if _DEBUG
Con_Message("P_DamageMobj2: Server ignores client's damage on svside.\n");
#endif
        //// \todo Damage requests have not been fully implemented yet.
        return false;
        //return true;
    }
    else if(IS_CLIENT && source->player - players == CONSOLEPLAYER)
    {
#if _DEBUG
Con_Message("P_DamageMobj2: Client should request damage on mobj %p.\n", target);
#endif
        return true;
    }

#if _DEBUG
Con_Message("P_DamageMobj2: Allowing normal damage in netgame.\n");
#endif
    // Process as normal damage.
    return false;
}

/**
 * Console command to change the players' colors.
 */
DEFCC(CCmdSetColor)
{
#if __JHEXEN__
    int                 numColors = 8;
#else
    int                 numColors = 4;
#endif

    PLRPROFILE.color = atoi(argv[1]);

    if(IS_SERVER) // A local player?
    {
        int                 player = CONSOLEPLAYER;

        if(IS_DEDICATED)
            return false;

        // Server players, must be treated as a special case because this is
        // a local mobj we're dealing with. We'll change the color translation
        // bits directly.

        gs.players[player].color = PLR_COLOR(player, PLRPROFILE.color);

        // Change the color of the mobj (translation flags).
        players[player].plr->mo->flags &= ~MF_TRANSLATION;

#if __JHEXEN__
        // Additional difficulty is caused by the fact that the Fighter's
        // colors 0 (blue) and 2 (yellow) must be swapped.
        players[player].plr->mo->flags |=
            (gs.players[player].pClass ==
             PCLASS_FIGHTER ? (gs.players[player].color == 0 ? 2 :
                               gs.players[player].color == 2 ? 0 :
                               gs.players[player].color) :
             gs.players[player].color) << MF_TRANSSHIFT;
        players[player].colorMap = gs.players[player].color;
#else
        players[player].plr->mo->flags |= gs.players[player].color << MF_TRANSSHIFT;
#endif

        // Tell the clients about the change.
        NetSv_SendPlayerInfo(player, DDSP_ALL_PLAYERS);
    }
    else
    {
        // Tell the server about the change.
        NetCl_SendPlayerInfo();
    }

    return true;
}

/**
 * Console command to change the players' class.
 */
#if __JHEXEN__
DEFCC(CCmdSetClass)
{
    playerclass_t       newClass = atoi(argv[1]);

    if(!(newClass < NUM_PLAYER_CLASSES))
        return false;

    if(!PCLASS_INFO(newClass)->userSelectable)
        return false;

    PLRPROFILE.pClass = newClass;

    if(IS_CLIENT)
    {
        // Tell the server that we've changed our class.
        NetCl_SendPlayerInfo();
    }
    else
    {
        P_PlayerChangeClass(&players[CONSOLEPLAYER], PLRPROFILE.pClass);
    }

    return true;
}
#endif

/**
 * Console command to change the current map.
 */
DEFCC(CCmdSetMap)
{
    int                 ep, map;

    // Only the server can change the map.
    if(!IS_SERVER)
        return false;
#if __JDOOM__ || __JHERETIC__
    if(argc != 3)
    {
        Con_Printf("Usage: %s (episode) (map)\n", argv[0]);
        return true;
    }
#else
    if(argc != 2)
    {
        Con_Printf("Usage: %s (map)\n", argv[0]);
        return true;
    }
#endif

    // Update game mode.
//    deathmatch = GAMERULES.deathmatch;
//    noMonstersParm = GAMERULES.noMonsters;
//    gs.cfg.jumpEnabled = GAMERULES.jumpAllow;

#if __JDOOM__ || __JHERETIC__
//    respawnMonsters = GAMERULES.respawn;
    ep = atoi(argv[1]);
    map = atoi(argv[2]);
#elif __JDOOM64__
//    respawnMonsters = GAMERULES.respawn;
    ep = 1;
    map = atoi(argv[1]);
#elif __JHEXEN__
//    randomClass = GAMERULES.randomClass;
    ep = 1;
    map = P_TranslateMap(atoi(argv[1]));
#endif

    // Use the configured network skill level for the new map.
    G_DeferedInitNew(gs.netSkill, ep, map);
    return true;
}
