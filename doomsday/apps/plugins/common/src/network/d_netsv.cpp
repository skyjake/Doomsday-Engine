/** @file d_netsv.cpp  Common code related to net games (server-side).
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2014 Daniel Swanson <danij@dengine.net>
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
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#include "common.h"
#include "d_netsv.h"

#include <cctype>
#include <cstdio>
#include <cstring>
#include <de/mathutil.h>

#include "d_net.h"
#include "gamesession.h"
#include "player.h"
#include "p_user.h"
#include "p_map.h"
#include "mobj.h"
#include "p_actor.h"
#include "g_common.h"
#include "g_defs.h"
#include "p_tick.h"
#include "p_start.h"
#include "p_inventory.h"

#ifdef __JHEXEN__
#  include "s_sequence.h"
#endif

using namespace de;
using namespace common;

#if __JHEXEN__
#  define SOUND_COUNTDOWN       SFX_PICKUP_KEY
#elif __JDOOM__ || __JDOOM64__
#  define SOUND_COUNTDOWN       SFX_GETPOW
#elif __JHERETIC__
#  define SOUND_COUNTDOWN       SFX_KEYUP
#endif

#define SOUND_VICTORY           SOUND_COUNTDOWN

struct maprule_t
{
    dd_bool     usetime, usefrags;
    int         time;           // Minutes.
    int         frags;          // Maximum frags for one player.
};

typedef enum cyclemode_s {
    CYCLE_IDLE,
    CYCLE_COUNTDOWN
} cyclemode_t;

void R_SetAllDoomsdayFlags();

/**
 * Calculates the frags of player 'pl'.
 */
int NetSv_GetFrags(int pl);

void NetSv_MapCycleTicker(void);

void NetSv_SendPlayerClass(int pnum, char cls);

char cyclingMaps;
char *mapCycle = (char *)"";
char mapCycleNoExit = true;
int netSvAllowSendMsg = true;
int netSvAllowCheats;

// This is returned in *_Get(DD_GAME_CONFIG). It contains a combination
// of space-separated keywords.
char gameConfigString[128];

static int cycleIndex;
static int cycleCounter = -1, cycleMode = CYCLE_IDLE;
static int cycleRulesCounter[MAXPLAYERS];

#if __JHERETIC__ || __JHEXEN__ || __JSTRIFE__
static int oldClasses[MAXPLAYERS];
#endif

void NetSv_UpdateGameConfigDescription()
{
    if (IS_CLIENT) return;

    GameRules const &gameRules = gfw_Session()->rules();

    QByteArray str = "skill" + QByteArray::number(gameRules.values.skill + 1);

    if (gameRules.values.deathmatch > 1)
    {
        str += " dm" + QByteArray::number(gameRules.values.deathmatch);
    }
    else if (gameRules.values.deathmatch)
    {
        str += " dm";
    }
    else
    {
        str += " coop";
    }

    if (gameRules.values.noMonsters)
    {
        str += " nomonst";
    }
#if !__JHEXEN__
    if (gameRules.values.respawnMonsters)
    {
        str += " respawn";
    }
#endif

    if (cfg.common.jumpEnabled)
    {
        str += " jump";
    }

    strcpy(gameConfigString, str);
}

void NetSv_Ticker()
{
    // Map rotation checker.
    NetSv_MapCycleTicker();

    // This is done here for servers.
    R_SetAllDoomsdayFlags();

    // Set the camera filters for players.
    for(int i = 0; i < MAXPLAYERS; ++i)
    {
        R_UpdateViewFilter(i);
    }

#ifdef __JHEXEN__
    SN_UpdateActiveSequences();
#endif

    // Inform clients about jumping?
    float power = (cfg.common.jumpEnabled ? cfg.common.jumpPower : 0);
    if(power != netJumpPower)
    {
        netJumpPower = power;
        for(int i = 0; i < MAXPLAYERS; ++i)
        {
            if(players[i].plr->inGame)
            {
                NetSv_SendJumpPower(i, power);
            }
        }
    }

    // Send the player state updates.
    for(int i = 0; i < MAXPLAYERS; ++i)
    {
        player_t *plr = &players[i];

        if(!plr->plr->inGame)
            continue;

        if(plr->update)
        {
            // Owned weapons and player state will be sent in the v2 packet.
            if(plr->update & (PSF_OWNED_WEAPONS | PSF_STATE))
            {
                int flags =
                    (plr->update & PSF_OWNED_WEAPONS ? PSF2_OWNED_WEAPONS : 0) |
                    (plr->update & PSF_STATE ? PSF2_STATE : 0);

                NetSv_SendPlayerState2(i, i, flags, true);

                plr->update &= ~(PSF_OWNED_WEAPONS | PSF_STATE);
                // That was all?
                if(!plr->update)
                {
                    continue;
                }
            }

            NetSv_SendPlayerState(i, i, plr->update, true);
            plr->update = 0;
        }

#if __JHERETIC__ || __JHEXEN__
        // Keep track of player class changes (fighter, cleric, mage, pig).
        // Notify clients accordingly. This is mostly just FYI (it'll update
        // pl->class_ on the clientside).
        if(oldClasses[i] != plr->class_)
        {
            oldClasses[i] = plr->class_;
            NetSv_SendPlayerClass(i, plr->class_);
        }
#endif
    }
}

static void NetSv_CycleToMapNum(de::Uri const &mapUri)
{
    de::String const warpCommand = de::String("warp ") + mapUri.compose(de::Uri::DecodePath);
    DD_Execute(false, warpCommand.toUtf8().constData());

    // In a couple of seconds, send everyone the rules of this map.
    for(int i = 0; i < MAXPLAYERS; ++i)
    {
        cycleRulesCounter[i] = 3 * TICSPERSEC;
    }

    cycleMode    = CYCLE_IDLE;
    cycleCounter = 0;
}

/**
 * Reads through the MapCycle cvar and finds the map with the given index.
 * Rules that apply to the map are returned in 'rules'.
 *
 * @pre The game session has already begun. Necessary because the cycle rules
 * for Hexen reference maps by "warp numbers", which, can only be resolved in
 * the context of an episode.
 */
