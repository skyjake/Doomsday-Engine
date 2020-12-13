/** @file p_mapsetup.cpp  Common map setup routines.
 *
 * Management of extended map data objects (e.g., xlines).
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2014 Daniel Swanson <danij@dengine.net>
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
#include "p_mapsetup.h"

#include <doomsday/world/entitydef.h>
#include <gamefw/mapspot.h>
#include <de/Script>
#include <de/Process>

#include <cmath>
#include <cctype>  // isspace
#include <cstring>
#include "acs/script.h"
#include "acs/system.h"
#include "d_net.h"
#include "d_netsv.h"
#include "dmu_lib.h"
#include "g_common.h"
#include "gamesession.h"
#include "hu_pspr.h"
#include "hu_stuff.h"
#include "hud/widgets/automapwidget.h"
#include "p_actor.h"
#include "p_scroll.h"
#include "p_start.h"
#include "p_tick.h"
#include "polyobjs.h"
#include "r_common.h"

#if __JDOOM64__
# define TOLIGHTIDX(c) (!((c) >> 8)? 0 : ((c) - 0x100) + 1)
#endif

using namespace de;
using namespace common;

// Our private map data structures
xsector_t *xsectors;
xline_t *xlines;

// If true we are in the process of setting up a map
dd_bool mapSetup;

xline_t *P_ToXLine(Line *line)
{
    if(!line) return NULL;

    // Is it a dummy?
    if(P_IsDummy(line))
    {
        return (xline_t *)P_DummyExtraData(line);
    }
    else
    {
        return &xlines[P_ToIndex(line)];
    }
}

xline_t *P_GetXLine(int idx)
{
    if(idx < 0 || idx >= numlines) return 0;
    return &xlines[idx];
}

void P_SetLineAutomapVisibility(int player, int lineIdx, dd_bool visible)
{
    Line *line = (Line *)P_ToPtr(DMU_LINE, lineIdx);
    if(!line || P_IsDummy(line)) return;

    xline_t *xline = P_ToXLine(line);

    // Will we need to rebuild one or more display lists?
    if(xline->mapped[player] != visible)
    {
        xline->mapped[player] = visible;

        if(auto *automap = ST_TryFindAutomapWidget(player))
        {
            automap->lineAutomapVisibilityChanged(*line);
        }
    }
}

xsector_t *P_ToXSector(Sector *sector)
{
    if(!sector) return NULL;

    // Is it a dummy?
    if(P_IsDummy(sector))
    {
        return (xsector_t *)P_DummyExtraData(sector);
    }
    else
    {
        return &xsectors[P_ToIndex(sector)];
    }
}

xsector_t const *P_ToXSector_const(Sector const *sector)
{
    return P_ToXSector(const_cast<Sector *>(sector));
}

xsector_t *P_GetXSector(int index)
{
    if(index < 0 || index >= numsectors) return 0;
    return &xsectors[index];
}

#if __JDOOM64__
static void getSurfaceColor(uint idx, float rgba[4])
{
    if(!idx)
    {
        rgba[0] = rgba[1] = rgba[2] = rgba[3] = 1;
    }
    else
    {
        rgba[0] = P_GetGMOFloat(MO_LIGHT, idx-1, MO_COLORR);
        rgba[1] = P_GetGMOFloat(MO_LIGHT, idx-1, MO_COLORG);
        rgba[2] = P_GetGMOFloat(MO_LIGHT, idx-1, MO_COLORB);
        rgba[3] = 1;
    }
}

typedef struct applysurfacecolorparams_s {
    Sector *frontSec;
    float topColor[4];
    float bottomColor[4];
} applysurfacecolorparams_t;

int applySurfaceColor(void *obj, void *context)
{
#define LDF_NOBLENDTOP          32
#define LDF_NOBLENDBOTTOM       64
#define LDF_BLEND               128

#define LTF_SWAPCOLORS          4

    Line *li = (Line *) obj;
    applysurfacecolorparams_t *params = (applysurfacecolorparams_t *) context;
    byte dFlags = P_GetGMOByte(MO_XLINEDEF, P_ToIndex(li), MO_DRAWFLAGS);
    byte tFlags = P_GetGMOByte(MO_XLINEDEF, P_ToIndex(li), MO_TEXFLAGS);

    if((dFlags & LDF_BLEND) && params->frontSec == P_GetPtrp(li, DMU_FRONT_SECTOR))
    {
        if(Side *side = (Side *)P_GetPtrp(li, DMU_FRONT))
        {
            float *top    = (tFlags & LTF_SWAPCOLORS)? params->bottomColor : params->topColor;
            float *bottom = (tFlags & LTF_SWAPCOLORS)? params->topColor : params->bottomColor;
            int flags;

            P_SetFloatpv(side, DMU_TOP_COLOR, top);
            P_SetFloatpv(side, DMU_BOTTOM_COLOR, bottom);

            flags = P_GetIntp(side, DMU_FLAGS);
            if(!(dFlags & LDF_NOBLENDTOP))
                flags |= SDF_BLENDTOPTOMID;
            if(!(dFlags & LDF_NOBLENDBOTTOM))
                flags |= SDF_BLENDBOTTOMTOMID;

            P_SetIntp(side, DMU_FLAGS, flags);
        }
    }

    if((dFlags & LDF_BLEND) && params->frontSec == P_GetPtrp(li, DMU_BACK_SECTOR))
    {
        if(Side *side = (Side *)P_GetPtrp(li, DMU_BACK))
        {
            float *top    = /*(tFlags & LTF_SWAPCOLORS)? params->bottomColor :*/ params->topColor;
            float *bottom = /*(tFlags & LTF_SWAPCOLORS)? params->topColor :*/ params->bottomColor;
            int flags;

            P_SetFloatpv(side, DMU_TOP_COLOR, top);
            P_SetFloatpv(side, DMU_BOTTOM_COLOR, bottom);

            flags = P_GetIntp(side, DMU_FLAGS);
            if(!(dFlags & LDF_NOBLENDTOP))
                flags |= SDF_BLENDTOPTOMID;
            if(!(dFlags & LDF_NOBLENDBOTTOM))
                flags |= SDF_BLENDBOTTOMTOMID;

            P_SetIntp(side, DMU_FLAGS, flags);
        }
    }

    return false; // Continue iteration
}
#endif

