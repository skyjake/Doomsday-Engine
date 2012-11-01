/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2012 Daniel Swanson <danij@dengine.net>
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
 * d_netsv.c : Common code related to net games (server-side).
 */

// HEADER FILES ------------------------------------------------------------

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "common.h"
#include "d_net.h"
#include "p_player.h"
#include "p_user.h"
#include "p_map.h"
#include "g_common.h"
#include "p_tick.h"
#include "p_start.h"
#include "p_inventory.h"

// MACROS ------------------------------------------------------------------

#if __JHEXEN__ || __JSTRIFE__
#  define SOUND_COUNTDOWN       SFX_PICKUP_KEY
#elif __JDOOM__ || __JDOOM64__
#  define SOUND_COUNTDOWN       SFX_GETPOW
#elif __JHERETIC__
#  define SOUND_COUNTDOWN       SFX_KEYUP
#endif

#define SOUND_VICTORY           SOUND_COUNTDOWN

//#define UPD_BUFFER_LEN          500

// How long is the largest possible sector update?
//#define MAX_SECTORUPD           20
//#define MAX_SIDEUPD             9

/*
#define WRITE_SHORT(byteptr, val)   {(*(short*)(byteptr) = SHORT(val)); byteptr += 2;}
#define WRITE_LONG(byteptr, val)    {(*(int*)(byteptr) = LONG(val)); byteptr += 4;}
#define WRITE_FLOAT(byteptr, val)   {(*(int*)(byteptr) = LONG(*(int*)&val)); byteptr += 4;}
*/

// TYPES -------------------------------------------------------------------

typedef struct maprule_s {
    boolean     usetime, usefrags;
    int         time;           // Minutes.
    int         frags;          // Maximum frags for one player.
} maprule_t;

typedef enum cyclemode_s {
    CYCLE_IDLE,
    CYCLE_COUNTDOWN
} cyclemode_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

void    R_SetAllDoomsdayFlags();
void    P_FireWeapon(player_t *player);

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

int     NetSv_GetFrags(int pl);
void    NetSv_MapCycleTicker(void);
void    NetSv_SendPlayerClass(int pnum, char cls);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

char    cyclingMaps = false;
char   *mapCycle = "";
char    mapCycleNoExit = true;
int     netSvAllowSendMsg = true;
int     netSvAllowCheats = false;

// This is returned in *_Get(DD_GAME_CONFIG). It contains a combination
// of space-separated keywords.
char    gameConfigString[128];

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static int cycleIndex;
static int cycleCounter = -1, cycleMode = CYCLE_IDLE;
static int cycleRulesCounter[MAXPLAYERS];

#if __JHERETIC__ || __JHEXEN__ || __JSTRIFE__
static int oldClasses[MAXPLAYERS];
#endif

// CODE --------------------------------------------------------------------

/**
 * Update the game config string with keywords that describe the game.
 * The string is sent out in netgames (also to the master).
 * Keywords: dm, coop, jump, nomonst, respawn, skillN
 */
void NetSv_UpdateGameConfig(void)
{
    if(IS_CLIENT)
        return;

    memset(gameConfigString, 0, sizeof(gameConfigString));

    sprintf(gameConfigString, "skill%i", gameSkill + 1);

    if(deathmatch > 1)
        sprintf(gameConfigString, " dm%i", deathmatch);
    else if(deathmatch)
        strcat(gameConfigString, " dm");
    else
        strcat(gameConfigString, " coop");

    if(noMonstersParm)
        strcat(gameConfigString, " nomonst");
#if !__JHEXEN__
    if(respawnMonsters)
        strcat(gameConfigString, " respawn");
#endif

    if(cfg.jumpEnabled)
        strcat(gameConfigString, " jump");
}

/**
 * Sharp ticker, i.e., called at 35 Hz.
 */