static de::Uri NetSv_ScanCycle(int index, maprule_t *rules = 0)
{
    bool clear = false, has_random = false;

    maprule_t dummy;
    if(!rules) rules = &dummy;

    // By default no rules apply.
    rules->usetime = rules->usefrags = false;

    int pos = -1;
    for(char *ptr = mapCycle; *ptr; ptr++)
    {
        if(isspace(*ptr)) continue;

        if(*ptr == ',' || *ptr == '+' || *ptr == ';' || *ptr == '/' ||
           *ptr == '\\')
        {
            // These symbols are allowed to combine "time" and "frags".
            // E.g. "Time:10/Frags:5" or "t:30, f:10"
            clear = false;
        }
        else if(!qstrnicmp("time", ptr, 1))
        {
            // Find the colon.
            while(*ptr && *ptr != ':') { ptr++; }

            if(!*ptr) break;

            if(clear)
            {
                rules->usefrags = false;
            }
            clear = true;

            char *end;
            rules->usetime = true;
            rules->time    = strtol(ptr + 1, &end, 0);

            ptr = end - 1;
        }
        else if(!qstrnicmp("frags", ptr, 1))
        {
            // Find the colon.
            while(*ptr && *ptr != ':') { ptr++; }

            if(!*ptr) break;

            if(clear)
            {
                rules->usetime = false;
            }
            clear = true;

            char *end;
            rules->usefrags = true;
            rules->frags    = strtol(ptr + 1, &end, 0);

            ptr = end - 1;
        }
        else if(*ptr == '*' || (*ptr >= '0' && *ptr <= '9'))
        {
            // A map identifier is here.
            pos++;

            // Read it.
            char tmp[3];
            tmp[0] = *ptr++;
            tmp[1] = de::clamp('0', *ptr, '9');
            tmp[2] = 0;

            if(strlen(tmp) < 2)
            {
                // Assume a zero is missing.
                tmp[1] = tmp[0];
                tmp[0] = '0';
            }

            if(index == pos)
            {
                // Are there randomized components?
                // (If so make many passes to try to find an existing map).
                if(tmp[0] == '*' || tmp[1] == '*')
                {
                    has_random = true;
                }

                for(int pass = 0; pass < (has_random? 100 : 1); ++pass)
                {
                    uint map = 0;
#if !__JHEXEN__
                    uint episode = 0;
#endif
#if __JDOOM__
                    if(!(gameModeBits & GM_ANY_DOOM2))
                    {
                        episode = tmp[0] == '*' ? RNG_RandByte() % 9 : tmp[0] - '0';
                        map     = tmp[1] == '*' ? RNG_RandByte() % 9 : tmp[1] - '0';
                    }
                    else
                    {
                        int tens = tmp[0] == '*' ? RNG_RandByte() % 10 : tmp[0] - '0';
                        int ones = tmp[1] == '*' ? RNG_RandByte() % 10 : tmp[1] - '0';
                        map = de::String("%1%2").arg(tens).arg(ones).toInt();
                    }
#elif __JHERETIC__
                    episode = tmp[0] == '*' ? RNG_RandByte() % 9 : tmp[0] - '0';
                    map     = tmp[1] == '*' ? RNG_RandByte() % 9 : tmp[1] - '0';

#else // __JHEXEN__ || __JDOOM64__
                    int tens = tmp[0] == '*' ? RNG_RandByte() % 10 : tmp[0] - '0';
                    int ones = tmp[1] == '*' ? RNG_RandByte() % 10 : tmp[1] - '0';
                    map = de::String("%1%2").arg(tens).arg(ones).toInt();
#endif

#if __JHEXEN__
                    // In Hexen map numbers must be translated (urgh...).
                    de::Uri mapUri = TranslateMapWarpNumber(gfw_Session()->episodeId(), map);
#else
                    de::Uri mapUri = G_ComposeMapUri(episode, map);
#endif

                    if(P_MapExists(mapUri.compose().toUtf8().constData()))
                    {
                        return mapUri;
                    }
                }

                // This was the map we were looking for...
                break;
            }
        }
    }

    // Didn't find it.
    return de::Uri();
}

void NetSv_TellCycleRulesToPlayerAfterTics(int destPlr, int tics)
{
    if(destPlr >= 0 && destPlr < MAXPLAYERS)
    {
        cycleRulesCounter[destPlr] = tics;
    }
    else if((unsigned)destPlr == DDSP_ALL_PLAYERS)
    {
        for(int i = 0; i < MAXPLAYERS; ++i)
        {
            cycleRulesCounter[i] = tics;
        }
    }
}

/**
 * Sends a message about the map cycle rules to a specific player.
 */
void NetSv_TellCycleRulesToPlayer(int destPlr)
{
    if(!cyclingMaps) return;

    LOGDEV_NET_VERBOSE("NetSv_TellCycleRulesToPlayer: %i") << destPlr;

    // Get the rules of the current map.
    maprule_t rules;
    NetSv_ScanCycle(cycleIndex, &rules);

    char msg[100];
    strcpy(msg, "MAP RULES: ");
    if(!rules.usetime && !rules.usefrags)
    {
        strcat(msg, "NONE");
    }
    else
    {
        char tmp[100];
        if(rules.usetime)
        {
            sprintf(tmp, "%i MINUTES", rules.time);
            strcat(msg, tmp);
        }
        if(rules.usefrags)
        {
            sprintf(tmp, "%s%i FRAGS", rules.usetime ? " OR " : "", rules.frags);
            strcat(msg, tmp);
        }
    }

    NetSv_SendMessage(destPlr, msg);
}