static dd_bool checkMapSpotSpawnFlags(mapspot_t const *spot)
{
#if __JHEXEN__
    /// @todo Move to classinfo_t
    static uint classFlags[] = {
        MSF_FIGHTER,
        MSF_CLERIC,
        MSF_MAGE
    };
#endif

    // Don't spawn things flagged for Multiplayer if we're not in a netgame.
    if(!IS_NETGAME && (spot->flags & MSF_NOTSINGLE))
        return false;

    // Don't spawn things flagged for Not Deathmatch if we're deathmatching.
    if(gfw_Rule(deathmatch) && (spot->flags & MSF_NOTDM))
        return false;

    // Don't spawn things flagged for Not Coop if we're coop'in.
    if(IS_NETGAME && !gfw_Rule(deathmatch) && (spot->flags & MSF_NOTCOOP))
        return false;

    // The special "spawn no things" skill mode means nothing is spawned.
    if(gfw_Rule(skill) == SM_NOTHINGS)
        return false;

    // Check for appropriate skill level.
    if(!(spot->skillModes & (1 << gfw_Rule(skill))))
        return false;

#if __JHEXEN__
    // Check current character classes with spawn flags.
    if(!IS_NETGAME)
    {
        // Single player.
        if((spot->flags & classFlags[P_ClassForPlayerWhenRespawning(0, false)]) == 0)
        {
            // Not for current class.
            return false;
        }
    }
    else if(!gfw_Rule(deathmatch))
    {
        // Cooperative mode.

        // No players are in the game when a dedicated server is started.
        // Also, players with new classes may join a game at any time.
        // Thus we will be generous and spawn stuff for all the classes.
        int spawnMask = MSF_FIGHTER | MSF_CLERIC | MSF_MAGE;

        if((spot->flags & spawnMask) == 0)
        {
            return false;
        }
    }
#endif

    return true;
}

/**
 * Determines if a client is allowed to spawn a thing of type @a doomEdNum.
 */
static dd_bool P_IsClientAllowedToSpawn(int doomEdNum)
{
    switch(doomEdNum)
    {
    case 11: // Deathmatch
    case 1:
    case 2:
    case 3:
    case 4:
#if __JHEXEN__
    case 9100: // Player starts 5 through 8.
    case 9101:
    case 9102:
    case 9103:
#endif
        return true;

    default:
        return false;
    }
}

/**
 * Should we auto-spawn one or more mobjs from the specified map spot?
 */
static dd_bool checkMapSpotAutoSpawn(mapspot_t const *spot)
{
#if __JHERETIC__
    // Ambient sound sequence activator?
    if(spot->doomEdNum >= 1200 && spot->doomEdNum < 1300)
        return false;
#elif __JHEXEN__
    // Sound sequence origin?
    if(spot->doomEdNum >= 1400 && spot->doomEdNum < 1410)
        return false;
#endif

    // The following are currently handled by special-case spawn logic elsewhere.
    switch(spot->doomEdNum)
    {
    case 1:    // Player starts 1 through 4.
    case 2:
    case 3:
    case 4:
    case 11:   // Player start (deathmatch).
#if __JHERETIC__
    case 56:   // Boss spot.
    case 2002: // Mace spot.
#endif
#if __JHEXEN__
    case 3000: // Polyobj origins.
    case 3001:
    case 3002:
    case 9100: // Player starts 5 through 8.
    case 9101:
    case 9102:
    case 9103:
#endif
        return false;

    default: break;
    }

    // So far so good. Now check the flags to make the final decision.
    return checkMapSpotSpawnFlags(spot);
}

