/** @file d_net.cpp  Common code related to net games.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2014 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2007 Jamie Jones <jamie_jones_au@yahoo.com.au>
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

#include "common.h"
#include "d_net.h"

#include <de/recordvalue.h>
#include "d_netcl.h"
#include "d_netsv.h"
#include "fi_lib.h"
#include "g_common.h"
#include "g_defs.h"
#include "gamesession.h"
#include "hu_menu.h"
#include "p_mapsetup.h"
#include "p_start.h"
#include "player.h"

using namespace de;
using namespace common;

D_CMD(SetColor);
#if __JHEXEN__
D_CMD(SetClass);
#endif
D_CMD(LocalMessage);

static void D_NetMessageEx(int player, const char *msg, dd_bool playSound);

float netJumpPower = 9;

static writer_s *netWriter;
static reader_s *netReader;

static void notifyAllowCheatsChange()
{
    if(IS_NETGAME && IS_NETWORK_SERVER && G_GameState() != GS_STARTUP)
    {
        NetSv_SendMessage(DDSP_ALL_PLAYERS,
                          Stringf("--- CHEATS NOW %s ON THIS SERVER ---",
                                         netSvAllowCheats ? "ENABLED" : "DISABLED"));
    }
}

String D_NetDefaultEpisode()
{
    return FirstPlayableEpisodeId();
}

res::Uri D_NetDefaultMap()
{
    const String episodeId = D_NetDefaultEpisode();

    res::Uri map("Maps:", RC_NULL);
    if(!episodeId.isEmpty())
    {
        map = res::makeUri(Defs().episodes.find("id", episodeId).gets("startMap"));
        DE_ASSERT(!map.isEmpty());
    }
    return map;
}

void D_NetConsoleRegister()
{
    C_VAR_CHARPTR("mapcycle",           &mapCycle,  CVF_HIDE | CVF_NO_ARCHIVE, 0, 0);

    C_CMD        ("setcolor",   "i",    SetColor);
#if __JHEXEN__
    C_CMD_FLAGS  ("setclass",   "i",    SetClass,   CMDF_NO_DEDICATED);
#endif
    C_CMD        ("startcycle", "",     MapCycle);
    C_CMD        ("endcycle",   "",     MapCycle);
    C_CMD        ("message",    "s",    LocalMessage);

    if(IS_DEDICATED)
    {
        C_VAR_CHARPTR("server-game-episode",    &cfg.common.netEpisode,    0, 0, 0);
        C_VAR_URIPTR ("server-game-map",        &cfg.common.netMap,        0, 0, 0);

        // Use the first playable map as the default.
        String   episodeId = D_NetDefaultEpisode();
        res::Uri map       = D_NetDefaultMap();

        Con_SetString("server-game-episode", episodeId);
        Con_SetUri   ("server-game-map",     reinterpret_cast<uri_s *>(&map));
    }

    /// @todo "server-*" cvars should only be registered by dedicated servers.
    //if(IS_DEDICATED) return;

#if !__JHEXEN__
    C_VAR_BYTE   ("server-game-announce-secret",            &cfg.secretMsg,                         0, 0, 1);
#endif
#if __JDOOM__ || __JDOOM64__
    C_VAR_BYTE   ("server-game-bfg-freeaim",                &cfg.netBFGFreeLook,                    0, 0, 1);
#endif
    C_VAR_INT2   ("server-game-cheat",                      &netSvAllowCheats,                      0, 0, 1, notifyAllowCheatsChange);
#if __JDOOM__ || __JDOOM64__
    C_VAR_BYTE   ("server-game-deathmatch",                 &cfg.common.netDeathmatch,              0, 0, 2);
#else
    C_VAR_BYTE   ("server-game-deathmatch",                 &cfg.common.netDeathmatch,              0, 0, 1);
#endif
    C_VAR_BYTE   ("server-game-jump",                       &cfg.common.netJumping,                 0, 0, 1);
    C_VAR_CHARPTR("server-game-mapcycle",                   &mapCycle,                              0, 0, 0);
    C_VAR_BYTE   ("server-game-mapcycle-noexit",            &mapCycleNoExit,                        0, 0, 1);
#if __JHERETIC__
    C_VAR_BYTE   ("server-game-maulotaur-fixfloorfire",     &cfg.fixFloorFire,                      0, 0, 1);
#endif
    C_VAR_BYTE   ("server-game-monster-meleeattack-nomaxz", &cfg.common.netNoMaxZMonsterMeleeAttack,0, 0, 1);
#if __JDOOM__ || __JDOOM64__
    C_VAR_BYTE   ("server-game-nobfg",                      &cfg.noNetBFG,                          0, 0, 1);
#endif
    C_VAR_BYTE   ("server-game-nomonsters",                 &cfg.common.netNoMonsters,              0, 0, 1);
#if !__JHEXEN__
    C_VAR_BYTE   ("server-game-noteamdamage",               &cfg.noTeamDamage,                      0, 0, 1);
#endif
#if __JHERETIC__
    C_VAR_BYTE   ("server-game-plane-fixmaterialscroll",    &cfg.fixPlaneScrollMaterialsEastOnly,   0, 0, 1);
#endif
    C_VAR_BYTE   ("server-game-radiusattack-nomaxz",        &cfg.common.netNoMaxZRadiusAttack,      0, 0, 1);
#if __JHEXEN__
    C_VAR_BYTE   ("server-game-randclass",                  &cfg.netRandomClass,                    0, 0, 1);
#endif
#if !__JHEXEN__
    C_VAR_BYTE   ("server-game-respawn",                    &cfg.netRespawn,                        0, 0, 1);
#endif
#if __JDOOM__ || __JHERETIC__
    C_VAR_BYTE   ("server-game-respawn-monsters-nightmare", &cfg.respawnMonstersNightmare,          0, 0, 1);
#endif
    C_VAR_BYTE   ("server-game-skill",                      &cfg.common.netSkill,                   0, 0, 4);

    // Modifiers:
    C_VAR_BYTE   ("server-game-mod-damage",                 &cfg.common.netMobDamageModifier,       0, 1, 100);
    C_VAR_INT    ("server-game-mod-gravity",                &cfg.common.netGravity,                 0, -1, 100);
    C_VAR_BYTE   ("server-game-mod-health",                 &cfg.common.netMobHealthModifier,       0, 1, 20);

    // Coop:
#if !__JHEXEN__
    C_VAR_BYTE   ("server-game-coop-nodamage",              &cfg.noCoopDamage,                      0, 0, 1);
#endif
#if __JDOOM__ || __JDOOM64__
    //C_VAR_BYTE   ("server-game-coop-nothing",               &cfg.noCoopAnything,                    0, 0, 1); // not implemented atm, see P_SpawnMobjXYZ
    C_VAR_BYTE   ("server-game-coop-noweapons",             &cfg.noCoopWeapons,                     0, 0, 1);
    C_VAR_BYTE   ("server-game-coop-respawn-items",         &cfg.coopRespawnItems,                  0, 0, 1);
#endif

    // Deathmatch:
#if __JDOOM__ || __JDOOM64__
    C_VAR_BYTE   ("server-game-deathmatch-killmsg",         &cfg.killMessages,                      0, 0, 1);
#endif
}

writer_s *D_NetWrite()
{
    if(netWriter)
    {
        Writer_Delete(netWriter);
    }
    netWriter = Writer_NewWithDynamicBuffer(0 /*unlimited*/);
    return netWriter;
}