void NetSv_MapCycleTicker()
{
    if(!cyclingMaps) return;

    // Check rules broadcasting.
    for(int i = 0; i < MAXPLAYERS; ++i)
    {
        if(!cycleRulesCounter[i] || !players[i].plr->inGame)
            continue;

        if(--cycleRulesCounter[i] == 0)
        {
            NetSv_TellCycleRulesToPlayer(i);
        }
    }

    cycleCounter--;

    switch(cycleMode)
    {
    case CYCLE_IDLE:
        // Check if the current map should end.
        if(cycleCounter <= 0)
        {
            // Test again in ten seconds time.
            cycleCounter = 10 * TICSPERSEC;

            maprule_t rules;
            if(NetSv_ScanCycle(cycleIndex, &rules).path().isEmpty())
            {
                if(NetSv_ScanCycle(cycleIndex = 0, &rules).path().isEmpty())
                {
                    // Hmm?! Abort cycling.
                    LOG_MAP_WARNING("All of a sudden MapCycle is invalid; stopping cycle");
                    DD_Execute(false, "endcycle");
                    return;
                }
            }

            if(rules.usetime &&
               mapTime > (rules.time * 60 - 29) * TICSPERSEC)
            {
                // Time runs out!
                cycleMode = CYCLE_COUNTDOWN;
                cycleCounter = 31 * TICSPERSEC;
            }

            if(rules.usefrags)
            {
                for(int i = 0; i < MAXPLAYERS; i++)
                {
                    if(!players[i].plr->inGame)
                        continue;

                    int frags = NetSv_GetFrags(i);
                    if(frags >= rules.frags)
                    {
                        char msg[100]; sprintf(msg, "--- %s REACHES %i FRAGS ---", Net_GetPlayerName(i), frags);
                        NetSv_SendMessage(DDSP_ALL_PLAYERS, msg);
                        S_StartSound(SOUND_VICTORY, NULL);

                        cycleMode = CYCLE_COUNTDOWN;
                        cycleCounter = 15 * TICSPERSEC; // No msg for 15 secs.
                        break;
                    }
                }
            }
        }
        break;

    case CYCLE_COUNTDOWN:
        if(cycleCounter == 30 * TICSPERSEC ||
           cycleCounter == 15 * TICSPERSEC ||
           cycleCounter == 10 * TICSPERSEC ||
           cycleCounter == 5 * TICSPERSEC)
        {
            char msg[100]; sprintf(msg, "--- WARPING IN %i SECONDS ---", cycleCounter / TICSPERSEC);
            NetSv_SendMessage(DDSP_ALL_PLAYERS, msg);
            // Also, a warning sound.
            S_StartSound(SOUND_COUNTDOWN, NULL);
        }
        else if(cycleCounter <= 0)
        {
            // Next map, please!
            de::Uri mapUri = NetSv_ScanCycle(++cycleIndex);
            if(mapUri.path().isEmpty())
            {
                // Must be past the end?
                mapUri = NetSv_ScanCycle(cycleIndex = 0);
                if(mapUri.path().isEmpty())
                {
                    // Hmm?! Abort cycling.
                    LOG_MAP_WARNING("All of a sudden MapCycle is invalid; stopping cycle");
                    DD_Execute(false, "endcycle");
                    return;
                }
            }

            // Warp to the next map. Don't bother with the intermission.
            NetSv_CycleToMapNum(mapUri);
        }
        break;
    }
}

void NetSv_ResetPlayerFrags(int plrNum)
{
    LOGDEV_NET_VERBOSE("NetSv_ResetPlayerFrags: Player %i") << plrNum;

    player_t *plr = &players[plrNum];
    de::zap(plr->frags);

    // The frag count is dependent on the others' frags.
    for(int i = 0; i < MAXPLAYERS; ++i)
    {
        players[i].frags[plrNum] = 0;

        // Everybody will get their frags updated.
        players[i].update |= PSF_FRAGS;
    }
}

void NetSv_NewPlayerEnters(int plrNum)
{
    LOGDEV_MSG("NetSv_NewPlayerEnters: player %i") << plrNum;

    player_t *plr = &players[plrNum];
    plr->playerState = PST_REBORN;  // Force an init.

    // Re-deal player starts.
    P_DealPlayerStarts(0);

    // Reset the player's frags.
    NetSv_ResetPlayerFrags(plrNum);

    // Spawn the player into the world.
    if(gfw_Rule(deathmatch))
    {
        G_DeathMatchSpawnPlayer(plrNum);
    }
    else
    {
        playerclass_t pClass = P_ClassForPlayerWhenRespawning(plrNum, false);
        playerstart_t const *start;

        if((start = P_GetPlayerStart(gfw_Session()->mapEntryPoint(), plrNum, false)))
        {
            mapspot_t const *spot = &mapSpots[start->spot];

            LOGDEV_MAP_MSG("NetSv_NewPlayerEnters: Spawning player with angle:%x") << spot->angle;

            P_SpawnPlayer(plrNum, pClass, spot->origin[VX], spot->origin[VY],
                          spot->origin[VZ], spot->angle, spot->flags,
                          false, true);
        }
        else
        {
            P_SpawnPlayer(plrNum, pClass, 0, 0, 0, 0, MSF_Z_FLOOR, true, true);
        }

        /// @todo Spawn a telefog in front of the player.
    }

    // Get rid of anybody at the starting spot.
    P_Telefrag(plr->plr->mo);

    NetSv_TellCycleRulesToPlayerAfterTics(plrNum, 5 * TICSPERSEC);
    NetSv_SendTotalCounts(plrNum);
}

void NetSv_Intermission(int flags, int state, int time)
{
    if(IS_CLIENT) return;

    writer_s *msg = D_NetWrite();
    Writer_WriteByte(msg, flags);

    /// @todo jHeretic does not transmit the intermission info!
#if !defined(__JHERETIC__)
    if(flags & IMF_BEGIN)
    {
        // Only include the necessary information.
#  if __JDOOM__ || __JDOOM64__
        Writer_WriteUInt16(msg, ::wmInfo.maxKills);
        Writer_WriteUInt16(msg, ::wmInfo.maxItems);
        Writer_WriteUInt16(msg, ::wmInfo.maxSecret);
#  endif
        Uri_Write(reinterpret_cast<uri_s *>(&::wmInfo.nextMap), msg);
#  if __JHEXEN__
        Writer_WriteByte(msg, ::wmInfo.nextMapEntryPoint);
#  else
        Uri_Write(reinterpret_cast<uri_s *>(&::wmInfo.currentMap), msg);
#  endif
#  if __JDOOM__ || __JDOOM64__
        Writer_WriteByte(msg, ::wmInfo.didSecret);
#  endif
    }
#  endif

    if(flags & IMF_STATE)
    {
        Writer_WriteInt16(msg, state);
    }

    if(flags & IMF_TIME)
    {
        Writer_WriteInt16(msg, time);
    }

    Net_SendPacket(DDSP_ALL_PLAYERS, GPT_INTERMISSION, Writer_Data(msg), Writer_Size(msg));
}