static void initXLines()
{
    xlines = (xline_t *)Z_Calloc(numlines * sizeof(xline_t), PU_MAP, 0);

    for(int i = 0; i < numlines; ++i)
    {
        xline_t *xl = &xlines[i];

        xl->flags   = P_GetGMOShort(MO_XLINEDEF, i, MO_FLAGS) & ML_VALID_MASK;
#if __JHEXEN__
        xl->special = P_GetGMOByte(MO_XLINEDEF, i, MO_TYPE);
        xl->arg1    = P_GetGMOByte(MO_XLINEDEF, i, MO_ARG0);
        xl->arg2    = P_GetGMOByte(MO_XLINEDEF, i, MO_ARG1);
        xl->arg3    = P_GetGMOByte(MO_XLINEDEF, i, MO_ARG2);
        xl->arg4    = P_GetGMOByte(MO_XLINEDEF, i, MO_ARG3);
        xl->arg5    = P_GetGMOByte(MO_XLINEDEF, i, MO_ARG4);
#else
# if __JDOOM64__
        xl->special = P_GetGMOByte(MO_XLINEDEF, i, MO_TYPE);
# else
        xl->special = P_GetGMOShort(MO_XLINEDEF, i, MO_TYPE);
# endif
        xl->tag     = P_GetGMOShort(MO_XLINEDEF, i, MO_TAG);
#endif
    }
}

static void initXSectors()
{
    xsectors = (xsector_t *)Z_Calloc(numsectors * sizeof(xsector_t), PU_MAP, 0);

    for(int i = 0; i < numsectors; ++i)
    {
        xsector_t *xsec = &xsectors[i];

        xsec->special = P_GetGMOShort(MO_XSECTOR, i, MO_TYPE);
        xsec->tag     = P_GetGMOShort(MO_XSECTOR, i, MO_TAG);

#if __JDOOM64__
        applysurfacecolorparams_t params;
        float rgba[4];
        Sector *sec = (Sector *)P_ToPtr(DMU_SECTOR, i);

        getSurfaceColor(TOLIGHTIDX(P_GetGMOShort(MO_XSECTOR, i, MO_FLOORCOLOR)), rgba);
        P_SetFloatpv(sec, DMU_FLOOR_COLOR, rgba);

        getSurfaceColor(TOLIGHTIDX(P_GetGMOShort(MO_XSECTOR, i, MO_CEILINGCOLOR)), rgba);
        P_SetFloatpv(sec, DMU_CEILING_COLOR, rgba);

        // Now set the side surface colors.
        params.frontSec = sec;
        getSurfaceColor(TOLIGHTIDX(P_GetGMOShort(MO_XSECTOR, i, MO_WALLTOPCOLOR)), params.topColor);
        getSurfaceColor(TOLIGHTIDX(P_GetGMOShort(MO_XSECTOR, i, MO_WALLBOTTOMCOLOR)), params.bottomColor);

        P_Iteratep(sec, DMU_LINE, applySurfaceColor, &params);
#endif
    }
}

static void initMapSpots()
{
    numMapSpots = P_CountMapObjs(MO_THING);
    mapSpots = reinterpret_cast<mapspot_t *>(Z_Malloc(numMapSpots * sizeof(mapspot_t), PU_MAP, 0));

    for(int i = 0; i < int(numMapSpots); ++i)
    {
        mapspot_t *spot = &mapSpots[i];

        spot->origin[VX] = P_GetGMODouble(MO_THING, i, MO_X);
        spot->origin[VY] = P_GetGMODouble(MO_THING, i, MO_Y);
        spot->origin[VZ] = P_GetGMODouble(MO_THING, i, MO_Z);

        spot->doomEdNum  = P_GetGMOInt(MO_THING, i, MO_DOOMEDNUM);
        spot->skillModes = P_GetGMOInt(MO_THING, i, MO_SKILLMODES);
        spot->angle      = P_GetGMOAngle(MO_THING, i, MO_ANGLE);
        spot->flags      = P_GetGMOInt(MO_THING, i, MO_FLAGS);

#if __JHEXEN__
        spot->tid     = P_GetGMOShort(MO_THING, i, MO_ID);
        spot->special = P_GetGMOByte(MO_THING, i, MO_SPECIAL);
        spot->arg1    = P_GetGMOByte(MO_THING, i, MO_ARG0);
        spot->arg2    = P_GetGMOByte(MO_THING, i, MO_ARG1);
        spot->arg3    = P_GetGMOByte(MO_THING, i, MO_ARG2);
        spot->arg4    = P_GetGMOByte(MO_THING, i, MO_ARG3);
        spot->arg5    = P_GetGMOByte(MO_THING, i, MO_ARG4);
#endif

#if __JHERETIC__
        // Ambient sound sequence activator?
        if(spot->doomEdNum >= 1200 && spot->doomEdNum < 1300)
        {
            P_AddAmbientSfx(spot->doomEdNum - 1200);
            continue;
        }
#elif __JHEXEN__
        // Sound sequence origin?
        if(spot->doomEdNum >= 1400 && spot->doomEdNum < 1410)
        {
            xsector_t *xsector = P_ToXSector(Sector_AtPoint_FixedPrecision(spot->origin));

            xsector->seqType = seqtype_t(spot->doomEdNum - 1400);
            continue;
        }
#endif

        switch(spot->doomEdNum)
        {
        case 11: // Player start (deathmatch).
            P_CreatePlayerStart(0, 0, true, i);
            break;

        case 1: // Player starts 1 through 4.
        case 2:
        case 3:
        case 4: {
#if __JHEXEN__
            uint entryPoint = spot->arg1;
#else
            uint entryPoint = 0;
#endif

            P_CreatePlayerStart(spot->doomEdNum, entryPoint, false, i);
            break; }

#if __JHERETIC__
        case 56: // Boss spot.
            P_AddBossSpot(i);
            break;

        case 2002:
            if(gameMode != heretic_shareware)
                P_AddMaceSpot(i);
            break;
#endif

#if __JHEXEN__
        case 3000: // Polyobj origins.
        case 3001:
        case 3002:
            break;

        case 9100: // Player starts 5 through 8.
        case 9101:
        case 9102:
        case 9103:
            P_CreatePlayerStart(5 + spot->doomEdNum - 9100, spot->arg1, false, i);
            break;
#endif
        default: // No special handling.
            break;
        }
    }

    P_DealPlayerStarts(0);

    if(gfw_Rule(deathmatch))
    {
        uint numDMStarts = P_GetNumPlayerStarts(true);
        uint playerCount = 0;

        for(int i = 0; i < MAXPLAYERS; ++i)
        {
            if(players[i].plr->inGame)
                playerCount++;
        }

        if(numDMStarts < playerCount)
        {
            App_Log(DE2_MAP_WARNING, "Not enough deathmatch spots in map (%i players, %i DM spots)",
                    playerCount, numDMStarts);
        }
    }
}

