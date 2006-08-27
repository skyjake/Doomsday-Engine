/* DE1: $Id$
 * Copyright (C) 2005 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * along with this program; if not: http://www.opensource.org/
 */

/*
 * d_net.c : Common code related to net games.

 * Connecting to/from a netgame server.
 * Netgame events (player and world) and netgame commands.
 */

// HEADER FILES ------------------------------------------------------------

#if  __DOOM64TC__
# include "doom64tc.h"
#elif __WOLFTC__
#  include "wolftc.h"
#elif __JDOOM__
#  include "jdoom.h"
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

// MACROS ------------------------------------------------------------------

// TYPES --------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

#if __JHERETIC__ || __JHEXEN__
void    SB_ChangePlayerClass(player_t *player, int newclass);
#endif

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

DEFCC(CCmdSetColor);
DEFCC(CCmdSetMap);

#if __JHEXEN__
DEFCC(CCmdSetClass);
#endif

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern int netSvAllowSendMsg;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

char    msgBuff[256];
float   netJumpPower = 9;

// Net code related console commands
ccmd_t netCCmds[] = {
    {"setcolor", CCmdSetColor},
    {"setmap", CCmdSetMap},
#if __JHEXEN__
    {"setclass", CCmdSetClass},
#endif
    {"startcycle", CCmdMapCycle},
    {"endcycle", CCmdMapCycle},
    {NULL}
};

// Net code related console variables
cvar_t netCVars[] = {
    {"MapCycle", CVF_HIDE | CVF_NO_ARCHIVE, CVT_CHARPTR, &mapCycle},
    {"server-game-mapcycle", 0, CVT_CHARPTR, &mapCycle},
    {"server-game-mapcycle-noexit", 0, CVT_BYTE, &mapCycleNoExit, 0, 1},
    {"server-game-cheat", 0, CVT_INT, &netSvAllowCheats, 0, 1},
    {NULL}
};

// PRIVATE DATA -------------------------------------------------------------

// CODE ---------------------------------------------------------------------

/*
 * Register the console commands and variables of the common netcode.
 */
void D_NetConsoleRegistration(void)
{
    int     i;

    for(i = 0; netCCmds[i].name; i++)
        Con_AddCommand(netCCmds + i);
    for(i = 0; netCVars[i].name; i++)
        Con_AddVariable(netCVars + i);
}

/*
 * Called when the network server starts.
 *
 * Duties include:
 *      updating global state variables
 *      initializing all players' settings
 */
int D_NetServerStarted(int before)
{
    int     netMap;

    if(before)
        return true;

    G_StopDemo();

    // We're the server, so...
    cfg.PlayerColor[0] = PLR_COLOR(0, cfg.netColor);

#if __JHEXEN__
    cfg.PlayerClass[0] = cfg.netClass;
#elif __JHERETIC__
    cfg.PlayerClass[0] = PCLASS_PLAYER;
#endif

    // Set the game parameters.
    deathmatch = cfg.netDeathmatch;
    nomonsters = cfg.netNomonsters;

    cfg.jumpEnabled = cfg.netJumping;

#if __JDOOM__
    respawnmonsters = cfg.netRespawn;
#elif __JHERETIC__
    respawnmonsters = cfg.netRespawn;
#elif __JHEXEN__
    randomclass = cfg.netRandomclass;
#endif

#ifdef __JDOOM__
    ST_updateGraphics();
#endif

    // Hexen has translated map numbers.
#ifdef __JHEXEN__
    netMap = P_TranslateMap(cfg.netMap);
#else
    netMap = cfg.netMap;
#endif
    G_InitNew(cfg.netSkill, cfg.netEpisode, netMap);

    // Close the menu, the game begins!!
    M_ClearMenus();
    return true;
}

/*
 * Called when a network server closes.
 *
 * Duties include: Restoring global state variables
 */