void NetSv_SendTotalCounts(int to)
{
    // Hexen does not have total counts.
#ifndef __JHEXEN__
    if(IS_CLIENT) return;

    writer_s *writer = D_NetWrite();
    Writer_WriteInt32(writer, totalKills);
    Writer_WriteInt32(writer, totalItems);
    Writer_WriteInt32(writer, totalSecret);

    // Send the packet.
    Net_SendPacket(to, GPT_TOTAL_COUNTS, Writer_Data(writer), Writer_Size(writer));
#else
    DENG2_UNUSED(to);
#endif
}

void NetSv_SendGameState(int flags, int to)
{
    if(!IS_NETWORK_SERVER) return;

    AutoStr *gameId    = AutoStr_FromTextStd(gfw_GameId().toLatin1().constData());
    AutoStr *episodeId = AutoStr_FromTextStd(gfw_Session()->episodeId().toLatin1().constData());
    de::Uri mapUri     = gfw_Session()->mapUri();

    // Print a short message that describes the game state.
    LOG_NET_NOTE("Sending game setup: %s %s %s %s")
            << Str_Text(gameId)
            << Str_Text(episodeId)
            << mapUri.resolved()
            << gameConfigString;

    // Send an update to all the players in the game.
    for(int i = 0; i < MAXPLAYERS; ++i)
    {
        if(!players[i].plr->inGame) continue;
        if((unsigned)to != DDSP_ALL_PLAYERS && to != i) continue;

        writer_s *writer = D_NetWrite();
        Writer_WriteByte(writer, flags);

        // Game identity key.
        Str_Write(gameId, writer);

        // Current map.
        Uri_Write(reinterpret_cast<uri_s *>(&mapUri), writer);

        // Current episode.
        Str_Write(episodeId, writer);

        // Old map number. Presently unused.
        Writer_WriteByte(writer, 0);

        Writer_WriteByte(writer, (gfw_Rule(deathmatch) & 0x3)
            | (!gfw_Rule(noMonsters)? 0x4 : 0)
#if !__JHEXEN__
            | (gfw_Rule(respawnMonsters)? 0x8 : 0)
#else
            | 0
#endif
            | (cfg.common.jumpEnabled? 0x10 : 0));

        // Note that SM_NOTHINGS will result in a value of '7'.
        Writer_WriteByte(writer, gfw_Rule(skill) & 0x7);
        Writer_WriteFloat(writer, (float)P_GetGravity());

        if(flags & GSF_CAMERA_INIT)
        {
            mobj_t *mo = players[i].plr->mo;
            Writer_WriteFloat(writer, mo->origin[VX]);
            Writer_WriteFloat(writer, mo->origin[VY]);
            Writer_WriteFloat(writer, mo->origin[VZ]);
            Writer_WriteUInt32(writer, mo->angle);
        }

        // Send the packet.
        Net_SendPacket(i, GPT_GAME_STATE, Writer_Data(writer), Writer_Size(writer));
    }
}

void NetSv_PlayerMobjImpulse(mobj_t *mobj, float mx, float my, float mz)
{
    if(!IS_SERVER) return;

    if(!mobj || !mobj->player) return;

    // Which player?
    int plrNum = mobj->player - players;

    writer_s *writer = D_NetWrite();
    Writer_WriteUInt16(writer, mobj->thinker.id);
    Writer_WriteFloat(writer, mx);
    Writer_WriteFloat(writer, my);
    Writer_WriteFloat(writer, mz);

    Net_SendPacket(plrNum, GPT_MOBJ_IMPULSE, Writer_Data(writer), Writer_Size(writer));
}

void NetSv_DismissHUDs(int plrNum, dd_bool fast)
{
    if(!IS_SERVER) return;
    if(plrNum < 1 || plrNum >= DDMAXPLAYERS) return;

    writer_s *writer = D_NetWrite();
    Writer_WriteByte(writer, fast? 1 : 0);

    Net_SendPacket(plrNum, GPT_DISMISS_HUDS, Writer_Data(writer), Writer_Size(writer));
}

void NetSv_SendPlayerSpawnPosition(int plrNum, float x, float y, float z, int angle)
{
    if(!IS_SERVER) return;

    LOGDEV_NET_MSG("NetSv_SendPlayerSpawnPosition: Player #%i pos:%s angle:%x")
            << plrNum << de::Vector3f(x, y, z).asText() << angle;

    writer_s *writer = D_NetWrite();
    Writer_WriteFloat(writer, x);
    Writer_WriteFloat(writer, y);
    Writer_WriteFloat(writer, z);
    Writer_WriteUInt32(writer, angle);

    Net_SendPacket(plrNum, GPT_PLAYER_SPAWN_POSITION,
                   Writer_Data(writer), Writer_Size(writer));
}

void NetSv_SendPlayerState2(int srcPlrNum, int destPlrNum, int flags, dd_bool /*reliable*/)
{
    int pType = (srcPlrNum == destPlrNum ? GPT_CONSOLEPLAYER_STATE2 : GPT_PLAYER_STATE2);
    player_t *pl = &players[srcPlrNum];

    // Check that this is a valid call.
    if(IS_CLIENT || !pl->plr->inGame ||
       (destPlrNum >= 0 && destPlrNum < MAXPLAYERS &&
        !players[destPlrNum].plr->inGame))
        return;

    writer_s *writer = D_NetWrite();

    // Include the player number if necessary.
    if(pType == GPT_PLAYER_STATE2)
    {
        Writer_WriteByte(writer, srcPlrNum);
    }
    Writer_WriteUInt32(writer, flags);

    if(flags & PSF2_OWNED_WEAPONS)
    {
        // This supports up to 16 weapons.
        int fl = 0;
        for(int i = 0; i < NUM_WEAPON_TYPES; ++i)
        {
            if(pl->weapons[i].owned)
                fl |= 1 << i;
        }
        Writer_WriteUInt16(writer, fl);
    }

    if(flags & PSF2_STATE)
    {
        Writer_WriteByte(writer, pl->playerState |
#if __JDOOM__ || __JHERETIC__ || __JDOOM64__ // Hexen doesn't have armortype.
            (pl->armorType << 4));
#else
            0);
#endif
        Writer_WriteByte(writer, pl->cheats);
    }

    // Finally, send the packet.
    Net_SendPacket(destPlrNum, pType, Writer_Data(writer), Writer_Size(writer));
}