#if __JHERETIC__
mapspot_t const *P_ChooseRandomMaceSpot()
{
    if(!maceSpots || !maceSpotCount)
        return 0;

    /*
     * Pass 1: Determine how many spots qualify given the current game rules.
     */
    uint numQualifyingSpots = 0;
    for(uint i = 0; i < maceSpotCount; ++i)
    {
        mapspotid_t mapSpotId = maceSpots[i];
        DENG_ASSERT(mapSpots != 0 && mapSpotId < numMapSpots);
        mapspot_t const *mapSpot = &mapSpots[mapSpotId];

        // Does this spot qualify given the current game configuration?
        if(checkMapSpotSpawnFlags(mapSpot))
            numQualifyingSpots += 1;
    }
    if(!numQualifyingSpots)
        return 0;

    /*
     * Pass 2: Choose and locate the chosen spot.
     */
    uint chosenQualifyingSpotIdx = M_Random() % numQualifyingSpots;
    uint qualifyingSpotIdx = 0;
    for(uint i = 0; i < maceSpotCount; ++i)
    {
        mapspotid_t mapSpotId = maceSpots[i];
        mapspot_t const *mapSpot = &mapSpots[mapSpotId];

        if(!checkMapSpotSpawnFlags(mapSpot))
            continue;

        if(qualifyingSpotIdx != chosenQualifyingSpotIdx)
        {
            qualifyingSpotIdx++;
            continue;
        }

        App_Log(DE2_DEV_MAP_MSG, "P_ChooseRandomMaceSpot: Chosen map spot id:%u.", mapSpotId);

        return mapSpot;
    }

    // Unreachable.
    DENG_ASSERT(false);
    return 0;
}
#endif // __JHERETIC__

static void spawnMapObjects()
{
    for(uint i = 0; i < numMapSpots; ++i)
    {
        mapspot_t const *spot = &mapSpots[i];

        // Not all map spots spawn mobjs on map load.
        if(!checkMapSpotAutoSpawn(spot))
            continue;

        // A spot that should auto-spawn one (or more) mobjs.

        // Find which type to spawn.
        mobjtype_t type = P_DoomEdNumToMobjType(spot->doomEdNum);
        if(type != MT_NONE)
        {
            // Check for things that clients don't spawn on their own.
            if(IS_CLIENT)
            {
                // Client is allowed to spawn objects that are flagged local.
                // The server will not send any information about them.
                if(!(MOBJINFO[type].flags & MF_LOCAL))
                {
                    if(!P_IsClientAllowedToSpawn(spot->doomEdNum))
                        continue;
                }
            }

            App_Log(DE2_DEV_MAP_XVERBOSE, "Spawning mobj at (%g, %g, %g) angle:%i ednum:%i flags:%x",
                    spot->origin[VX], spot->origin[VY], spot->origin[VZ], spot->angle,
                    spot->doomEdNum, spot->flags);

            if(mobj_t *mo = P_SpawnMobj(type, spot->origin, spot->angle, spot->flags))
            {
                if(mo->tics > 0)
                    mo->tics = 1 + (P_Random() % mo->tics);

#if __JHEXEN__
                mo->tid = spot->tid;
                mo->special = spot->special;
                mo->args[0] = spot->arg1;
                mo->args[1] = spot->arg2;
                mo->args[2] = spot->arg3;
                mo->args[3] = spot->arg4;
                mo->args[4] = spot->arg5;
#endif

#if __JHEXEN__
                if(mo->flags2 & MF2_FLOATBOB)
                    mo->special1 = FLT2FIX(spot->origin[VZ]);
#endif

#if __JDOOM__ || __JDOOM64__ || __JHERETIC__
                if(mo->flags & MF_COUNTKILL)
                    totalKills++;
                if(mo->flags & MF_COUNTITEM)
                    totalItems++;
#endif
            }
        }
        else
        {
            App_Log(DE2_MAP_WARNING, "Unknown DoomEdNum %i at (%g, %g, %g)",
                    spot->doomEdNum, spot->origin[VX], spot->origin[VY], spot->origin[VZ]);
        }
    }

#if __JHERETIC__
    // Spawn a Firemace?
    App_Log(DE2_DEV_MAP_VERBOSE, "spawnMapObjects: %i Firemace spot(s)", maceSpotCount);
    if (gfw_MapInfoFlags() & MIF_SPAWN_ALL_FIREMACES)
    {
        for (uint i = 0; i < maceSpotCount; ++i)
        {
            const mapspot_t *spot = &mapSpots[maceSpots[i]];
            if (checkMapSpotSpawnFlags(spot))
            {
                P_SpawnMobjXYZ(
                    MT_WMACE, spot->origin[VX], spot->origin[VY], 0, spot->angle, MSF_Z_FLOOR);
            }
        }
    }
    else
    {
        if (!IS_CLIENT && maceSpotCount)
        {
            // Sometimes the Firemace doesn't show up if not in deathmatch.
            if (gfw_Rule(deathmatch) || M_Random() >= 64)
            {
                if (mapspot_t const *spot = P_ChooseRandomMaceSpot())
                {
                    App_Log(DE2_DEV_MAP_VERBOSE,
                            "spawnMapObjects: Spawning Firemace at (%g, %g, %g)",
                            spot->origin[VX],
                            spot->origin[VY],
                            spot->origin[VZ]);

                    P_SpawnMobjXYZ(
                        MT_WMACE, spot->origin[VX], spot->origin[VY], 0, spot->angle, MSF_Z_FLOOR);
                }
            }
        }
    }
#endif

#if __JHEXEN__
    P_CreateTIDList();
#endif

    P_SpawnPlayers();
}