void NetSv_Ticker(void)
{
    float power;
    int i;

    // Map rotation checker.
    NetSv_MapCycleTicker();

    // This is done here for servers.
    R_SetAllDoomsdayFlags();

    // Set the camera filters for players.
    for(i = 0; i < MAXPLAYERS; ++i)
    {
        R_UpdateViewFilter(i);
    }

    // Inform clients about jumping?
    power = (cfg.jumpEnabled ? cfg.jumpPower : 0);
    if(power != netJumpPower)
    {
        netJumpPower = power;
        for(i = 0; i < MAXPLAYERS; ++i)
        {
            if(players[i].plr->inGame)
                NetSv_SendJumpPower(i, power);
        }
    }

    // Send the player state updates.
    for(i = 0; i < MAXPLAYERS; ++i)
    {
        player_t* plr = &players[i];

        /*
        // Don't send on every tic. Also, don't send to all
        // players at the same time.
        if(((int) GAMETIC + i) % 10)
            continue;
        */

        if(!plr->plr->inGame)
            continue;

        if(plr->update)
        {
            // Owned weapons and player state will be sent in a new kind of
            // packet.
            if(plr->update & (PSF_OWNED_WEAPONS | PSF_STATE))
            {
                int flags =
                    (plr->update & PSF_OWNED_WEAPONS ? PSF2_OWNED_WEAPONS : 0) |
                    (plr->update & PSF_STATE ? PSF2_STATE : 0);

                NetSv_SendPlayerState2(i, i, flags, true);

                plr->update &= ~(PSF_OWNED_WEAPONS | PSF_STATE);
                // That was all?
                if(!plr->update)
                    continue;
            }

            // The delivery of the state packet will be confirmed.
            NetSv_SendPlayerState(i, i, plr->update, true);
            plr->update = 0;
        }

#if __JHERETIC__ || __JHEXEN__ || __JSTRIFE__
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

void NetSv_CycleToMapNum(uint map)
{
    char tmp[3], cmd[80];
    int i;

    sprintf(tmp, "%02u", map);
#if __JDOOM64__
    sprintf(cmd, "setmap 1 %u", map);
#elif __JDOOM__
    if(gameModeBits & GM_ANY_DOOM2)
        sprintf(cmd, "setmap 1 %u", map);
    else
        sprintf(cmd, "setmap %c %c", tmp[0], tmp[1]);
#elif __JHERETIC__
    sprintf(cmd, "setmap %c %c", tmp[0], tmp[1]);
#elif __JHEXEN__ || __JSTRIFE__
    sprintf(cmd, "setmap %u", map);
#endif

    DD_Execute(false, cmd);

    // In a couple of seconds, send everyone the rules of this map.
    for(i = 0; i < MAXPLAYERS; ++i)
        cycleRulesCounter[i] = 3 * TICSPERSEC;

    cycleMode = CYCLE_IDLE;
    cycleCounter = 0;
}

/**
 * Reads through the MapCycle cvar and finds the map with the given index.
 * Rules that apply to the map are returned in 'rules'.
 */
int NetSv_ScanCycle(int index, maprule_t* rules)
{
    char       *ptr = mapCycle, *end;
    int         i, pos = -1;
    uint        episode, map;

#if __JHEXEN__ || __JSTRIFE__
    int         m;
#endif
    char        tmp[3], lump[10];
    boolean     clear = false, has_random = false;
    maprule_t   dummy;

    if(!rules)
        rules = &dummy;

    // By default no rules apply.
    rules->usetime = rules->usefrags = false;

    for(; *ptr; ptr++)
    {
        if(isspace(*ptr))
            continue;

        if(*ptr == ',' || *ptr == '+' || *ptr == ';' || *ptr == '/' ||
           *ptr == '\\')
        {
            // These symbols are allowed to combine "time" and "frags".
            // E.g. "Time:10/Frags:5" or "t:30, f:10"
            clear = false;
        }
        else if(!strnicmp("time", ptr, 1))
        {
            // Find the colon.
            while(*ptr && *ptr != ':')
                ptr++;

            if(!*ptr)
                return -1;

            if(clear)
                rules->usefrags = false;
            clear = true;

            rules->usetime = true;
            rules->time = strtol(ptr + 1, &end, 0);
            ptr = end - 1;
        }
        else if(!strnicmp("frags", ptr, 1))
        {
            // Find the colon.
            while(*ptr && *ptr != ':')
                ptr++;

            if(!*ptr)
                return -1;

            if(clear)
                rules->usetime = false;
            clear = true;

            rules->usefrags = true;
            rules->frags = strtol(ptr + 1, &end, 0);
            ptr = end - 1;
        }
        else if(*ptr == '*' || (*ptr >= '0' && *ptr <= '9'))
        {
            // A map identifier is here.
            pos++;

            // Read it.
            tmp[0] = *ptr++;
            tmp[1] = *ptr;
            tmp[2] = 0;

            if(strlen(tmp) < 2)
            {
                // Assume a zero is missing.
                tmp[1] = tmp[0];
                tmp[0] = '0';
            }

            if(index == pos)
            {
                if(tmp[0] == '*' || tmp[1] == '*')
                    has_random = true;

                // This is the map we're looking for. Return it.
                // But first randomize the asterisks.
                for(i = 0; i < 100; i++) // Try many times to find a good map.
                {
                    // The differences in map numbering make this harder
                    // than it should be.
#if __JDOOM64__
                    sprintf(lump, "MAP%u%u", episode =
                            tmp[0] == '*' ? RNG_RandByte() % 4 : tmp[0] - '0',
                            map = tmp[1] == '*' ? RNG_RandByte() % 10 : tmp[1] - '0');
#elif __JDOOM__
                    if(gameModeBits & GM_ANY_DOOM2)
                    {
                        sprintf(lump, "MAP%u%u", episode =
                                tmp[0] == '*' ? RNG_RandByte() % 4 : tmp[0] - '0',
                                map = tmp[1] == '*' ? RNG_RandByte() % 10 : tmp[1] - '0');
                    }
                    else
                    {
                        sprintf(lump, "E%uM%u", episode =
                                tmp[0] == '*' ? 1 + RNG_RandByte() % 4 : tmp[0] - '0',
                                map = tmp[1] == '*' ? 1 + RNG_RandByte() % 9 : tmp[1] - '0');
                    }
#elif __JSTRIFE__
                    sprintf(lump, "MAP%u%u", episode =
                            tmp[0] == '*' ? RNG_RandByte() % 4 : tmp[0] - '0',
                            map =
                            tmp[1] ==
                            '*' ? RNG_RandByte() % 10 : tmp[1] - '0');
#elif __JHERETIC__
                    sprintf(lump, "E%uM%u", episode =
                            tmp[0] == '*' ? 1 + RNG_RandByte() % 6 : tmp[0] - '0',
                            map =
                            tmp[1] == '*' ? 1 + RNG_RandByte() % 9 : tmp[1] - '0');
#elif __JHEXEN__
                    sprintf(lump, "%u%u", episode =
                            tmp[0] == '*' ? RNG_RandByte() % 4 : tmp[0] - '0',
                            map =
                            tmp[1] == '*' ? RNG_RandByte() % 10 : tmp[1] - '0');
                    m = P_TranslateMap(atoi(lump));
                    if(m < 0)
                        continue;
                    sprintf(lump, "MAP%02u", m);
#endif
                    if(W_CheckLumpNumForName(lump) >= 0)
                    {
                        tmp[0] = episode + '0';
                        tmp[1] = map + '0';
                        break;
                    }
                    else if(!has_random)
                    {
                        return -1;
                    }
                }

                // Convert to a number.
                return atoi(tmp);
            }
        }
    }

    // Didn't find it.
    return -1;
}

void NetSv_TellCycleRulesToPlayerAfterTics(int destPlr, int tics)
{
    if(destPlr >= 0 && destPlr < MAXPLAYERS)
    {
        cycleRulesCounter[destPlr] = tics;
    }
    else if(destPlr == DDSP_ALL_PLAYERS)
    {
        int i;
        for(i = 0; i < MAXPLAYERS; ++i)
            cycleRulesCounter[i] = tics;
    }
}

/**
 * Sends a message about the map cycle rules to a specific player.
 */
void NetSv_TellCycleRulesToPlayer(int destPlr)
{
    maprule_t rules;
    char msg[100];
    char tmp[100];

    if(!cyclingMaps) return;

#ifdef _DEBUG
    Con_Message("NetSv_TellCycleRulesToPlayer: %i\n", destPlr);
#endif

    // Get the rules of the current map.
    NetSv_ScanCycle(cycleIndex, &rules);
    strcpy(msg, "MAP RULES: ");
    if(!rules.usetime && !rules.usefrags)
    {
        strcat(msg, "NONE");
    }
    else
    {
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

void NetSv_MapCycleTicker(void)
{
    int         map, i, f;
    maprule_t   rules;
    char        msg[100];

    if(!cyclingMaps) return;

    // Check rules broadcasting.
    for(i = 0; i < MAXPLAYERS; ++i)
    {
        if(!cycleRulesCounter[i] || !players[i].plr->inGame) continue;
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
            // Test every ten seconds.
            cycleCounter = 10 * TICSPERSEC;

            map = NetSv_ScanCycle(cycleIndex, &rules);
            if(map < 0)
            {
                map = NetSv_ScanCycle(cycleIndex = 0, &rules);
                if(map < 0)
                {
                    // Hmm?! Abort cycling.
                    Con_Message("NetSv_CheckCycling: All of a sudden MapCycle is invalid!\n");
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
                for(i = 0; i < MAXPLAYERS; i++)
                {
                    if(!players[i].plr->inGame)
                        continue;
                    if((f = NetSv_GetFrags(i)) >= rules.frags)
                    {
                        sprintf(msg, "--- %s REACHES %i FRAGS ---", Net_GetPlayerName(i), f);
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
            sprintf(msg, "--- WARPING IN %i SECONDS ---",
                    cycleCounter / TICSPERSEC);

            NetSv_SendMessage(DDSP_ALL_PLAYERS, msg);
            // Also, a warning sound.
            S_StartSound(SOUND_COUNTDOWN, NULL);
        }
        else if(cycleCounter <= 0)
        {
            // Next map, please!
            map = NetSv_ScanCycle(++cycleIndex, NULL);
            if(map < 0)
            {
                // Must be past the end?
                map = NetSv_ScanCycle(cycleIndex = 0, NULL);
                if(map < 0)
                {
                    // Hmm?! Abort cycling.
                    Con_Message("NetSv_CheckCycling: All of a sudden MapCycle is invalid!\n");
                    DD_Execute(false, "endcycle");
                    return;
                }
            }

            // Warp to the next map. Don't bother with the intermission.
            NetSv_CycleToMapNum(map);
        }
        break;
    }
}

/**
 * Resets a player's frag count and other players' frag counts toward the player.
 *
 * @param plrNum  Player to reset.
 */
void NetSv_ResetPlayerFrags(int plrNum)
{
    int i;
    player_t* plr = &players[plrNum];

#ifdef _DEBUG
    Con_Message("NetSv_ResetPlayerFrags: Player %i.\n", plrNum);
#endif
    memset(plr->frags, 0, sizeof(plr->frags));

    // The frag count is dependent on the others' frags.
    for(i = 0; i < MAXPLAYERS; ++i)
    {
        players[i].frags[plrNum] = 0;

        // Everybody will get their frags updated.
        players[i].update |= PSF_FRAGS;
    }
}

/**
 * Server calls this when new players enter the game.
 */
void NetSv_NewPlayerEnters(int plrNum)
{
    player_t* plr = &players[plrNum];

    Con_Message("NetSv_NewPlayerEnters: spawning player %i.\n", plrNum);

    plr->playerState = PST_REBORN;  // Force an init.

    // Re-deal player starts.
    P_DealPlayerStarts(0);

    // Reset the player's frags.
    NetSv_ResetPlayerFrags(plrNum);

    // Spawn the player into the world.
    if(deathmatch)
    {
        G_DeathMatchSpawnPlayer(plrNum);
    }
    else
    {
        playerclass_t pClass = P_ClassForPlayerWhenRespawning(plrNum, false);
        const playerstart_t* start;

        if((start = P_GetPlayerStart(gameMapEntryPoint, plrNum, false)))
        {
            const mapspot_t* spot = &mapSpots[start->spot];
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
}

void NetSv_Intermission(int flags, int state, int time)
{
    Writer* msg;

    if(IS_CLIENT) return;

    msg = D_NetWrite();
    Writer_WriteByte(msg, flags);

#if __JDOOM__ || __JDOOM64__
    if(flags & IMF_BEGIN)
    {
        // Only include the necessary information.
        Writer_WriteUInt16(msg, wmInfo.maxKills);
        Writer_WriteUInt16(msg, wmInfo.maxItems);
        Writer_WriteUInt16(msg, wmInfo.maxSecret);
        Writer_WriteByte(msg, wmInfo.nextMap);
        Writer_WriteByte(msg, wmInfo.currentMap);
        Writer_WriteByte(msg, wmInfo.didSecret);
    }
#endif

#if __JHEXEN__ || __JSTRIFE__
    if(flags & IMF_BEGIN)
    {
        Writer_WriteByte(msg, state); // LeaveMap
        Writer_WriteByte(msg, time); // LeavePosition
    }
#endif

    if(flags & IMF_STATE)
        Writer_WriteInt16(msg, state);

    if(flags & IMF_TIME)
        Writer_WriteInt16(msg, time);

    Net_SendPacket(DDSP_ALL_PLAYERS, GPT_INTERMISSION, Writer_Data(msg), Writer_Size(msg));
}

#if 0
/**
 * The actual script is sent to the clients. 'script' can be NULL.
 */
void NetSv_Finale(int flags, const char* script, const boolean* conds, byte numConds)
{
    size_t scriptLen = 0;
    Writer* writer;

    if(IS_CLIENT)
        return;

    writer = D_NetWrite();

    // How much memory do we need?
    if(script)
    {
        flags |= FINF_SCRIPT;
        scriptLen = strlen(script);
    }

    // First the flags.
    Writer_WriteByte(writer, flags);

    if(script)
    {
        int i;

        // The conditions.
        Writer_WriteByte(writer, numConds);
        for(i = 0; i < numConds; ++i)
        {
            Writer_WriteByte(writer, conds[i]);
        }

        // Then the script itself.
        Writer_WriteUInt32(writer, scriptLen);
        Writer_Write(writer, script, scriptLen);
    }

    Net_SendPacket(DDSP_ALL_PLAYERS | DDSP_ORDERED, GPT_FINALE2, Writer_Data(writer), Writer_Size(writer));
}
#endif

void NetSv_SendGameState(int flags, int to)
{
    int i;
    Writer* writer;
    GameInfo gameInfo;
    Uri* mapUri;
    ddstring_t const* str;

    if(IS_CLIENT)
        return;

    DD_GameInfo(&gameInfo);
    mapUri = G_ComposeMapUri(gameEpisode, gameMap);

    // Print a short message that describes the game state.
    str = Uri_ResolvedConst(mapUri);
    Con_Message("NetSv_SendGameState: Game setup: %s %s %s\n",
                gameInfo.identityKey, Str_Text(str), gameConfigString);

    // Send an update to all the players in the game.
    for(i = 0; i < MAXPLAYERS; ++i)
    {
        if(!players[i].plr->inGame || (to != DDSP_ALL_PLAYERS && to != i))
            continue;

        writer = D_NetWrite();
        Writer_WriteByte(writer, flags);

        // Game identity key.
        Writer_WriteByte(writer, strlen(gameInfo.identityKey));
        Writer_Write(writer, gameInfo.identityKey, strlen(gameInfo.identityKey));

        // The current map.
        Uri_Write(mapUri, writer);

        // Also include the episode and map numbers.
        Writer_WriteByte(writer, gameEpisode);
        Writer_WriteByte(writer, gameMap);

        Writer_WriteByte(writer, (deathmatch & 0x3)
            | (!noMonstersParm? 0x4 : 0)
#if !__JHEXEN__
            | (respawnMonsters? 0x8 : 0)
#else
            | 0
#endif
            | (cfg.jumpEnabled? 0x10 : 0));

        Writer_WriteByte(writer, gameSkill & 0x7);
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

    Uri_Delete(mapUri);
}

/**
 * Informs a player of an impulse momentum that needs to be applied to the player's mobj.
 */
void NetSv_PlayerMobjImpulse(mobj_t* mobj, float mx, float my, float mz)
{
    int plrNum = 0;
    Writer* writer;

    if(!IS_SERVER || !mobj || !mobj->player) return;

    // Which player?
    plrNum = mobj->player - players;

    writer = D_NetWrite();
    Writer_WriteUInt16(writer, mobj->thinker.id);
    Writer_WriteFloat(writer, mx);
    Writer_WriteFloat(writer, my);
    Writer_WriteFloat(writer, mz);

    Net_SendPacket(plrNum, GPT_MOBJ_IMPULSE, Writer_Data(writer), Writer_Size(writer));
}

/**
 * Sends the initial player position to a client. This is the position defined
 * by the map's start spots. It is sent immediately after the server determines
 * where a player is to spawn.
 */
void NetSv_SendPlayerSpawnPosition(int plrNum, float x, float y, float z, int angle)
{
    Writer* writer;

    if(!IS_SERVER) return;

#ifdef _DEBUG
    Con_Message("NetSv_SendPlayerSpawnPosition: Player #%i pos:[%g, %g, %g] angle:%x\n",
        plrNum, x, y, z, angle);
#endif
    writer = D_NetWrite();
    Writer_WriteFloat(writer, x);
    Writer_WriteFloat(writer, y);
    Writer_WriteFloat(writer, z);
    Writer_WriteUInt32(writer, angle);

    Net_SendPacket(plrNum, GPT_PLAYER_SPAWN_POSITION,
        Writer_Data(writer), Writer_Size(writer));
}

/**
 * More player state information. Had to be separate because of backwards
 * compatibility.
 */
void NetSv_SendPlayerState2(int srcPlrNum, int destPlrNum, int flags, boolean reliable)
{
    int         pType = (srcPlrNum == destPlrNum ? GPT_CONSOLEPLAYER_STATE2 : GPT_PLAYER_STATE2);
    player_t   *pl = &players[srcPlrNum];
    int         i, fl;
    Writer*     writer;

    // Check that this is a valid call.
    if(IS_CLIENT || !pl->plr->inGame ||
       (destPlrNum >= 0 && destPlrNum < MAXPLAYERS &&
        !players[destPlrNum].plr->inGame))
        return;

    writer = D_NetWrite();

    // Include the player number if necessary.
    if(pType == GPT_PLAYER_STATE2)
    {
        Writer_WriteByte(writer, srcPlrNum);
    }
    Writer_WriteUInt32(writer, flags);

    if(flags & PSF2_OWNED_WEAPONS)
    {
        // This supports up to 16 weapons.
        for(fl = 0, i = 0; i < NUM_WEAPON_TYPES; ++i)
            if(pl->weapons[i].owned)
                fl |= 1 << i;
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
    Net_SendPacket(destPlrNum, pType,
                   Writer_Data(writer), Writer_Size(writer));
}

void NetSv_SendPlayerState(int srcPlrNum, int destPlrNum, int flags, boolean reliable)
{
    int         pType = (srcPlrNum == destPlrNum ? GPT_CONSOLEPLAYER_STATE : GPT_PLAYER_STATE);
    player_t   *pl = &players[srcPlrNum];
    byte        fl;
    int         i, k;
    Writer*     writer;

    if(IS_CLIENT || !pl->plr->inGame ||
       (destPlrNum >= 0 && destPlrNum < MAXPLAYERS && !players[destPlrNum].plr->inGame))
        return;

#ifdef _DEBUG
    Con_Message("NetSv_SendPlayerState: src=%i, dest=%i, flags=%x\n", srcPlrNum, destPlrNum, flags);
#endif

    writer = D_NetWrite();

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
        Writer_WriteByte(writer, pl->health);

    if(flags & PSF_ARMOR_POINTS)
    {
#if __JHEXEN__
        // Hexen has many types of armor points, send them all.
        for(i = 0; i < NUMARMOR; ++i)
            Writer_WriteByte(writer, pl->armorPoints[i]);
#else
        Writer_WriteByte(writer, pl->armorPoints);
#endif
    }

#if __JHERETIC__ || __JHEXEN__
    if(flags & PSF_INVENTORY)
    {
        uint i, count = 0;

        for(i = 0; i < NUM_INVENTORYITEM_TYPES; ++i)
            count += (P_InventoryCount(srcPlrNum, IIT_FIRST + i)? 1 : 0);

        Writer_WriteByte(writer, count);
        if(count)
        {
            for(i = 0; i < NUM_INVENTORYITEM_TYPES; ++i)
            {
                inventoryitemtype_t type = IIT_FIRST + i;
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
        for(i = 1; i < NUM_POWER_TYPES; ++i)
            if(pl->powers[i])
                powers |= 1 << (i - 1);
#else
        for(i = 0; i < NUM_POWER_TYPES; ++i)
        {
#  if __JDOOM__ || __JDOOM64__
            if(i == PT_IRONFEET || i == PT_STRENGTH)
                continue;
#  endif
            if(pl->powers[i])
                powers |= 1 << i;
        }
#endif
        Writer_WriteByte(writer, powers);

        // Send the non-zero powers.
#if __JHEXEN__ || __JSTRIFE__
        for(i = 1; i < NUM_POWER_TYPES; ++i)
            if(pl->powers[i])
                Writer_WriteByte(writer, (pl->powers[i] + 34) / 35);
#else
        for(i = 0; i < NUM_POWER_TYPES; ++i)
        {
#  if __JDOOM__ || __JDOOM64__
            if(i == PT_IRONFEET || i == PT_STRENGTH)
                continue;
#  endif
            if(pl->powers[i])
                Writer_WriteByte(writer, (pl->powers[i] + 34) / 35); // Send as seconds.
        }
#endif
    }

    if(flags & PSF_KEYS)
    {
        byte keys = 0;

#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
        for(i = 0; i < NUM_KEY_TYPES; ++i)
            if(pl->keys[i])
                keys |= 1 << i;
#endif

        Writer_WriteByte(writer, keys);
    }

    if(flags & PSF_FRAGS)
    {
        byte count = 0;

        // How many are there?
        for(i = 0; i < MAXPLAYERS; ++i) if(pl->frags[i] > 0) count++;
        Writer_WriteByte(writer, count);

        // We'll send all non-zero frags. The topmost four bits of
        // the word define the player number.
        for(i = 0; i < MAXPLAYERS; ++i)
            if(pl->frags[i] > 0)
                Writer_WriteUInt16(writer, (i << 12) | pl->frags[i]);
    }

    if(flags & PSF_OWNED_WEAPONS)
    {
        for(k = 0, i = 0; i < NUM_WEAPON_TYPES; ++i)
            if(pl->weapons[i].owned)
                k |= 1 << i;
        Writer_WriteByte(writer, k);
    }

    if(flags & PSF_AMMO)
    {
        for(i = 0; i < NUM_AMMO_TYPES; ++i)
            Writer_WriteInt16(writer, pl->ammo[i].owned);
    }

    if(flags & PSF_MAX_AMMO)
    {
#if __JDOOM__ || __JHERETIC__ || __JDOOM64__ // Hexen has no use for max ammo.
        for(i = 0; i < NUM_AMMO_TYPES; ++i)
            Writer_WriteInt16(writer, pl->ammo[i].max);
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
        fl = 0;
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
#ifdef _DEBUG
        Con_Message("NetSv_SendPlayerState: Player %i, sending morph tics as %i seconds.\n", srcPlrNum, (pl->morphTics + 34) / 35);
#endif
        // Send as seconds.
        Writer_WriteByte(writer, (pl->morphTics + 34) / 35);
    }
#endif

#if __JHEXEN__ || __JSTRIFE__
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

void NetSv_SendPlayerInfo(int whose, int to_whom)
{
    Writer* writer;

    if(IS_CLIENT)
        return;

    writer = D_NetWrite();
    Writer_WriteByte(writer, whose);
    Writer_WriteByte(writer, cfg.playerColor[whose]);
/*#ifdef _DEBUG
    Con_Message("NetSv_SendPlayerInfo: To %i, player %i's color is %i.\n", to_whom, whose, cfg.playerColor[whose]);
#endif*/
#if __JHERETIC__ || __JHEXEN__
    Writer_WriteByte(writer, cfg.playerClass[whose]); // current class
#endif
    Net_SendPacket(to_whom, GPT_PLAYER_INFO, Writer_Data(writer), Writer_Size(writer));
}

void NetSv_ChangePlayerInfo(int from, Reader* msg)
{
    int                 col;
    int                 newClass;
    player_t*           pl = &players[from];

    // Color is first.
    col = Reader_ReadByte(msg);
    cfg.playerColor[from] = PLR_COLOR(from, col);

    // Player class.
    newClass = Reader_ReadByte(msg);
    P_SetPlayerRespawnClass(from, newClass); // requesting class change?

#ifdef _DEBUG
    Con_Message("NetSv_ChangePlayerInfo: pl%i, col=%i, requested class=%i\n",
                from, cfg.playerColor[from], newClass);
#endif

    // The 'colorMap' variable controls the setting of the color
    // translation flags when the player is (re)spawned.
    pl->colorMap = cfg.playerColor[from];

    if(pl->plr->mo)
    {
        // Change the player's mobj's color translation flags.
        pl->plr->mo->flags &= ~MF_TRANSLATION;
        pl->plr->mo->flags |= cfg.playerColor[from] << MF_TRANSSHIFT;
    }

#ifdef _DEBUG
    if(pl->plr->mo)
    {
        Con_Message("Player %i mo %i translation flags %x\n", from, pl->plr->mo->thinker.id,
                    (pl->plr->mo->flags & MF_TRANSLATION) >> MF_TRANSSHIFT);
    }
#endif

    // Re-deal start spots.
    P_DealPlayerStarts(0);

    // Tell the other clients about the change.
    NetSv_SendPlayerInfo(from, DDSP_ALL_PLAYERS);
}

/**
 * Sends the frags of player 'whose' to all other players.
 */
void NetSv_FragsForAll(player_t *player)
{
    NetSv_SendPlayerState(player - players, DDSP_ALL_PLAYERS, PSF_FRAGS, true);
}

/**
 * Calculates the frags of player 'pl'.
 */
int NetSv_GetFrags(int pl)
{
    int         i, frags = 0;

    for(i = 0; i < MAXPLAYERS; ++i)
    {
#if __JDOOM__ || __JDOOM64__
        frags += players[pl].frags[i] * (i == pl ? -1 : 1);
#else
        frags += players[pl].frags[i];
#endif
    }

    return frags;
}

/**
 * Send one of the kill messages, depending on the weapon of the killer.
 */
void NetSv_KillMessage(player_t *killer, player_t *fragged, boolean stomping)
{
#if __JDOOM__ || __JDOOM64__
    char buf[500], *in, tmp[2];

    if(!cfg.killMessages || !deathmatch)
        return;

    buf[0] = 0;
    tmp[1] = 0;

    // Choose the right kill message template.
    in = GET_TXT(stomping ? TXT_KILLMSG_STOMP : killer ==
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
                in++;
        }
        tmp[0] = *in;
        strcat(buf, tmp);
    }

    // Send the message to everybody.
    NetSv_SendMessage(DDSP_ALL_PLAYERS, buf);
#endif
}

void NetSv_SendPlayerClass(int plrNum, char cls)
{
    Writer* writer;

#ifdef _DEBUG
    Con_Message("NetSv_SendPlayerClass: Player %i has class %i.\n", plrNum, cls);
#endif
    writer = D_NetWrite();
    Writer_WriteByte(writer, cls);
    Net_SendPacket(plrNum, GPT_CLASS, Writer_Data(writer), Writer_Size(writer));
}

/**
 * The default jump power is 9.
 */
void NetSv_SendJumpPower(int target, float power)
{
    Writer* writer;

    if(!IS_SERVER)
        return;

    writer = D_NetWrite();
    Writer_WriteFloat(writer, power);
    Net_SendPacket(target, GPT_JUMP_POWER, Writer_Data(writer), Writer_Size(writer));
}

void NetSv_ExecuteCheat(int player, const char* command)
{
    // Killing self is always allowed.
    /// @todo fixme: really? Even in deathmatch??
    if(!strnicmp(command, "suicide", 7))
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
    if(!strnicmp(command, "god",    3) ||
       !strnicmp(command, "noclip", 6) ||
       !strnicmp(command, "give",   4))
    {
        DD_Executef(false, "%s %i", command, player);
    }
}

/**
 * Process the requested cheat command, if possible.
 */
void NetSv_DoCheat(int player, Reader* msg)
{
    size_t len = Reader_ReadUInt16(msg);
    char* command = Z_Calloc(len + 1, PU_GAMESTATIC, 0);

    Reader_Read(msg, command, len);
    NetSv_ExecuteCheat(player, command);
    Z_Free(command);
}

/**
 * Calls @a callback on @a thing while it is temporarily placed at the
 * specified position and angle. Afterwards the thing's old position is restored.
 */
void NetSv_TemporaryPlacedCallback(mobj_t* thing, void* param, coord_t tempOrigin[3],
                                   angle_t angle, void (*callback)(mobj_t*, void*))
{
    coord_t oldOrigin[3] = { thing->origin[VX], thing->origin[VY], thing->origin[VZ] };
    coord_t oldFloorZ = thing->floorZ;
    coord_t oldCeilingZ = thing->ceilingZ;
    angle_t oldAngle = thing->angle;

    // We will temporarily move the object to the temp coords.
    if(P_CheckPosition(thing, tempOrigin))
    {
        P_MobjUnlink(thing);
        thing->origin[VX] = tempOrigin[VX];
        thing->origin[VY] = tempOrigin[VY];
        thing->origin[VZ] = tempOrigin[VZ];
        P_MobjLink(thing, DDLINK_SECTOR | DDLINK_BLOCKMAP);
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
    P_MobjLink(thing, DDLINK_SECTOR | DDLINK_BLOCKMAP);
    thing->floorZ = oldFloorZ;
    thing->ceilingZ = oldCeilingZ;
    thing->angle = oldAngle;
}

static void NetSv_UseActionCallback(mobj_t* mo, void* param)
{
    P_UseLines((player_t*)param);
}

static void NetSv_FireWeaponCallback(mobj_t* mo, void* param)
{
    P_FireWeapon((player_t*)param);
}

static void NetSv_HitFloorCallback(mobj_t* mo, void* param)
{
#ifdef _DEBUG
    Con_Message("NetSv_HitFloorCallback: mo %i\n", mo->thinker.id);
#endif
    P_HitFloor(mo);
}

void NetSv_DoFloorHit(int player, Reader* msg)
{
    player_t* plr = &players[player];
    mobj_t* mo;
    coord_t pos[3];
    coord_t mom[3];

    if(player < 0 || player >= MAXPLAYERS)
        return;

    mo = plr->plr->mo;
    if(!mo) return;

    pos[VX] = Reader_ReadFloat(msg);
    pos[VY] = Reader_ReadFloat(msg);
    pos[VZ] = Reader_ReadFloat(msg);

    // The momentum is included, although we don't really need it.
    mom[MX] = Reader_ReadFloat(msg);
    mom[MY] = Reader_ReadFloat(msg);
    mom[MZ] = Reader_ReadFloat(msg);

    NetSv_TemporaryPlacedCallback(mo, 0, pos, mo->angle, NetSv_HitFloorCallback);
}

/**
 * Process the requested player action, if possible.
 */
void NetSv_DoAction(int player, Reader* msg)
{
    int type = 0;
    coord_t pos[3];
    angle_t angle = 0;
    float lookDir = 0;
    int actionParam = 0;
    player_t* pl = &players[player];

    type = Reader_ReadInt32(msg);
    pos[VX] = Reader_ReadFloat(msg);
    pos[VY] = Reader_ReadFloat(msg);
    pos[VZ] = Reader_ReadFloat(msg);
    angle = Reader_ReadUInt32(msg);
    lookDir = Reader_ReadFloat(msg);
    actionParam = Reader_ReadInt32(msg);

#ifdef _DEBUG
    Con_Message("NetSv_DoAction: player=%i, type=%i, xyz=(%.1f,%.1f,%.1f)\n  "
                "angle=%x lookDir=%g weapon=%i\n",
                player, type, pos[VX], pos[VY], pos[VZ],
                angle, lookDir, actionParam);
#endif

    if(G_GameState() != GS_MAP)
    {
        if(G_GameState() == GS_INTERMISSION)
        {
            if(type == GPA_USE || type == GPA_FIRE)
            {
#ifdef _DEBUG
                Con_Message("NetSv_DoAction: Intermission accelerate.\n");
#endif
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
            // Update lookdir.
            pl->plr->lookDir = lookDir;

            NetSv_TemporaryPlacedCallback(pl->plr->mo, pl, pos, angle,
                                          type == GPA_USE? NetSv_UseActionCallback : NetSv_FireWeaponCallback);
            /*
            mobj_t* mo = pl->plr->mo;
            coord_t oldPos[3] = { mo->origin[VX], mo->origin[VY], mo->origin[VZ] };
            coord_t oldFloorZ = mo->floorZ;
            coord_t oldCeilingZ = mo->ceilingZ;

            // We will temporarily move the object to the action coords.
            if(P_CheckPosition(mo, pos))
            {
                P_MobjUnlink(mo);
                mo->origin[VX] = pos[VX];
                mo->origin[VY] = pos[VY];
                mo->origin[VZ] = pos[VZ];
                P_MobjLink(mo, DDLINK_SECTOR | DDLINK_BLOCKMAP);
                mo->floorZ = tmFloorZ;
                mo->ceilingZ = tmCeilingZ;
            }
            mo->angle = angle;

            if(type == GPA_USE)
                P_UseLines(pl);
            else
                P_FireWeapon(pl);

            // Restore the old position.
            P_MobjUnlink(mo);
            mo->origin[VX] = oldPos[VX];
            mo->origin[VY] = oldPos[VY];
            mo->origin[VZ] = oldPos[VZ];
            P_MobjLink(mo, DDLINK_SECTOR | DDLINK_BLOCKMAP);
            mo->floorZ = oldFloorZ;
            mo->ceilingZ = oldCeilingZ;*/
        }
        break;

    case GPA_CHANGE_WEAPON:
        pl->brain.changeWeapon = actionParam;
        break;

    case GPA_USE_FROM_INVENTORY:
#if __JHERETIC__ || __JHEXEN__ || __JDOOM64__
        P_InventoryUse(player, actionParam, true);
#endif
        break;
    }
}

void NetSv_DoDamage(int player, Reader* msg)
{
    int damage = Reader_ReadInt32(msg);
    thid_t target = Reader_ReadUInt16(msg);
    thid_t inflictor = Reader_ReadUInt16(msg);
    thid_t source = Reader_ReadUInt16(msg);

#ifdef _DEBUG
    Con_Message("NetSv_DoDamage: Client %i requests damage %i on %i via %i by %i.\n",
                player, damage, target, inflictor, source);
#endif

    P_DamageMobj2(P_MobjForID(target), P_MobjForID(inflictor), P_MobjForID(source), damage,
                  false /*not stomping*/, true /*just do it*/);
}

void NetSv_SaveGame(unsigned int game_id)
{
    Writer* writer;

    if(!IS_SERVER || !IS_NETGAME)
        return;

    // This will make the clients save their games.
    writer = D_NetWrite();
    Writer_WriteUInt32(writer, game_id);
    Net_SendPacket(DDSP_ALL_PLAYERS, GPT_SAVE, Writer_Data(writer), Writer_Size(writer));
}

void NetSv_LoadGame(unsigned int game_id)
{
    Writer* writer;

    if(!IS_SERVER || !IS_NETGAME)
        return;

    writer = D_NetWrite();
    Writer_WriteUInt32(writer, game_id);
    Net_SendPacket(DDSP_ALL_PLAYERS, GPT_LOAD, Writer_Data(writer), Writer_Size(writer));
}

/**
 * Inform all clients about a change in the 'pausedness' of a game.
 */
void NetSv_Paused(boolean isPaused)
{
    Writer* writer;

    if(!IS_SERVER || !IS_NETGAME)
        return;

    writer = D_NetWrite();
    Writer_WriteByte(writer, (isPaused != false));
    Net_SendPacket(DDSP_ALL_PLAYERS, GPT_PAUSE, Writer_Data(writer), Writer_Size(writer));
}

void NetSv_SendMessageEx(int plrNum, const char *msg, boolean yellow)
{
    Writer* writer;

    if(IS_CLIENT || !netSvAllowSendMsg)
        return;

    if(plrNum >= 0 && plrNum < MAXPLAYERS)
        if(!players[plrNum].plr->inGame)
            return;

#ifdef _DEBUG
    Con_Message("NetSv_SendMessageEx: Message '%s'\n", msg);
#endif

    if(plrNum == DDSP_ALL_PLAYERS)
    {
        // Also show locally. No sound is played!
        D_NetMessageNoSound(CONSOLEPLAYER, msg);
    }

    writer = D_NetWrite();
    Writer_WriteUInt16(writer, strlen(msg));
    Writer_Write(writer, msg, strlen(msg));
    Net_SendPacket(plrNum,
                   yellow ? GPT_YELLOW_MESSAGE : GPT_MESSAGE,
                   Writer_Data(writer), Writer_Size(writer));
}

void NetSv_SendMessage(int plrNum, const char* msg)
{
    NetSv_SendMessageEx(plrNum, msg, false);
}

void NetSv_SendYellowMessage(int plrNum, const char* msg)
{
    NetSv_SendMessageEx(plrNum, msg, true);
}

void NetSv_MaybeChangeWeapon(int plrNum, int weapon, int ammo, int force)
{
    Writer* writer;

    if(IS_CLIENT) return;
    if(plrNum < 0 || plrNum >= MAXPLAYERS)
        return;

#ifdef _DEBUG
    Con_Message("NetSv_MaybeChangeWeapon: Plr=%i Weapon=%i Ammo=%i Force=%i\n",
                plrNum, weapon, ammo, force);
#endif

    writer = D_NetWrite();
    Writer_WriteInt16(writer, weapon);
    Writer_WriteInt16(writer, ammo);
    Writer_WriteByte(writer, force != 0);
    Net_SendPacket(plrNum, GPT_MAYBE_CHANGE_WEAPON, Writer_Data(writer), Writer_Size(writer));
}

void NetSv_SendLocalMobjState(mobj_t* mobj, const char* stateName)
{
    Writer* msg;
    ddstring_t name;

    assert(mobj);

    Str_InitStatic(&name, stateName);

    // Inform the client about this.
    msg = D_NetWrite();
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

void P_Telefrag(mobj_t *thing)
{
    P_TeleportMove(thing, thing->origin[VX], thing->origin[VY], false);
}

/**
 * Handles the console commands "startcycle" and "endcycle".
 */
D_CMD(MapCycle)
{
    int map;
    int i;

    if(!IS_SERVER)
    {
        Con_Printf("Only allowed for a server.\n");
        return false;
    }

    if(!stricmp(argv[0], "startcycle")) // (Re)start rotation?
    {
        // Find the first map in the sequence.
        map = NetSv_ScanCycle(cycleIndex = 0, 0);
        if(map < 0)
        {
            Con_Printf("MapCycle \"%s\" is invalid.\n", mapCycle);
            return false;
        }
        for(i = 0; i < MAXPLAYERS; ++i)
            cycleRulesCounter[i] = 0;
        // Warp there.
        NetSv_CycleToMapNum(map);
        cyclingMaps = true;
    }
    else
    {   // OK, then we need to end it.
        if(cyclingMaps)
        {
            cyclingMaps = false;
            NetSv_SendMessage(DDSP_ALL_PLAYERS, "MAP ROTATION ENDS");
        }
    }

    return true;
}