void NetSv_SendPlayerState(int srcPlrNum, int destPlrNum, int flags, dd_bool /*reliable*/)
{
    int pType = (srcPlrNum == destPlrNum ? GPT_CONSOLEPLAYER_STATE : GPT_PLAYER_STATE);
    player_t *pl = &players[srcPlrNum];

    if(!IS_NETWORK_SERVER || !pl->plr->inGame ||
       (destPlrNum >= 0 && destPlrNum < MAXPLAYERS && !players[destPlrNum].plr->inGame))
        return;

    LOGDEV_NET_MSG("NetSv_SendPlayerState: src=%i, dest=%i, flags=%x") << srcPlrNum << destPlrNum << flags;

    writer_s *writer = D_NetWrite();

    // Include the player number if necessary.
    if(pType == GPT_PLAYER_STATE)
    {
        Writer_WriteByte(writer, srcPlrNum);
    }

    // The first bytes contain the flags.
    Writer_WriteUInt16(writer, flags);
    if(flags & PSF_STATE)
    {
        Writer_WriteByte(writer, pl->playerState |
#if __JDOOM__ || __JHERETIC__ || __JDOOM64__ // Hexen doesn't have armortype.
            (pl->armorType << 4));
#else
            0);
#endif
    }

    if(flags & PSF_HEALTH)
    {
        Writer_WriteByte(writer, pl->health);
    }

    if(flags & PSF_ARMOR_POINTS)
    {
#if __JHEXEN__
        // Hexen has many types of armor points, send them all.
        for(int i = 0; i < NUMARMOR; ++i)
        {
            Writer_WriteByte(writer, pl->armorPoints[i]);
        }
#else
        Writer_WriteByte(writer, pl->armorPoints);
#endif
    }

#if __JHERETIC__ || __JHEXEN__
    if(flags & PSF_INVENTORY)
    {
        uint count = 0;
        for(int i = 0; i < NUM_INVENTORYITEM_TYPES; ++i)
        {
            count += P_InventoryCount(srcPlrNum, inventoryitemtype_t(IIT_FIRST + i))? 1 : 0;
        }

        Writer_WriteByte(writer, count);
        if(count)
        {
            for(int i = 0; i < NUM_INVENTORYITEM_TYPES; ++i)
            {
                inventoryitemtype_t type = inventoryitemtype_t(IIT_FIRST + i);
                uint num = P_InventoryCount(srcPlrNum, type);

                if(num)
                {
                    Writer_WriteUInt16(writer, (type & 0xff) | ((num & 0xff) << 8));
                }
            }
        }
    }
#endif

    if(flags & PSF_POWERS)
    {
        byte powers = 0;

        // First see which powers should be sent.
#if __JHEXEN__ || __JSTRIFE__
        for(int i = 1; i < NUM_POWER_TYPES; ++i)
        {
            if(pl->powers[i])
            {
                powers |= 1 << (i - 1);
            }
        }
#else
        for(int i = 0; i < NUM_POWER_TYPES; ++i)
        {
#  if __JDOOM__ || __JDOOM64__
            if(i == PT_IRONFEET || i == PT_STRENGTH)
                continue;
#  endif
            if(pl->powers[i])
            {
                powers |= 1 << i;
            }
        }
#endif
        Writer_WriteByte(writer, powers);

        // Send the non-zero powers.
#if __JHEXEN__ || __JSTRIFE__
        for(int i = 1; i < NUM_POWER_TYPES; ++i)
        {
            if(pl->powers[i])
            {
                Writer_WriteByte(writer, (pl->powers[i] + 34) / 35);
            }
        }
#else
        for(int i = 0; i < NUM_POWER_TYPES; ++i)
        {
#  if __JDOOM__ || __JDOOM64__
            if(i == PT_IRONFEET || i == PT_STRENGTH)
                continue;
#  endif
            if(pl->powers[i])
            {
                Writer_WriteByte(writer, (pl->powers[i] + 34) / 35); // Send as seconds.
            }
        }
#endif
    }

    if(flags & PSF_KEYS)
    {
        byte keys = 0;

#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
        for(int i = 0; i < NUM_KEY_TYPES; ++i)
        {
            if(pl->keys[i])
                keys |= 1 << i;
        }
#endif
#if __JHEXEN__
        keys = pl->keys;
#endif

        Writer_WriteByte(writer, keys);
    }

    if(flags & PSF_FRAGS)
    {
        // How many are there?
        byte count = 0;
        for(int i = 0; i < MAXPLAYERS; ++i)
        {
            if(pl->frags[i] > 0) count++;
        }
        Writer_WriteByte(writer, count);

        // We'll send all non-zero frags. The topmost four bits of
        // the word define the player number.
        for(int i = 0; i < MAXPLAYERS; ++i)
        {
            if(pl->frags[i] > 0)
            {
                Writer_WriteUInt16(writer, (i << 12) | pl->frags[i]);
            }
        }
    }

    if(flags & PSF_OWNED_WEAPONS)
    {
        int ownedBits = 0;
        for(int i = 0; i < NUM_WEAPON_TYPES; ++i)
        {
            if(pl->weapons[i].owned)
                ownedBits |= 1 << i;
        }
        Writer_WriteByte(writer, ownedBits);
    }

    if(flags & PSF_AMMO)
    {
        for(int i = 0; i < NUM_AMMO_TYPES; ++i)
        {
            Writer_WriteInt16(writer, pl->ammo[i].owned);
        }
    }

    if(flags & PSF_MAX_AMMO)
    {
#if __JDOOM__ || __JHERETIC__ || __JDOOM64__ // Hexen has no use for max ammo.
        for(int i = 0; i < NUM_AMMO_TYPES; ++i)
        {
            Writer_WriteInt16(writer, pl->ammo[i].max);
        }
#endif
    }

    if(flags & PSF_COUNTERS)
    {
        Writer_WriteInt16(writer, pl->killCount);
        Writer_WriteByte(writer, pl->itemCount);
        Writer_WriteByte(writer, pl->secretCount);
    }

    if((flags & PSF_PENDING_WEAPON) || (flags & PSF_READY_WEAPON))
    {
        // These two will be in the same byte.
        byte fl = 0;
        if(flags & PSF_PENDING_WEAPON)
            fl |= pl->pendingWeapon & 0xf;
        if(flags & PSF_READY_WEAPON)
            fl |= (pl->readyWeapon & 0xf) << 4;
        Writer_WriteByte(writer, fl);
    }

    if(flags & PSF_VIEW_HEIGHT)
    {
        // @todo Do clients really need to know this?
        Writer_WriteByte(writer, (byte) pl->viewHeight);
    }