void P_SetupMap(de::Uri const &mapUri)
{
    if (IS_DEDICATED)
    {
        // Whenever the map changes, update the game rule config based on cvars.
        GameRules newRules(gfw_Session()->rules());
        GameRules_Set(newRules, skill,      cfg.common.netSkill);
        GameRules_Set(newRules, deathmatch, cfg.common.netDeathmatch);
        GameRules_Set(newRules, noMonsters, cfg.common.netNoMonsters);
        /*newRules.*/cfg.common.jumpEnabled = cfg.common.netJumping;
#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
        GameRules_Set(newRules, respawnMonsters, cfg.netRespawn);
#endif
#if __JHEXEN__
        GameRules_Set(newRules, randomClasses, cfg.netRandomClass);
#endif
        gfw_Session()->applyNewRules(newRules);
    }

    // If we're the server, let clients know the map will change.
    NetSv_SendGameState(GSF_CHANGE_MAP, DDSP_ALL_PLAYERS);

    // It begins...
    mapSetup = true;

    ::timerGame = 0;
    if (gfw_Rule(deathmatch))
    {
        int parm = CommandLine_Check("-timer");
        if (parm && parm < CommandLine_Count() - 1)
        {
            ::timerGame = atoi(CommandLine_At(parm + 1)) * 35 * 60;
        }
    }

    P_ResetWorldState();

    if (!P_MapChange(mapUri.compose().toUtf8().constData()))
    {
        Con_Error("P_SetupMap: Failed changing/loading map \"%s\".\n", mapUri.compose().toUtf8().constData());
        exit(1); // Unreachable.
    }

    // Make sure the game is paused for the requested period.
    Pause_MapStarted();

    // It ends.
    mapSetup = false;
}

struct mobjtype_precachedata_t
{
    mobjtype_t type;
    int gameModeBits;
};