reader_s *D_NetRead(const byte *buffer, size_t len)
{
    // Get rid of the old reader.
    if(netReader)
    {
        Reader_Delete(netReader);
    }
    netReader = Reader_NewWithBuffer(buffer, len);
    return netReader;
}

void D_NetClearBuffer()
{
    if(netReader) Reader_Delete(netReader);
    if(netWriter) Writer_Delete(netWriter);

    netReader = 0;
    netWriter = 0;
}

int D_NetServerStarted(int before)
{
    if(before) return true;

    // We're the server, so...
    ::cfg.playerColor[0] = PLR_COLOR(0, ::cfg.common.netColor);

#if __JHEXEN__
    ::cfg.playerClass[0] = playerclass_t(::cfg.netClass);
#elif __JHERETIC__
    ::cfg.playerClass[0] = PCLASS_PLAYER;
#endif
    P_ResetPlayerRespawnClasses();

    String episodeId = Con_GetString("server-game-episode");
    res::Uri mapUri = *reinterpret_cast<const res::Uri *>(Con_GetUri("server-game-map"));
    if(mapUri.scheme().isEmpty()) mapUri.setScheme("Maps");

    GameRules rules(gfw_Session()->rules()); // Make a copy of the current rules.
    GameRules_Set(rules, skill, skillmode_t(cfg.common.netSkill));

    gfw_Session()->end();

    try
    {
        // First try the configured map.
        gfw_Session()->begin(rules, episodeId, mapUri);
    }
    catch(const Error &er)
    {
        LOGDEV_ERROR("Failed to start server: %s") << er.asText();
        episodeId = D_NetDefaultEpisode();
        mapUri    = D_NetDefaultMap();
        LOG_INFO("Using the default map (%s) to start the server due to failure to load the configured map")
                << mapUri;

        gfw_Session()->begin(rules, episodeId, mapUri);
    }

    G_SetGameAction(GA_NONE); /// @todo Necessary?

    return true;
}