int D_NetServerClose(int before)
{
    if(!before)
    {
        // Restore normal game state.
        deathmatch = false;
        nomonsters = false;
#if __JHEXEN__
        randomclass = false;
#endif
        D_NetMessage("NETGAME ENDS");
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
    M_ClearMenus();
    return true;
}

int D_NetDisconnect(int before)
{
    if(before)
        return true;

    // Restore normal game state.
    deathmatch = false;
    nomonsters = false;
#if __JHEXEN__
    randomclass = false;
#endif

    // Start demo.
    G_StartTitle();
    return true;
}

long int D_NetPlayerEvent(int plrNumber, int peType, void *data)
{
    // Kludge: To preserve the ABI, these are done through player events.
    // (They are, sort of.)
    if(peType == DDPE_WRITE_COMMANDS)
    {
        // It's time to send ticcmds to the server.
        // 'plrNumber' contains the number of commands.
        return (long int) NetCl_WriteCommands(data, plrNumber);
    }
    else if(peType == DDPE_READ_COMMANDS)
    {
        // Read ticcmds sent by a client.
        // 'plrNumber' is the length of the packet.
        return (long int) NetSv_ReadCommands(data, plrNumber);
    }

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
        else if(plrNumber == consoleplayer)
        {
            // We have arrived, the game should be begun.
            Con_Message("PE: (client) arrived in netgame.\n");
            gamestate = GS_WAITING;
            showmsg = false;
        }
        else
        {
            // Client responds to new player?
            Con_Message("PE: (client) player %i has arrived.\n", plrNumber);
            G_DoReborn(plrNumber);
            //players[plrNumber].playerstate = PST_REBORN;
        }
        if(showmsg)
        {
            // Print a notification.
            sprintf(msgBuff, "%s joined the game",
                    Net_GetPlayerName(plrNumber));
            D_NetMessage(msgBuff);
        }
    }
    else if(peType == DDPE_EXIT)
    {
        Con_Message("PE: player %i has left.\n", plrNumber);

        players[plrNumber].playerstate = PST_GONE;

        // Print a notification.
        sprintf(msgBuff, "%s left the game", Net_GetPlayerName(plrNumber));
        D_NetMessage(msgBuff);

        if(IS_SERVER)
            P_DealPlayerStarts(0);
    }
    // DDPE_CHAT_MESSAGE occurs when a pkt_chat is received.
    // Here we will only display the message (if not a local message).
    else if(peType == DDPE_CHAT_MESSAGE && plrNumber != consoleplayer)
    {
        int     i, num, oldecho = cfg.echoMsg;

        // Count the number of players.
        for(i = num = 0; i < MAXPLAYERS; i++)
            if(players[i].plr->ingame)
                num++;
        // If there are more than two players, include the name of
        // the player who sent this.
        if(num > 2)
            sprintf(msgBuff, "%s: %s", Net_GetPlayerName(plrNumber),
                    (const char *) data);
        else
            strcpy(msgBuff, data);

        // The chat message is already echoed by the console.
        cfg.echoMsg = false;
        D_NetMessage(msgBuff);
        cfg.echoMsg = oldecho;
    }
    return true;
}

int D_NetWorldEvent(int type, int parm, void *data)
{
    //short *ptr;
    int     i;

    //int mtype, flags;
    //fixed_t x, y, z, momx, momy, momz;

    switch (type)
    {
        //
        // Server events:
        //
    case DDWE_HANDSHAKE:
        // A new player is entering the game. We as a server should send him
        // the handshake packet(s) to update his world.
        // If 'data' is zero, this is a re-handshake that's used to
        // begin demos.
        Con_Message("D_NetWorldEvent: Sending a %shandshake to player %i.\n",
                    data ? "" : "(re)", parm);

        // Mark new player for update.
        players[parm].update |= PSF_REBORN;

        // First, the game state.
        NetSv_SendGameState(GSF_CHANGE_MAP | GSF_CAMERA_INIT |
                            (data ? 0 : GSF_DEMO), parm);

        // Send info about all players to the new one.
        for(i = 0; i < MAXPLAYERS; i++)
            if(players[i].plr->ingame && i != parm)
                NetSv_SendPlayerInfo(i, parm);

        // Send info about our jump power.
        NetSv_SendJumpPower(parm, cfg.jumpEnabled ? cfg.jumpPower : 0);
        NetSv_Paused(paused);
        break;

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
        for(ptr = *(short **) data, i = 0; i < parm; i++)
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
        deathmatch = false;
        nomonsters = false;
#if __JDOOM__ || __JHERETIC__
        respawnmonsters = false;
#endif

#if __JHEXEN__
        randomclass = false;
#endif
        break;

    default:
        return false;
    }
    return true;
}