static void precacheResources()
{
    // Disabled?
    if(!precache || IS_DEDICATED)
        return;

    R_PrecachePSprites();

#if __JDOOM__ || __JHERETIC__
    {
        static const mobjtype_precachedata_t types[] = {
#  if __JDOOM__
            { MT_SKULL,             GM_ANY },

            // Missiles:
            { MT_BRUISERSHOT,       GM_ANY },
            { MT_TROOPSHOT,         GM_ANY },
            { MT_HEADSHOT,          GM_ANY },
            { MT_ROCKET,            GM_ANY },
            { MT_PLASMA,            GM_ANY ^ GM_DOOM_SHAREWARE },
            { MT_BFG,               GM_ANY ^ GM_DOOM_SHAREWARE },
            { MT_ARACHPLAZ,         GM_DOOM2 },
            { MT_FATSHOT,           GM_DOOM2 },

            // Potentially dropped weapons:
            { MT_CLIP,              GM_ANY },
            { MT_SHOTGUN,           GM_ANY },
            { MT_CHAINGUN,          GM_ANY },

            // Misc effects:
            { MT_FIRE,              GM_DOOM2 },
            { MT_TRACER,            GM_ANY },
            { MT_SMOKE,             GM_ANY },
            { MT_FATSHOT,           GM_DOOM2 },
            { MT_BLOOD,             GM_ANY },
            { MT_PUFF,              GM_ANY },
            { MT_TFOG,              GM_ANY }, // Teleport FX.
            { MT_EXTRABFG,          GM_ANY },
            { MT_ROCKETPUFF,        GM_ANY },
#  elif __JHERETIC__
            { MT_BLOODYSKULL,       GM_ANY },
            { MT_CHICPLAYER,        GM_ANY },
            { MT_CHICKEN,           GM_ANY },

            // Player weapon effects:
            { MT_STAFFPUFF,         GM_ANY },
            { MT_STAFFPUFF2,        GM_ANY },
            { MT_BEAKPUFF,          GM_ANY },
            { MT_GAUNTLETPUFF1,     GM_ANY },
            { MT_GAUNTLETPUFF2,     GM_ANY },
            { MT_BLASTERFX1,        GM_ANY },
            { MT_BLASTERSMOKE,      GM_ANY },
            { MT_RIPPER,            GM_ANY },
            { MT_BLASTERPUFF1,      GM_ANY },
            { MT_BLASTERPUFF2,      GM_ANY },
            { MT_MACEFX1,           GM_ANY ^ GM_HERETIC_SHAREWARE },
            { MT_MACEFX2,           GM_ANY ^ GM_HERETIC_SHAREWARE },
            { MT_MACEFX3,           GM_ANY ^ GM_HERETIC_SHAREWARE },
            { MT_MACEFX4,           GM_ANY ^ GM_HERETIC_SHAREWARE },
            { MT_HORNRODFX1,        GM_ANY ^ GM_HERETIC_SHAREWARE },
            { MT_HORNRODFX2,        GM_ANY ^ GM_HERETIC_SHAREWARE },
            { MT_RAINPLR3,          GM_ANY ^ GM_HERETIC_SHAREWARE }, // SP color
            { MT_GOLDWANDFX1,       GM_ANY },
            { MT_GOLDWANDFX2,       GM_ANY },
            { MT_GOLDWANDPUFF1,     GM_ANY },
            { MT_GOLDWANDPUFF2,     GM_ANY },
            { MT_PHOENIXPUFF,       GM_ANY ^ GM_HERETIC_SHAREWARE },
            { MT_PHOENIXFX2,        GM_ANY ^ GM_HERETIC_SHAREWARE },
            { MT_CRBOWFX1,          GM_ANY },
            { MT_CRBOWFX2,          GM_ANY },
            { MT_CRBOWFX3,          GM_ANY },
            { MT_CRBOWFX4,          GM_ANY },

            // Artefacts:
            { MT_EGGFX,             GM_ANY },
            { MT_FIREBOMB,          GM_ANY },

            // Enemy effects:
            { MT_MUMMYSOUL,         GM_ANY },
            { MT_MUMMYFX1,          GM_ANY },
            { MT_BEASTBALL,         GM_ANY ^ GM_HERETIC_SHAREWARE },
            { MT_BURNBALL,          GM_ANY ^ GM_HERETIC_SHAREWARE },
            { MT_BURNBALLFB,        GM_ANY ^ GM_HERETIC_SHAREWARE },
            { MT_PUFFY,             GM_ANY ^ GM_HERETIC_SHAREWARE },
            { MT_SNAKEPRO_A,        GM_ANY ^ GM_HERETIC_SHAREWARE },
            { MT_SNAKEPRO_B,        GM_ANY ^ GM_HERETIC_SHAREWARE },
            { MT_HEADFX1,           GM_ANY },
            { MT_HEADFX2,           GM_ANY },
            { MT_HEADFX3,           GM_ANY },
            { MT_WHIRLWIND,         GM_ANY },
            { MT_WIZFX1,            GM_ANY },
            { MT_IMPCHUNK1,         GM_ANY },
            { MT_IMPCHUNK2,         GM_ANY },
            { MT_IMPBALL,           GM_ANY },
            { MT_KNIGHTAXE,         GM_ANY },
            { MT_REDAXE,            GM_ANY },
            { MT_SRCRFX1,           GM_ANY ^ GM_HERETIC_SHAREWARE },
            { MT_SORCERER2,         GM_ANY ^ GM_HERETIC_SHAREWARE },
            { MT_SOR2FX1,           GM_ANY ^ GM_HERETIC_SHAREWARE },
            { MT_SOR2FXSPARK,       GM_ANY ^ GM_HERETIC_SHAREWARE },
            { MT_SOR2FX2,           GM_ANY ^ GM_HERETIC_SHAREWARE },
            { MT_SOR2TELEFADE,      GM_ANY ^ GM_HERETIC_SHAREWARE },
            { MT_WIZARD,            GM_ANY ^ GM_HERETIC_SHAREWARE }, // In case D'sparil is on a map with no Disciples
            { MT_MNTRFX1,           GM_ANY ^ GM_HERETIC_SHAREWARE },
            { MT_MNTRFX2,           GM_ANY ^ GM_HERETIC_SHAREWARE },
            { MT_MNTRFX3,           GM_ANY ^ GM_HERETIC_SHAREWARE },

            // Potentially dropped ammo:
            { MT_AMGWNDWIMPY,       GM_ANY },
            { MT_AMCBOWWIMPY,       GM_ANY },
            { MT_AMSKRDWIMPY,       GM_ANY ^ GM_HERETIC_SHAREWARE },
            { MT_AMPHRDWIMPY,       GM_ANY ^ GM_HERETIC_SHAREWARE },
            { MT_AMBLSRWIMPY,       GM_ANY },

            // Potentially dropped artefacts:
            { MT_ARTITOMEOFPOWER,   GM_ANY },
            { MT_ARTIEGG,           GM_ANY },
            { MT_ARTISUPERHEAL,     GM_ANY ^ GM_HERETIC_SHAREWARE },

            // Misc effects:
            { MT_POD,               GM_ANY },
            { MT_PODGOO,            GM_ANY },
            { MT_SPLASH,            GM_ANY },
            { MT_SPLASHBASE,        GM_ANY },
            { MT_LAVASPLASH,        GM_ANY },
            { MT_LAVASMOKE,         GM_ANY },
            { MT_SLUDGECHUNK,       GM_ANY },
            { MT_SLUDGESPLASH,      GM_ANY },
            { MT_VOLCANOBLAST,      GM_ANY ^ GM_HERETIC_SHAREWARE },
            { MT_VOLCANOTBLAST,     GM_ANY ^ GM_HERETIC_SHAREWARE },
            { MT_TELEGLITTER,       GM_ANY },
            { MT_TELEGLITTER2,      GM_ANY },
            { MT_TFOG,              GM_ANY },
            { MT_BLOOD,             GM_ANY },
            { MT_BLOODSPLATTER,     GM_ANY },
            { MT_FEATHER,           GM_ANY },
#  endif
            { MT_NONE, 0 }
        };

        for(int i = 0; types[i].type != MT_NONE; ++i)
        {
            if(types[i].gameModeBits & gameModeBits)
            {
                Rend_CacheForMobjType(types[i].type);
            }
        }
    }

    if(IS_NETGAME)
    {
#  if __JDOOM__
        Rend_CacheForMobjType(MT_IFOG);
#  else // __JHERETIC__
        Rend_CacheForMobjType(MT_RAINPLR1);
        Rend_CacheForMobjType(MT_RAINPLR2);
        Rend_CacheForMobjType(MT_RAINPLR3);
        Rend_CacheForMobjType(MT_RAINPLR4);
#  endif
    }
#endif
}