int D_NetServerClose(int before)
{
    if (!before)
    {
        P_ResetPlayerRespawnClasses();

        // Restore normal game state.
        /// @todo fixme: "normal" is defined by the game rules config which may
        /// be changed from the command line (e.g., -fast, -nomonsters).
        /// In order to "restore normal" this logic is insufficient.
        GameRules newRules(gfw_Session()->rules());
        GameRules_Set(newRules, deathmatch, 0);
        GameRules_Set(newRules, noMonsters, false);
#if __JHEXEN__
        GameRules_Set(newRules, randomClasses, false);
#endif
        gfw_Session()->applyNewRules(newRules);

        D_NetMessage(CONSOLEPLAYER, "NETGAME ENDS");
        D_NetClearBuffer();
    }
    return true;
}

int D_NetConnect(int before)
{
    if(before)
    {
        BusyMode_FreezeGameForBusyMode();
        return true;
    }

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
    {
        // Free PU_MAP, Zone-allocated storage for the local world state.
        P_ResetWorldState();
        return true;
    }

    D_NetClearBuffer();

    // Start demo.
    gfw_Session()->endAndBeginTitle();

    /*GameRules newRules(gfw_Session()->rules());
    newRules.deathmatch    = false;
    newRules.noMonsters    = false;
#if __JHEXEN__
    newRules.randomClasses = false;
#endif
    gfw_Session()->applyNewRules(newRules);*/

    return true;
}

long int D_NetPlayerEvent(int plrNumber, int peType, void *data)
{
    // If this isn't a netgame, we won't react.
    if(!IS_NETGAME)
        return true;

    if(peType == DDPE_ARRIVAL)
    {
        dd_bool showmsg = true;

        if(IS_SERVER)
        {
            NetSv_NewPlayerEnters(plrNumber);
        }
        else if(plrNumber == CONSOLEPLAYER)
        {
            // We have arrived, the game should be begun.
            App_Log(DE2_NET_NOTE, "Arrived in netgame, waiting for data...");
            G_ChangeGameState(GS_WAITING);
            showmsg = false;
        }
        else
        {
            // Client responds to new player?
            App_Log(DE2_LOG_NOTE, "Player %i has arrived in the game", plrNumber);
            P_RebornPlayerInMultiplayer(plrNumber);
            //players[plrNumber].playerstate = PST_REBORN;
        }
        if(showmsg)
        {
            // Print a notification.
            AutoStr *str = AutoStr_New();
            Str_Appendf(str, "%s joined the game", Net_GetPlayerName(plrNumber));
            D_NetMessage(CONSOLEPLAYER, Str_Text(str));
        }
    }
    else if(peType == DDPE_EXIT)
    {
        AutoStr *str = AutoStr_New();

        App_Log(DE2_LOG_NOTE, "Player %i has left the game", plrNumber);

        players[plrNumber].playerState = playerstate_t(PST_GONE);

        // Print a notification.
        Str_Appendf(str, "%s left the game", Net_GetPlayerName(plrNumber));
        D_NetMessage(CONSOLEPLAYER, Str_Text(str));

        if(IS_SERVER)
        {
            P_DealPlayerStarts(0);
        }
    }
    // DDPE_CHAT_MESSAGE occurs when a PKT_CHAT is received.
    // Here we will only display the message.
    else if(peType == DDPE_CHAT_MESSAGE)
    {
        int oldecho = cfg.common.echoMsg;
        AutoStr *msg = AutoStr_New();

        if(plrNumber > 0)
        {
            Str_Appendf(msg, "%s: %s", Net_GetPlayerName(plrNumber), (const char *) data);
        }
        else
        {
            Str_Appendf(msg, "[sysop] %s", (const char *) data);
        }
        Str_Truncate(msg, NETBUFFER_MAXMESSAGE); // not overly long, please

        // The chat message is already echoed by the console.
        cfg.common.echoMsg = false;
        D_NetMessageEx(CONSOLEPLAYER, Str_Text(msg), (cfg.common.chatBeep? true : false));
        cfg.common.echoMsg = oldecho;
    }

    return true;
}