#if __JHERETIC__ || __JHEXEN__ || __JSTRIFE__
    if(flags & PSF_MORPH_TIME)
    {
        App_Log(DE2_DEV_NET_MSG,
                "NetSv_SendPlayerState: Player %i, sending morph tics as %i seconds",
                srcPlrNum, (pl->morphTics + 34) / 35);

        // Send as seconds.
        Writer_WriteByte(writer, (pl->morphTics + 34) / 35);
    }
#endif

#if defined(HAVE_EARTHQUAKE) || __JSTRIFE__
    if(flags & PSF_LOCAL_QUAKE)
    {
        // Send the "quaking" state.
        Writer_WriteByte(writer, localQuakeHappening[srcPlrNum]);
    }
#endif

    // Finally, send the packet.
    Net_SendPacket(destPlrNum, pType,
                   Writer_Data(writer), Writer_Size(writer));
}

void NetSv_SendPlayerInfo(int whose, int toWhom)
{
    if(IS_CLIENT) return;

    writer_s *writer = D_NetWrite();
    Writer_WriteByte(writer, whose);
    Writer_WriteByte(writer, cfg.playerColor[whose]);

#if __JHERETIC__ || __JHEXEN__
    Writer_WriteByte(writer, cfg.playerClass[whose]); // current class
#endif
    Net_SendPacket(toWhom, GPT_PLAYER_INFO, Writer_Data(writer), Writer_Size(writer));
}

void NetSv_ChangePlayerInfo(int from, reader_s *msg)
{
    player_t *pl = &players[from];

    // Color is first.
    int col = Reader_ReadByte(msg);
    cfg.playerColor[from] = PLR_COLOR(from, col);

    // Player class.
    playerclass_t newClass = playerclass_t(Reader_ReadByte(msg));
    P_SetPlayerRespawnClass(from, newClass); // requesting class change?

    App_Log(DE2_DEV_NET_NOTE,
            "NetSv_ChangePlayerInfo: pl%i, col=%i, requested class=%i",
            from, cfg.playerColor[from], newClass);

    // The 'colorMap' variable controls the setting of the color
    // translation flags when the player is (re)spawned.
    pl->colorMap = cfg.playerColor[from];

    if(pl->plr->mo)
    {
        // Change the player's mobj's color translation flags.
        pl->plr->mo->flags &= ~MF_TRANSLATION;
        pl->plr->mo->flags |= cfg.playerColor[from] << MF_TRANSSHIFT;
    }

    if(pl->plr->mo)
    {
        App_Log(DE2_DEV_NET_XVERBOSE,
                "Player %i mo %i translation flags %x", from, pl->plr->mo->thinker.id,
                (pl->plr->mo->flags & MF_TRANSLATION) >> MF_TRANSSHIFT);
    }

    // Re-deal start spots.
    P_DealPlayerStarts(0);

    // Tell the other clients about the change.
    NetSv_SendPlayerInfo(from, DDSP_ALL_PLAYERS);
}

void NetSv_FragsForAll(player_t *player)
{
    DENG2_ASSERT(player != 0);
    NetSv_SendPlayerState(player - players, DDSP_ALL_PLAYERS, PSF_FRAGS, true);
}

int NetSv_GetFrags(int pl)
{
    int count = 0;
    for(int i = 0; i < MAXPLAYERS; ++i)
    {
#if __JDOOM__ || __JDOOM64__
        count += players[pl].frags[i] * (i == pl ? -1 : 1);
#else
        count += players[pl].frags[i];
#endif
    }
    return count;
}

void NetSv_KillMessage(player_t *killer, player_t *fragged, dd_bool stomping)
{
#if __JDOOM__ || __JDOOM64__
    if(!cfg.killMessages) return;
    if(!gfw_Rule(deathmatch)) return;

    char buf[500];
    buf[0] = 0;

    char tmp[2];
    tmp[1] = 0;

    // Choose the right kill message template.
    char const *in = GET_TXT(stomping ? TXT_KILLMSG_STOMP : killer ==
                             fragged ? TXT_KILLMSG_SUICIDE : TXT_KILLMSG_WEAPON0 +
                             killer->readyWeapon);

    for(; *in; in++)
    {
        if(in[0] == '%')
        {
            if(in[1] == '1')
            {
                strcat(buf, Net_GetPlayerName(killer - players));
                in++;
                continue;
            }

            if(in[1] == '2')
            {
                strcat(buf, Net_GetPlayerName(fragged - players));
                in++;
                continue;
            }

            if(in[1] == '%')
            {
                in++;
            }
        }
        tmp[0] = *in;
        strcat(buf, tmp);
    }

    // Send the message to everybody.
    NetSv_SendMessage(DDSP_ALL_PLAYERS, buf);
#else
    DENG2_UNUSED3(killer, fragged, stomping);
#endif
}

void NetSv_SendPlayerClass(int plrNum, char cls)
{
    App_Log(DE2_DEV_NET_MSG, "NetSv_SendPlayerClass: Player %i has class %i", plrNum, cls);

    writer_s *writer = D_NetWrite();
    Writer_WriteByte(writer, cls);
    Net_SendPacket(plrNum, GPT_CLASS, Writer_Data(writer), Writer_Size(writer));
}

void NetSv_SendJumpPower(int target, float power)
{
    if(!IS_SERVER) return;

    writer_s *writer = D_NetWrite();
    Writer_WriteFloat(writer, power);
    Net_SendPacket(target, GPT_JUMP_POWER, Writer_Data(writer), Writer_Size(writer));
}