void P_FinalizeMapChange(uri_s const *mapUri_)
{
    de::Uri const &mapUri = *reinterpret_cast<de::Uri const *>(mapUri_);
#if !__JHEXEN__
    DENG2_UNUSED(mapUri);
#endif

    initXLines();
    initXSectors();

    Thinker_Init();
#if __JHERETIC__
    P_InitAmbientSound();
#endif
#if __JHEXEN__
    P_InitCorpseQueue();
#endif

    // Script to run during map setup. It is executed at this specific point so that it can
    // initialize data for map spots and spawned things. Other script hooks could be added
    // to be called at other points during map setup (before/after, for example).
    if (const String onSetupSrc = G_MapInfoForMapUri(mapUri).gets("onSetup"))
    {
        Script script(onSetupSrc);
        Process proc;
        proc.run(script);
        proc.execute();
    }

    initMapSpots();
    spawnMapObjects();
    PO_InitForMap();

    HU_UpdatePsprites();

    // Set up world state.
    P_BuildAllTagLists();

#if !__JHEXEN__
    // Init extended generalized lines and sectors.
    XG_Init();
#endif

    gfw_Session()->acsSystem().loadModuleForMap(mapUri);
    gfw_Session()->acsSystem().worldSystemMapChanged();

#if __JDOOM__ || __JDOOM64__ || __JHERETIC__
    P_FindSecrets();
#endif
    P_SpawnAllSpecialThinkers();
    P_SpawnAllMaterialOriginScrollers();

    // Preload resources we'll likely need but which aren't present (usually) in the map.
    precacheResources();

    if(IS_SERVER)
    {
        R_SetAllDoomsdayFlags();
        NetSv_SendTotalCounts(DDSP_ALL_PLAYERS);
    }

    /*
     * Do any map finalization including any game-specific stuff.
     */

#if __JDOOM__
    // Adjust slime lower wall textures (a hack!).
    // This will hide the ugly green bright line that would otherwise be
    // visible due to texture repeating and interpolation.
    if(!(gameModeBits & (GM_DOOM2_HACX|GM_DOOM_CHEX)))
    {
        world_Material *mat = (world_Material *)P_ToPtr(DMU_MATERIAL, Materials_ResolveUriCString("Textures:NUKE24"));

        for(int i = 0; i < numlines; ++i)
        {
            Line *line = (Line *)P_ToPtr(DMU_LINE, i);

            for(int k = 0; k < 2; ++k)
            {
                Side *side = (Side *)P_GetPtrp(line, k == 0? DMU_FRONT : DMU_BACK);
                if(!side) continue;

                world_Material *bottomMat = (world_Material *)P_GetPtrp(side, DMU_BOTTOM_MATERIAL);
                world_Material *midMat    = (world_Material *)P_GetPtrp(side, DMU_MIDDLE_MATERIAL);

                if(bottomMat == mat && !midMat)
                {
                    float yoff = P_GetFloatp(side, DMU_BOTTOM_MATERIAL_OFFSET_Y);
                    P_SetFloatp(side, DMU_BOTTOM_MATERIAL_OFFSET_Y, yoff + 1.0f);
                }
            }
        }
    }

#elif __JHEXEN__
    // Initialize lightning & thunder clap effects (if in use).
    P_InitLightning();
#endif

    // Do some fine tuning with mobj placement and orientation.
#if __JHERETIC__ || __JHEXEN__
    P_MoveThingsOutOfWalls();
#endif
#if __JHERETIC__
    P_TurnGizmosAwayFromDoors();

    // Torch rendering mode for the map. By default (vanilla), Heretic has a
    // fullbright torch.
    DD_SetInteger(DD_FIXEDCOLORMAP_ATTENUATE,
                  (gfw_Session()->mapInfo().geti("flags") & MIF_DIM_TORCH) != 0);
#endif
}