int D_NetWorldEvent(int type, int parm, void *data)
{
    switch(type)
    {
    //
    // Server events:
    //
    case DDWE_HANDSHAKE: {
        dd_bool newPlayer = *((dd_bool*) data);

        // A new player is entering the game. We as a server should send him
        // the handshake packet(s) to update his world.
        // If 'data' is zero, this is a re-handshake that's used to
        // begin demos.
        App_Log(DE2_DEV_NET_MSG, "Sending a game state %shandshake to player %i",
                newPlayer? "" : "(re)", parm);

        // Mark new player for update.
        players[parm].update |= PSF_REBORN;

        // First, the game state.
        NetSv_SendGameState(GSF_CHANGE_MAP | GSF_CAMERA_INIT | (newPlayer ? 0 : GSF_DEMO), parm);

        // Send info about all players to the new one.
        for(int i = 0; i < MAXPLAYERS; ++i)
        {
            if(players[i].plr->inGame && i != parm)
                NetSv_SendPlayerInfo(i, parm);
        }

        // Send info about our jump power.
        NetSv_SendJumpPower(parm, cfg.common.jumpEnabled? cfg.common.jumpPower : 0);
        NetSv_Paused(paused);
        break; }

    //
    // Client events:
    //
#if 0
    case DDWE_SECTOR_SOUND:
        // High word: sector number, low word: sound id.
        if(parm & 0xffff)
            S_SectorSound(P_ToPtr(DMU_SECTOR, parm >> 16), parm & 0xffff);
        else
            S_SectorStopSounds(P_ToPtr(DMU_SECTOR, parm >> 16));

        break;

    case DDWE_DEMO_END:
        // Demo playback has ended. Advance demo sequence.
        if(parm)                // Playback was aborted.
            G_DemoAborted();
        else                    // Playback ended normally.
            G_DemoEnds();

        // Restore normal game state.
        gfw_Rule(deathmatch) = false;
        gfw_Session()->rules().noMonsters = false;
#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
        gfw_Rule(espawnMonsters) = false;
#endif

#if __JHEXEN__
        gfw_Session()->rules().randomClasses = false;
#endif
        break;
#endif

    default:
        return false;
    }

    return true;
}