void NetSv_ExecuteCheat(int player, char const *command)
{
    // Killing self is always allowed.
    /// @todo fixme: really? Even in deathmatch?? (should be a game rule)
    if(!qstrnicmp(command, "suicide", 7))
    {
        DD_Executef(false, "suicide %i", player);
    }

    // If cheating is not allowed, we ain't doing nuthin'.
    if(!netSvAllowCheats)
    {
        NetSv_SendMessage(player, "--- CHEATS DISABLED ON THIS SERVER ---");
        return;
    }

    /// @todo Can't we use the multipurpose cheat command here?
    if(!qstrnicmp(command, "god", 3)
       || !qstrnicmp(command, "noclip", 6)
       || !qstrnicmp(command, "give", 4)
       || !qstrnicmp(command, "kill", 4)
#ifdef __JHERETIC__
       || !qstrnicmp(command, "chicken", 7)
#elif __JHEXEN__
       || !qstrnicmp(command, "class", 5)
       || !qstrnicmp(command, "pig", 3)
       || !qstrnicmp(command, "runscript", 9)
#endif
       )
    {
        DD_Executef(false, "%s %i", command, player);
    }
}

void NetSv_DoCheat(int player, reader_s *msg)
{
    size_t len = Reader_ReadUInt16(msg);
    char *command = (char *)Z_Calloc(len + 1, PU_GAMESTATIC, 0);

    Reader_Read(msg, command, len);
    NetSv_ExecuteCheat(player, command);
    Z_Free(command);
}

/**
 * Calls @a callback on @a thing while it is temporarily placed at the
 * specified position and angle. Afterwards the thing's old position is restored.
 */
void NetSv_TemporaryPlacedCallback(mobj_t *thing, void *param, coord_t tempOrigin[3],
    angle_t angle, void (*callback)(mobj_t *, void *))
{
    coord_t const oldOrigin[3] = { thing->origin[VX], thing->origin[VY], thing->origin[VZ] };
    coord_t const oldFloorZ    = thing->floorZ;
    coord_t const oldCeilingZ  = thing->ceilingZ;
    angle_t const oldAngle     = thing->angle;

    // We will temporarily move the object to the temp coords.
    if(P_CheckPosition(thing, tempOrigin))
    {
        P_MobjUnlink(thing);
        thing->origin[VX] = tempOrigin[VX];
        thing->origin[VY] = tempOrigin[VY];
        thing->origin[VZ] = tempOrigin[VZ];
        P_MobjLink(thing);
        thing->floorZ = tmFloorZ;
        thing->ceilingZ = tmCeilingZ;
    }
    thing->angle = angle;

    callback(thing, param);

    // Restore the old position.
    P_MobjUnlink(thing);
    thing->origin[VX] = oldOrigin[VX];
    thing->origin[VY] = oldOrigin[VY];
    thing->origin[VZ] = oldOrigin[VZ];
    P_MobjLink(thing);
    thing->floorZ = oldFloorZ;
    thing->ceilingZ = oldCeilingZ;
    thing->angle = oldAngle;
}

static void NetSv_UseActionCallback(mobj_t * /*mo*/, void *context)
{
    P_UseLines(static_cast<player_t *>(context));
}

static void NetSv_FireWeaponCallback(mobj_t * /*mo*/, void *context)
{
    P_FireWeapon(static_cast<player_t *>(context));
}

static void NetSv_HitFloorCallback(mobj_t *mo, void * /*context*/)
{
    App_Log(DE2_DEV_MAP_XVERBOSE, "NetSv_HitFloorCallback: mo %i", mo->thinker.id);

    P_HitFloor(mo);
}

void NetSv_DoFloorHit(int player, reader_s *msg)
{
    player_t *plr = &players[player];

    if(player < 0 || player >= MAXPLAYERS)
        return;

    mobj_t *mo = plr->plr->mo;
    if(!mo) return;

    coord_t pos[3];
    pos[VX] = Reader_ReadFloat(msg);
    pos[VY] = Reader_ReadFloat(msg);
    pos[VZ] = Reader_ReadFloat(msg);

    // The momentum is included, although we don't really need it.
    /*mom[MX] =*/ Reader_ReadFloat(msg);
    /*mom[MY] =*/ Reader_ReadFloat(msg);
    /*mom[MZ] =*/ Reader_ReadFloat(msg);

    NetSv_TemporaryPlacedCallback(mo, 0, pos, mo->angle, NetSv_HitFloorCallback);
}

void NetSv_DoAction(int player, reader_s *msg)
{
    player_t *pl = &players[player];

    int type        = Reader_ReadInt32(msg);
    coord_t pos[3];
    pos[VX]         = Reader_ReadFloat(msg);
    pos[VY]         = Reader_ReadFloat(msg);
    pos[VZ]         = Reader_ReadFloat(msg);
    angle_t angle   = Reader_ReadUInt32(msg);
    float lookDir   = Reader_ReadFloat(msg);
    int actionParam = Reader_ReadInt32(msg);

    App_Log(DE2_DEV_MAP_VERBOSE,
            "NetSv_DoAction: player=%i, action=%i, xyz=(%.1f,%.1f,%.1f)\n  "
            "angle=%x lookDir=%g param=%i",
            player, type, pos[VX], pos[VY], pos[VZ],
            angle, lookDir, actionParam);

    if(G_GameState() != GS_MAP)
    {
        if(G_GameState() == GS_INTERMISSION)
        {
            if(type == GPA_USE || type == GPA_FIRE)
            {
                App_Log(DE2_NET_MSG, "Intermission skip requested");
                IN_SkipToNext();
            }
        }
        return;
    }

    if(pl->playerState == PST_DEAD)
    {
        // This player is dead. Rise, my friend!
        P_PlayerReborn(pl);
        return;
    }

    switch(type)
    {
    case GPA_USE:
    case GPA_FIRE:
        if(pl->plr->mo)
        {
            // Update lookdir to match client's direction at the time of the action.
            pl->plr->lookDir = lookDir;

            if(type == GPA_FIRE)
            {
                pl->refire = actionParam;
            }

            NetSv_TemporaryPlacedCallback(pl->plr->mo, pl, pos, angle,
                                          type == GPA_USE? NetSv_UseActionCallback :
                                                           NetSv_FireWeaponCallback);
        }
        break;

    case GPA_CHANGE_WEAPON:
        pl->brain.changeWeapon = actionParam;
        break;

    case GPA_USE_FROM_INVENTORY:
#if __JHERETIC__ || __JHEXEN__ || __JDOOM64__
        P_InventoryUse(player, inventoryitemtype_t(actionParam), true);
#endif
        break;
    }
}