void D_HandlePacket(int fromplayer, int type, void *data, int length)
{
    byte   *bData = data;

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
        strcpy(msgBuff, data);
        P_SetMessage(&players[consoleplayer], msgBuff, false);

        break;

#ifndef __JDOOM__
#ifndef __JHERETIC__
    case GPT_YELLOW_MESSAGE:
        strcpy(msgBuff, data);
        P_SetYellowMessage(&players[consoleplayer], msgBuff, false);
        break;
#endif
#endif

    case GPT_CONSOLEPLAYER_STATE:
        NetCl_UpdatePlayerState(data, consoleplayer);
        break;

    case GPT_CONSOLEPLAYER_STATE2:
        NetCl_UpdatePlayerState2(data, consoleplayer);
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

#ifndef __JDOOM__
    case GPT_CLASS:
        players[consoleplayer].class = bData[0];
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

/*
 * Plays a (local) chat sound.
 */
void D_ChatSound(void)
{
#ifdef __JHEXEN__
    S_LocalSound(SFX_CHAT, NULL);
#elif __JSTRIFE__
    S_LocalSound(SFX_CHAT, NULL);
#elif __JHERETIC__
    S_LocalSound(sfx_chat, NULL);
#else
    if(gamemode == commercial)
        S_LocalSound(sfx_radio, NULL);
    else
        S_LocalSound(sfx_tink, NULL);
#endif
}

/*
 * Show a message on screen, optionally accompanied
 * by Chat sound effect.
 */
void D_NetMessageEx(char *msg, boolean play_sound)
{
    strcpy(msgBuff, msg);
    // This is intended to be a local message.
    // Let's make sure P_SetMessage doesn't forward it anywhere.
    netSvAllowSendMsg = false;
    P_SetMessage(players + consoleplayer, msgBuff, false);

    if(play_sound)
        D_ChatSound();

    netSvAllowSendMsg = true;
}

/*
 * Show message on screen, play chat sound.
 */
void D_NetMessage(char *msg)
{
    D_NetMessageEx(msg, true);
}

/*
 * Show message on screen.
 */
void D_NetMessageNoSound(char *msg)
{
    D_NetMessageEx(msg, false);
}

/*
 * Console command to change the players' colors.
 */
DEFCC(CCmdSetColor)
{
#if __JHEXEN__
    int     numColors = 8;
#elif __JSTRIFE__
    int     numColors = 8;
#else
    int     numColors = 4;
#endif

    if(argc != 2)
    {
        Con_Printf("Usage: %s (color)\n", argv[0]);
        Con_Printf("Color #%i uses the player number as color.\n", numColors);
        return true;
    }
    cfg.netColor = atoi(argv[1]);
    if(IS_SERVER)               // Player zero?
    {
        if(IS_DEDICATED)
            return false;

        // The server player, plr#0, must be treated as a special case
        // because this is a local mobj we're dealing with. We'll change
        // the color translation bits directly.

        cfg.PlayerColor[0] = PLR_COLOR(0, cfg.netColor);
#ifdef __JDOOM__
        ST_updateGraphics();
#endif
        // Change the color of the mobj (translation flags).
        players[0].plr->mo->flags &= ~MF_TRANSLATION;

#if __JHEXEN__
        // Additional difficulty is caused by the fact that the Fighter's
        // colors 0 (blue) and 2 (yellow) must be swapped.
        players[0].plr->mo->flags |=
            (cfg.PlayerClass[0] ==
             PCLASS_FIGHTER ? (cfg.PlayerColor[0] ==
                               0 ? 2 : cfg.PlayerColor[0] ==
                               2 ? 0 : cfg.PlayerColor[0]) : cfg.
             PlayerColor[0]) << MF_TRANSSHIFT;
        players[0].colormap = cfg.PlayerColor[0];
#else
        players[0].plr->mo->flags |= cfg.PlayerColor[0] << MF_TRANSSHIFT;
#endif

        // Tell the clients about the change.
        NetSv_SendPlayerInfo(0, DDSP_ALL_PLAYERS);
    }
    else
    {
        // Tell the server about the change.
        NetCl_SendPlayerInfo();
    }
    return true;
}

/*
 * Console command to change the players' class.
 */
#if __JHEXEN__
DEFCC(CCmdSetClass)
{
    if(argc != 2)
    {
        Con_Printf("Usage: %s (0-2)\n", argv[0]);
        return true;
    }
    cfg.netClass = atoi(argv[1]);
    if(cfg.netClass > 2)
        cfg.netClass = 2;
    if(IS_CLIENT)
    {
        // Tell the server that we've changed our class.
        NetCl_SendPlayerInfo();
    }
    else if(IS_DEDICATED)
    {
        return false;
    }
    else
    {
        SB_ChangePlayerClass(players + consoleplayer, cfg.netClass);
    }
    return true;
}
#endif
/*
 * Console command to change the current map.
 */
DEFCC(CCmdSetMap)
{
    int     ep, map;

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
    deathmatch = cfg.netDeathmatch;
    nomonsters = cfg.netNomonsters;

    cfg.jumpEnabled = cfg.netJumping;

#if __JDOOM__ || __JHERETIC__
    respawnmonsters = cfg.netRespawn;
    ep = atoi(argv[1]);
    map = atoi(argv[2]);
#elif __JSTRIFE__
    ep = 1;
    map = atoi(argv[1]);
#elif __JHEXEN__
    randomclass = cfg.netRandomclass;
    ep = 1;
    map = P_TranslateMap(atoi(argv[1]));
#endif

    // Use the configured network skill level for the new map.
    G_DeferedInitNew(cfg.netSkill, ep, map);
    return true;
}