void D_HandlePacket(int fromplayer, int type, void *data, size_t length)
{
    reader_s *reader = D_NetRead((byte *)data, length);

    //
    // Server events.
    //
    if(IS_SERVER)
    {
        switch (type)
        {
        case GPT_PLAYER_INFO:
            // A player has changed color or other settings.
            NetSv_ChangePlayerInfo(fromplayer, reader);
            break;

        case GPT_CHEAT_REQUEST:
            NetSv_DoCheat(fromplayer, reader);
            break;

        case GPT_FLOOR_HIT_REQUEST:
            NetSv_DoFloorHit(fromplayer, reader);
            break;

        case GPT_ACTION_REQUEST:
            NetSv_DoAction(fromplayer, reader);
            break;

        case GPT_DAMAGE_REQUEST:
            NetSv_DoDamage(fromplayer, reader);
            break;
        }
        return;
    }
    //
    // Client events.
    //
    switch(type)
    {
    case GPT_GAME_STATE:
        App_Log(DE2_DEV_NET_MSG, "Received GTP_GAME_STATE");
        NetCl_UpdateGameState(reader);

        // Tell the engine we're ready to proceed. It'll start handling
        // the world updates after this variable is set.
        DD_SetInteger(DD_GAME_READY, true);
        break;

    case GPT_PLAYER_SPAWN_POSITION:
        NetCl_PlayerSpawnPosition(reader);
        break;

    case GPT_TOTAL_COUNTS:
        NetCl_UpdateTotalCounts(reader);
        break;

    case GPT_MOBJ_IMPULSE:
        NetCl_MobjImpulse(reader);
        break;

    case GPT_LOCAL_MOBJ_STATE:
        NetCl_LocalMobjState(reader);
        break;

    case GPT_MESSAGE:
#if __JHEXEN__
    case GPT_YELLOW_MESSAGE:
#endif
    {
        size_t len = Reader_ReadUInt16(reader);
        char *msg = (char *)Z_Malloc(len + 1, PU_GAMESTATIC, 0);
        Reader_Read(reader, msg, len); msg[len] = 0;

#if __JHEXEN__
        if(type == GPT_YELLOW_MESSAGE)
        {
            P_SetYellowMessage(&players[CONSOLEPLAYER], msg);
        }
        else
#endif
        {
            P_SetMessage(&players[CONSOLEPLAYER], msg);
        }
        Z_Free(msg);
        break; }

    case GPT_MAYBE_CHANGE_WEAPON: {
        weapontype_t wt = (weapontype_t) Reader_ReadInt16(reader);
        ammotype_t at   = (ammotype_t) Reader_ReadInt16(reader);
        dd_bool force   = (Reader_ReadByte(reader) != 0);
        P_MaybeChangeWeapon(&players[CONSOLEPLAYER], wt, at, force);
        break; }

    case GPT_CONSOLEPLAYER_STATE:
        NetCl_UpdatePlayerState(reader, CONSOLEPLAYER);
        break;

    case GPT_CONSOLEPLAYER_STATE2:
        NetCl_UpdatePlayerState2(reader, CONSOLEPLAYER);
        break;

    case GPT_PLAYER_STATE:
        NetCl_UpdatePlayerState(reader, -1);
        break;

    case GPT_PLAYER_STATE2:
        NetCl_UpdatePlayerState2(reader, -1);
        break;

    case GPT_PSPRITE_STATE:
        NetCl_UpdatePSpriteState(reader);
        break;

    case GPT_INTERMISSION:
        NetCl_Intermission(reader);
        break;

    case GPT_FINALE_STATE:
        NetCl_UpdateFinaleState(reader);
        break;

    case GPT_PLAYER_INFO:
        NetCl_UpdatePlayerInfo(reader);
        break;

#if __JHERETIC__ || __JHEXEN__ || __JSTRIFE__
    case GPT_CLASS: {
        player_t *plr = &players[CONSOLEPLAYER];
        playerclass_t newClass = playerclass_t(Reader_ReadByte(reader));
# if __JHERETIC__
        playerclass_t oldClass = plr->class_;
# endif
        plr->class_ = newClass;
        App_Log(DE2_DEV_MAP_MSG, "Player %i class changed to %i", CONSOLEPLAYER, plr->class_);

# if __JHERETIC__
        if(oldClass != newClass)
        {
            if(newClass == PCLASS_CHICKEN)
            {
                App_Log(DE2_DEV_MAP_VERBOSE, "Player %i activating morph", CONSOLEPLAYER);

                P_ActivateMorphWeapon(plr);
            }
            else if(oldClass == PCLASS_CHICKEN)
            {
                App_Log(DE2_DEV_MAP_VERBOSE, "Player %i post-morph weapon %i", CONSOLEPLAYER, plr->readyWeapon);

                // The morph has ended.
                P_PostMorphWeapon(plr, plr->readyWeapon);
            }
        }
# endif
        break; }
#endif

    case GPT_SAVE:
        NetCl_SaveGame(reader);
        break;

    case GPT_LOAD:
        NetCl_LoadGame(reader);
        break;

    case GPT_PAUSE:
        NetCl_Paused(reader);
        break;

    case GPT_JUMP_POWER:
        NetCl_UpdateJumpPower(reader);
        break;

    case GPT_DISMISS_HUDS:
        NetCl_DismissHUDs(reader);
        break;

    default:
        App_Log(DE2_NET_WARNING, "Game received unknown packet (type:%i)", type);
    }
}

/**
 * Plays a (local) chat sound.
 */