void NetSv_DoDamage(int player, reader_s *msg)
{
    int damage       = Reader_ReadInt32(msg);
    thid_t target    = Reader_ReadUInt16(msg);
    thid_t inflictor = Reader_ReadUInt16(msg);
    thid_t source    = Reader_ReadUInt16(msg);

    App_Log(DE2_DEV_MAP_XVERBOSE,
            "NetSv_DoDamage: Client %i requests damage %i on %i via %i by %i",
            player, damage, target, inflictor, source);

    P_DamageMobj2(Mobj_ById(target), Mobj_ById(inflictor), Mobj_ById(source), damage,
                  false /*not stomping*/, true /*just do it*/);
}

void NetSv_SaveGame(uint sessionId)
{
#if !__JHEXEN__

    if(!IS_SERVER || !IS_NETGAME)
        return;

    // This will make the clients save their games.
    writer_s *writer = D_NetWrite();
    Writer_WriteUInt32(writer, sessionId);
    Net_SendPacket(DDSP_ALL_PLAYERS, GPT_SAVE, Writer_Data(writer), Writer_Size(writer));

#else
    DENG2_UNUSED(sessionId);
#endif
}

void NetSv_LoadGame(uint sessionId)
{
#if !__JHEXEN__

    if(!IS_SERVER || !IS_NETGAME)
        return;

    writer_s *writer = D_NetWrite();
    Writer_WriteUInt32(writer, sessionId);
    Net_SendPacket(DDSP_ALL_PLAYERS, GPT_LOAD, Writer_Data(writer), Writer_Size(writer));

#else
    DENG2_UNUSED(sessionId);
#endif
}

void NetSv_SendMessageEx(int plrNum, char const *msg, dd_bool yellow)
{
    if(IS_CLIENT || !netSvAllowSendMsg)
        return;

    if(plrNum >= 0 && plrNum < MAXPLAYERS)
    {
        if(!players[plrNum].plr->inGame)
            return;
    }

    App_Log(DE2_DEV_NET_VERBOSE, "NetSv_SendMessageEx: '%s'", msg);

    if((unsigned)plrNum == DDSP_ALL_PLAYERS)
    {
        // Also show locally. No sound is played!
        D_NetMessageNoSound(CONSOLEPLAYER, msg);
    }

    writer_s *writer = D_NetWrite();
    Writer_WriteUInt16(writer, uint16_t(strlen(msg)));
    Writer_Write(writer, msg, strlen(msg));
    Net_SendPacket(plrNum,
                   yellow ? GPT_YELLOW_MESSAGE : GPT_MESSAGE,
                   Writer_Data(writer), Writer_Size(writer));
}

void NetSv_SendMessage(int plrNum, char const *msg)
{
    NetSv_SendMessageEx(plrNum, msg, false);
}

void NetSv_SendYellowMessage(int plrNum, char const *msg)
{
    NetSv_SendMessageEx(plrNum, msg, true);
}

void NetSv_MaybeChangeWeapon(int plrNum, int weapon, int ammo, int force)
{
    if(IS_CLIENT) return;

    if(plrNum < 0 || plrNum >= MAXPLAYERS)
        return;

    App_Log(DE2_DEV_NET_VERBOSE,
            "NetSv_MaybeChangeWeapon: Plr=%i Weapon=%i Ammo=%i Force=%i",
            plrNum, weapon, ammo, force);

    writer_s *writer = D_NetWrite();
    Writer_WriteInt16(writer, weapon);
    Writer_WriteInt16(writer, ammo);
    Writer_WriteByte(writer, force != 0);
    Net_SendPacket(plrNum, GPT_MAYBE_CHANGE_WEAPON, Writer_Data(writer), Writer_Size(writer));
}

void NetSv_SendLocalMobjState(mobj_t *mobj, char const *stateName)
{
    DENG2_ASSERT(mobj != 0);

    ddstring_t name;
    Str_InitStatic(&name, stateName);

    // Inform the client about this.
    writer_s *msg = D_NetWrite();
    Writer_WriteUInt16(msg, mobj->thinker.id);
    Writer_WriteUInt16(msg, mobj->target? mobj->target->thinker.id : 0); // target id
    Str_Write(&name, msg); // state to switch to
#if !defined(__JDOOM__) && !defined(__JDOOM64__)
    Writer_WriteInt32(msg, mobj->special1);
#else
    Writer_WriteInt32(msg, 0);
#endif

    Net_SendPacket(DDSP_ALL_PLAYERS, GPT_LOCAL_MOBJ_STATE, Writer_Data(msg), Writer_Size(msg));
}

/**
 * Handles the console commands "startcycle" and "endcycle".
 */
D_CMD(MapCycle)
{
    DENG2_UNUSED2(src, argc);

    if(!IS_SERVER)
    {
        App_Log(DE2_SCR_ERROR, "Only allowed for a server");
        return false;
    }

    if(!qstricmp(argv[0], "startcycle")) // (Re)start rotation?
    {
        // Find the first map in the sequence.
        de::Uri mapUri = NetSv_ScanCycle(cycleIndex = 0);
        if(mapUri.path().isEmpty())
        {
            App_Log(DE2_SCR_ERROR, "MapCycle \"%s\" is invalid.", mapCycle);
            return false;
        }
        for(int i = 0; i < MAXPLAYERS; ++i)
        {
            cycleRulesCounter[i] = 0;
        }
        // Warp there.
        NetSv_CycleToMapNum(mapUri);
        cyclingMaps = true;
    }
    else
    {
        // OK, then we need to end it.
        if(cyclingMaps)
        {
            cyclingMaps = false;
            NetSv_SendMessage(DDSP_ALL_PLAYERS, "MAP ROTATION ENDS");
        }
    }

    return true;
}