void P_ResetWorldState()
{
#if __JHEXEN__
    static int firstFragReset = 1;
#endif

    ::wmInfo.nextMap.clear();
#if __JHEXEN__
    ::wmInfo.nextMapEntryPoint = 0;
#endif
#if __JDOOM__ || __JDOOM64__
    ::wmInfo.maxFrags = 0;
    ::wmInfo.parTime = -1;
#endif

#if !__JHEXEN__
    if(!IS_CLIENT)
    {
        ::totalKills = ::totalItems = ::totalSecret = 0;
    }
#endif

#if __JDOOM__
    delete ::theBossBrain;
    ::theBossBrain = new BossBrain;
#endif

#if __JHEXEN__
    SN_StopAllSequences();
#endif

#if __JHERETIC__
    ::maceSpotCount = 0;
    ::maceSpots     = 0;
    ::bossSpotCount = 0;
    ::bossSpots     = 0;
#endif

    P_PurgeDeferredSpawns();

    for(int i = 0; i < MAXPLAYERS; ++i)
    {
        player_t *plr = &::players[i];
        ddplayer_t *ddplr = plr->plr;

        ddplr->mo = NULL;
        plr->killCount = plr->secretCount = plr->itemCount = 0;
        plr->update |= PSF_COUNTERS;

        if(ddplr->inGame && plr->playerState == PST_DEAD)
            plr->playerState = PST_REBORN;

#if __JHEXEN__
        if(!IS_NETGAME || (IS_NETGAME != 0 && gfw_Rule(deathmatch) != 0) || firstFragReset == 1)
        {
            memset(plr->frags, 0, sizeof(plr->frags));
            firstFragReset = 0;
        }
#else
        std::memset(plr->frags, 0, sizeof(plr->frags));
#endif

        G_ResetLookOffset(i);
    }

#if __JDOOM__ || __JDOOM64__
    ::bodyQueueSlot = 0;
#endif

    P_DestroyPlayerStarts();

#if __JHERETIC__ || __JHEXEN__
    // The pointers in the body queue are now invalid.
    P_ClearBodyQueue();
#endif
}

#if __JDOOM__ || __JDOOM64__ || __JHERETIC__
void P_FindSecrets()
{
    totalSecret = 0;

    // Find secret sectors.
    for(int i = 0; i < numsectors; ++i)
    {
        xsector_t *xsec = P_ToXSector((Sector *)P_ToPtr(DMU_SECTOR, i));

        // XG sector types override the game's built-in types.
        if(xsec->xg) continue;

        if(xsec->special == 9)
        {
            totalSecret++;
        }
    }

#if __JDOOM64__
    // Find secret lines.
    for(int i = 0; i < numlines; ++i)
    {
        xline_t *xline = P_ToXLine((Line *)P_ToPtr(DMU_LINE, i));

        // XG line types override the game's built-in types.
        if(xline->xg) continue;

        if(xline->special == 994)
        {
            totalSecret++;
        }
    }
#endif
}
#endif

void P_SpawnSectorMaterialOriginScrollers()
{
    // Clients do not spawn material origin scrollers on their own.
    if(IS_CLIENT) return;

    for(int i = 0; i < numsectors; ++i)
    {
        Sector *sec     = (Sector *)P_ToPtr(DMU_SECTOR, i);
        xsector_t *xsec = P_ToXSector(sec);

#if !__JHEXEN__
        // XG sector types override the game's built-in types.
        if(xsec->xg) continue;
#endif

        P_SpawnSectorMaterialOriginScroller(sec, PLN_FLOOR, xsec->special);
    }
}

void P_SpawnSideMaterialOriginScrollers()
{
    // Clients do not spawn material origin scrollers on their own.
    if(IS_CLIENT) return;

    for(int i = 0; i < numlines; ++i)
    {
        Line *line     = (Line *)P_ToPtr(DMU_LINE, i);
        xline_t *xline = P_ToXLine(line);

#if !__JHEXEN__
        // XG line types override the game's built-in types.
        if(xline->xg) continue;
#endif

        Side *frontSide = (Side *)P_GetPtrp(line, DMU_FRONT);
        P_SpawnSideMaterialOriginScroller(frontSide, xline->special);
    }
}

void P_SpawnAllMaterialOriginScrollers()
{
    P_SpawnSideMaterialOriginScrollers();
    P_SpawnSectorMaterialOriginScrollers();
}