void D_ChatSound()
{
#if __JHEXEN__
    S_LocalSound(SFX_CHAT, NULL);
#elif __JSTRIFE__
    S_LocalSound(SFX_CHAT, NULL);
#elif __JHERETIC__
    S_LocalSound(SFX_CHAT, NULL);
#else
# if __JDOOM__
    if(gameModeBits & GM_ANY_DOOM2)
        S_LocalSound(SFX_RADIO, NULL);
    else
# endif
        S_LocalSound(SFX_TINK, NULL);
#endif
}

/**
 * Show a message on screen, optionally accompanied by Chat sound effect.
 *
 * @param player     Player number to send the message to.
 * @param playSound  @c true = play the chat sound.
 */
static void D_NetMessageEx(int player, const char *msg, dd_bool playSound)
{
    if(player < 0 || player > MAXPLAYERS) return;
    player_t *plr = &players[player];

    if(!plr->plr->inGame)
        return;

    // This is intended to be a local message.
    // Let's make sure P_SetMessageWithFlags doesn't forward it anywhere.
    netSvAllowSendMsg = false;
    P_SetMessage(plr, msg);

    if(playSound)
    {
        D_ChatSound();
    }

    netSvAllowSendMsg = true;
}

void D_NetMessage(int player, const char *msg)
{
    D_NetMessageEx(player, msg, true);
}

void D_NetMessageNoSound(int player, const char *msg)
{
    D_NetMessageEx(player, msg, false);
}

dd_bool D_NetDamageMobj(mobj_t *target, mobj_t *inflictor, mobj_t *source, int damage)
{
    int sourcePlrNum = -1;
    if(source && source->player)
    {
        sourcePlrNum = source->player - players;
    }

    if(source && !source->player)
    {
        // Not applicable: only damage from players.
        return false;
    }

    if(IS_SERVER && sourcePlrNum > 0)
    {
        /*
         * A client is trying to do damage. However, it is not guaranteed that the server is 100%
         * accurately aware of the gameplay situation in which the damage is being inflicted (due
         * to network latency), so instead of applying the damage now we will wait for the client
         * to request it separately.
         */
        return false;
    }
    else if(IS_CLIENT)
    {
        if((sourcePlrNum < 0 || sourcePlrNum == CONSOLEPLAYER) &&
           target && target->player && target->player - players == CONSOLEPLAYER)
        {
            // Clients are allowed to damage themselves.
            NetCl_DamageRequest(ClPlayer_ClMobj(CONSOLEPLAYER), inflictor, source, damage);

            // No further processing of this damage is needed.
            return true;
        }
    }
    return false;
}

/**
 * Console command to change the players' colors.
 */
D_CMD(SetColor)
{
    DE_UNUSED(src, argc);

    cfg.common.netColor = atoi(argv[1]);
    if(IS_SERVER) // A local player?
    {
        if(IS_DEDICATED) return false;

        int player = CONSOLEPLAYER;

        // Server players, must be treated as a special case because this is
        // a local mobj we're dealing with. We'll change the color translation
        // bits directly.

        cfg.playerColor[player] = PLR_COLOR(player, cfg.common.netColor);
        players[player].colorMap = cfg.playerColor[player];

        if(players[player].plr->mo)
        {
            // Change the color of the mobj (translation flags).
            players[player].plr->mo->flags &= ~MF_TRANSLATION;
            players[player].plr->mo->flags |= (cfg.playerColor[player] << MF_TRANSSHIFT);
        }

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
D_CMD(SetClass)
{
    DE_UNUSED(src, argc);

    playerclass_t newClass = playerclass_t(atoi(argv[1]));

    if(!(newClass < NUM_PLAYER_CLASSES))
    {
        return false;
    }

    if(!PCLASS_INFO(newClass)->userSelectable)
    {
        return false;
    }

    cfg.netClass = newClass; // Stored as a cvar.

    if(IS_CLIENT)
    {
        // Tell the server that we want to change our class.
        NetCl_SendPlayerInfo();
    }
    else
    {
        // On the server (or singleplayer) we can do an immediate change.
        P_PlayerChangeClass(&players[CONSOLEPLAYER], playerclass_t(cfg.netClass));
    }

    return true;
}
#endif

/**
 * Post a local game message.
 */
D_CMD(LocalMessage)
{
    DE_UNUSED(src, argc);
    D_NetMessageNoSound(CONSOLEPLAYER, argv[1]);
    return true;
}
