/**
 * @file p_mapsetup.h
 * Common map setup routines.
 *
 * Management of extended map data objects (e.g., xlines) is done here.
 *
 * @ingroup libcommon
 *
 * @authors Copyright &copy; 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright &copy; 2005-2012 Daniel Swanson <danij@dengine.net>
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

#include <math.h>
#include <ctype.h>  // has isspace
#include <string.h>

#if __JDOOM__
#  include "jdoom.h"
#elif __JDOOM64__
#  include "jdoom64.h"
#elif __JHERETIC__
#  include "jheretic.h"
#elif __JHEXEN__
#  include "jhexen.h"
#endif

#include "am_map.h"
#include "dmu_lib.h"
#include "r_common.h"
#include "p_actor.h"
#include "p_mapsetup.h"
#include "p_scroll.h"
#include "p_start.h"
#include "p_tick.h"
#include "hu_pspr.h"
#include "hu_stuff.h"

#if __JDOOM64__
# define TOLIGHTIDX(c) (!((c) >> 8)? 0 : ((c) - 0x100) + 1)
#endif

typedef struct {
    int gameModeBits;
    mobjtype_t type;
} mobjtype_precachedata_t;

static void P_ResetWorldState(void);
static void P_FinalizeMap(void);

// Our private map data structures
xsector_t* xsectors;
xline_t* xlines;

// If true we are in the process of setting up a map
boolean mapSetup;

xline_t* P_ToXLine(LineDef* line)
{
    if(!line) return NULL;

    // Is it a dummy?
    if(P_IsDummy(line))
    {
        return P_DummyExtraData(line);
    }
    else
    {
        return &xlines[P_ToIndex(line)];
    }
}

xline_t* P_GetXLine(uint idx)
{
    if(idx >= numlines) return NULL;
    return &xlines[idx];
}

void P_SetLinedefAutomapVisibility(int player, uint lineIdx, boolean visible)
{
    LineDef* line = P_ToPtr(DMU_LINEDEF, lineIdx);
    xline_t* xline;
    if(!line || P_IsDummy(line)) return;

    xline = P_ToXLine(line);
    // Will we need to rebuild one or more display lists?
    if(xline->mapped[player] != visible)
    {
        ST_RebuildAutomap(player);
    }
    xline->mapped[player] = visible;
}

xsector_t* P_ToXSector(Sector* sector)
{
    if(!sector) return NULL;

    // Is it a dummy?
    if(P_IsDummy(sector))
    {
        return P_DummyExtraData(sector);
    }
    else
    {
        return &xsectors[P_ToIndex(sector)];
    }
}

xsector_t* P_ToXSectorOfBspLeaf(BspLeaf* bspLeaf)
{
    Sector* sec;

    if(!bspLeaf) return NULL;

    sec = P_GetPtrp(bspLeaf, DMU_SECTOR);

    // Is it a dummy?
    if(P_IsDummy(sec))
    {
        return P_DummyExtraData(sec);
    }
    else
    {
        return &xsectors[P_ToIndex(sec)];
    }
}

xsector_t* P_GetXSector(uint index)
{
    if(index >= numsectors)
        return NULL;

    return &xsectors[index];
}

void P_SetupForMapData(int type, uint num)
{
    switch(type)
    {
    case DMU_SECTOR:
        if(num > 0)
            xsectors = Z_Calloc(num * sizeof(xsector_t), PU_MAP, 0);
        else
            xsectors = NULL;
        break;

    case DMU_LINEDEF:
        if(num > 0)
            xlines = Z_Calloc(num * sizeof(xline_t), PU_MAP, 0);
        else
            xlines = NULL;
        break;

    default:
        break;
    }
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
    Sector* frontSec;
    float topColor[4];
    float bottomColor[4];
} applysurfacecolorparams_t;

int applySurfaceColor(void* obj, void* context)
{
#define LDF_NOBLENDTOP          32
#define LDF_NOBLENDBOTTOM       64
#define LDF_BLEND               128

#define LTF_SWAPCOLORS          4

    LineDef* li = (LineDef*) obj;
    applysurfacecolorparams_t* params = (applysurfacecolorparams_t*) context;
    byte dFlags = P_GetGMOByte(MO_XLINEDEF, P_ToIndex(li), MO_DRAWFLAGS);
    byte tFlags = P_GetGMOByte(MO_XLINEDEF, P_ToIndex(li), MO_TEXFLAGS);

    if((dFlags & LDF_BLEND) &&
       params->frontSec == P_GetPtrp(li, DMU_FRONT_SECTOR))
    {
        SideDef* side = P_GetPtrp(li, DMU_SIDEDEF0);

        if(side)
        {
            float* top    = (tFlags & LTF_SWAPCOLORS)? params->bottomColor : params->topColor;
            float* bottom = (tFlags & LTF_SWAPCOLORS)? params->topColor : params->bottomColor;
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

    if((dFlags & LDF_BLEND) &&
       params->frontSec == P_GetPtrp(li, DMU_BACK_SECTOR))
    {
        SideDef* side = P_GetPtrp(li, DMU_SIDEDEF1);

        if(side)
        {
            float* top    = /*(tFlags & LTF_SWAPCOLORS)? params->bottomColor :*/ params->topColor;
            float* bottom = /*(tFlags & LTF_SWAPCOLORS)? params->topColor :*/ params->bottomColor;
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

static boolean checkMapSpotSpawnFlags(const mapspot_t* spot)
{
#if __JHEXEN__
    /// @todo Move to classinfo_t
    static unsigned int classFlags[] = {
        MSF_FIGHTER,
        MSF_CLERIC,
        MSF_MAGE
    };
#endif

    // Don't spawn things flagged for Multiplayer if we're not in a netgame.
    if(!IS_NETGAME && (spot->flags & MSF_NOTSINGLE))
        return false;

    // Don't spawn things flagged for Not Deathmatch if we're deathmatching.
    if(deathmatch && (spot->flags & MSF_NOTDM))
        return false;

    // Don't spawn things flagged for Not Coop if we're coop'in.
    if(IS_NETGAME && !deathmatch && (spot->flags & MSF_NOTCOOP))
        return false;

    // Check for appropriate skill level.
    if(!(spot->skillModes & (1 << gameSkill)))
        return false;

#if __JHEXEN__
    // Check current character classes with spawn flags.
    if(!IS_NETGAME)
    {
        // Single player.
        if((spot->flags & classFlags[P_ClassForPlayerWhenRespawning(0, false)]) == 0)
        {   // Not for current class.
            return false;
        }
    }
    else if(!deathmatch)
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
static boolean P_IsClientAllowedToSpawn(int doomEdNum)
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
static boolean checkMapSpotAutoSpawn(const mapspot_t* spot)
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

static void initXLineDefs(void)
{
    uint i;

    for(i = 0; i < numlines; ++i)
    {
        xline_t* xl = &xlines[i];

        xl->flags = P_GetGMOShort(MO_XLINEDEF, i, MO_FLAGS) & ML_VALID_MASK;
#if __JHEXEN__
        xl->special = P_GetGMOByte(MO_XLINEDEF, i, MO_TYPE);
        xl->arg1 = P_GetGMOByte(MO_XLINEDEF, i, MO_ARG0);
        xl->arg2 = P_GetGMOByte(MO_XLINEDEF, i, MO_ARG1);
        xl->arg3 = P_GetGMOByte(MO_XLINEDEF, i, MO_ARG2);
        xl->arg4 = P_GetGMOByte(MO_XLINEDEF, i, MO_ARG3);
        xl->arg5 = P_GetGMOByte(MO_XLINEDEF, i, MO_ARG4);
#else
# if __JDOOM64__
        xl->special = P_GetGMOByte(MO_XLINEDEF, i, MO_TYPE);
# else
        xl->special = P_GetGMOShort(MO_XLINEDEF, i, MO_TYPE);
# endif
        xl->tag = P_GetGMOShort(MO_XLINEDEF, i, MO_TAG);
#endif
    }
}

static void initXSectors(void)
{
    uint i;

    for(i = 0; i < numsectors; ++i)
    {
        xsector_t* xsec = &xsectors[i];

        xsec->special = P_GetGMOShort(MO_XSECTOR, i, MO_TYPE);
        xsec->tag     = P_GetGMOShort(MO_XSECTOR, i, MO_TAG);

#if __JDOOM64__
        {
        applysurfacecolorparams_t params;
        float rgba[4];
        Sector* sec = P_ToPtr(DMU_SECTOR, i);

        getSurfaceColor(TOLIGHTIDX(P_GetGMOShort(MO_XSECTOR, i, MO_FLOORCOLOR)), rgba);
        P_SetFloatpv(sec, DMU_FLOOR_COLOR, rgba);

        getSurfaceColor(TOLIGHTIDX(P_GetGMOShort(MO_XSECTOR, i, MO_CEILINGCOLOR)), rgba);
        P_SetFloatpv(sec, DMU_CEILING_COLOR, rgba);

        // Now set the side surface colors.
        params.frontSec = sec;
        getSurfaceColor(TOLIGHTIDX(P_GetGMOShort(MO_XSECTOR, i, MO_WALLTOPCOLOR)), params.topColor);
        getSurfaceColor(TOLIGHTIDX(P_GetGMOShort(MO_XSECTOR, i, MO_WALLBOTTOMCOLOR)), params.bottomColor);

        P_Iteratep(sec, DMU_LINEDEF, &params, applySurfaceColor);
        }
#endif
    }
}

static void loadMapSpots(void)
{
    uint i;

    numMapSpots = P_CountGameMapObjs(MO_THING);
    if(numMapSpots > 0)
        mapSpots = Z_Malloc(numMapSpots * sizeof(mapspot_t), PU_MAP, 0);
    else
        mapSpots = NULL;

    for(i = 0; i < numMapSpots; ++i)
    {
        mapspot_t* spot = &mapSpots[i];

        spot->origin[VX] = P_GetGMOFloat(MO_THING, i, MO_X);
        spot->origin[VY] = P_GetGMOFloat(MO_THING, i, MO_Y);
        spot->origin[VZ] = P_GetGMOFloat(MO_THING, i, MO_Z);

        spot->doomEdNum = P_GetGMOInt(MO_THING, i, MO_DOOMEDNUM);
        spot->skillModes = P_GetGMOInt(MO_THING, i, MO_SKILLMODES);
        spot->flags = P_GetGMOInt(MO_THING, i, MO_FLAGS);
        spot->angle = P_GetGMOAngle(MO_THING, i, MO_ANGLE);

#if __JHEXEN__
        spot->tid = P_GetGMOShort(MO_THING, i, MO_ID);
        spot->special = P_GetGMOByte(MO_THING, i, MO_SPECIAL);
        spot->arg1 = P_GetGMOByte(MO_THING, i, MO_ARG0);
        spot->arg2 = P_GetGMOByte(MO_THING, i, MO_ARG1);
        spot->arg3 = P_GetGMOByte(MO_THING, i, MO_ARG2);
        spot->arg4 = P_GetGMOByte(MO_THING, i, MO_ARG3);
        spot->arg5 = P_GetGMOByte(MO_THING, i, MO_ARG4);
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
            BspLeaf* bspLeaf = P_BspLeafAtPoint(spot->origin);
            xsector_t* xsector = P_ToXSector(P_GetPtrp(bspLeaf, DMU_SECTOR));

            xsector->seqType = spot->doomEdNum - 1400;
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
        case 4:
            {
#if __JHEXEN__
            uint entryPoint = spot->arg1;
#else
            uint entryPoint = 0;
#endif

            P_CreatePlayerStart(spot->doomEdNum, entryPoint, false, i);
            break;
            }

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

    if(deathmatch)
    {
        int i;
        uint numDMStarts = P_GetNumPlayerStarts(true);
        uint playerCount = 0;

        for(i = 0; i < MAXPLAYERS; ++i)
        {
            if(players[i].plr->inGame)
                playerCount++;
        }

        if(numDMStarts < playerCount)
        {
            Con_Message("P_SetupMap: Player count (%i) exceeds deathmatch spots (%i).\n", playerCount, numDMStarts);
        }
    }
}

static void spawnMapObjects(void)
{
    uint i;

    for(i = 0; i < numMapSpots; ++i)
    {
        const mapspot_t* spot = &mapSpots[i];
        mobjtype_t type;

        // Not all map spots spawn mobjs on map load.
        if(!checkMapSpotAutoSpawn(spot))
            continue;

        // A spot that should auto-spawn one (or more) mobjs.

        // Find which type to spawn.
        type = P_DoomEdNumToMobjType(spot->doomEdNum);
        if(type != MT_NONE)
        {
            mobj_t* mo;

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

/*#if _DEBUG
            Con_Message("spawning x:[%g, %g, %g] angle:%i ednum:%i flags:%i\n",
                    spot->pos[VX], spot->pos[VY], spot->pos[VZ], spot->angle,
                    spot->doomedNum, spot->flags);
#endif*/

            if((mo = P_SpawnMobj(type, spot->origin, spot->angle, spot->flags)))
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
            Con_Message("Warning: Unknown DoomEdNum %i at [%g, %g, %g].\n", spot->doomEdNum,
                        spot->origin[VX], spot->origin[VY], spot->origin[VZ]);
        }
    }

#if __JHERETIC__
    if(maceSpotCount)
    {
        // Sometimes doesn't show up if not in deathmatch.
        if(!(!deathmatch && P_Random() < 64))
        {
            const mapspot_t* spot = &mapSpots[maceSpots[P_Random() % maceSpotCount]];

            P_SpawnMobjXYZ(MT_WMACE, spot->origin[VX], spot->origin[VY], 0,
                           spot->angle, MSF_Z_FLOOR);
        }
    }
#endif

#if __JHEXEN__
    P_CreateTIDList();
#endif

    P_SpawnPlayers();
}

void P_SetupMap(Uri* mapUri, uint episode, uint map)
{
    ddstring_t* mapUriStr = mapUri? Uri_Compose(mapUri) : 0;

    if(!mapUriStr) return;

    // It begins...
    mapSetup = true;

    // The engine manages polyobjects, so reset the count.
    DD_SetInteger(DD_POLYOBJ_COUNT, 0);
    P_ResetWorldState();

    // Let the engine know that we are about to start setting up a map.
    R_SetupMap(DDSMM_INITIALIZE, 0);

    // Initialize The Logical Sound Manager.
    S_MapChange();

#if __JHERETIC__ || __JHEXEN__
    // The pointers in the body queue just became invalid.
    P_ClearBodyQueue();
#endif

    if(!P_LoadMap(Str_Text(mapUriStr)))
    {
        ddstring_t* path = Uri_ToString(mapUri);
        Con_Error("P_SetupMap: Failed loading map \"%s\".\n", Str_Text(path));
        exit(1); // Unreachable.
    }
    Str_Delete(mapUriStr);

    DD_InitThinkers();
#if __JHERETIC__
    P_InitAmbientSound();
#endif
#if __JHEXEN__
    P_InitCorpseQueue();
#endif

    initXLineDefs();
    initXSectors();
    loadMapSpots();

    spawnMapObjects();

#if __JHEXEN__
    PO_InitForMap();

    { lumpname_t mapId;
    G_MapId(episode, map, mapId);
    Con_Message("Load ACS scripts\n");
    // @todo Should be interpreted by the map converter.
    P_LoadACScripts(W_GetLumpNumForName(mapId) + 11 /*ML_BEHAVIOR*/); // ACS object code
    }
#endif

    HU_UpdatePsprites();

    // Set up world state.
    P_BuildAllTagLists();
#if __JDOOM__ || __JDOOM64__ || __JHERETIC__
    P_FindSecrets();
#endif
    P_SpawnAllSpecialThinkers();
    P_SpawnAllMaterialOriginScrollers();

#if !__JHEXEN__
    // Init extended generalized lines and sectors.
    XG_Init();
#endif

#if __JHEXEN__
    // Initialize the sky.
    P_InitSky(map);
#endif

    // Preload resources we'll likely need but which aren't present (usually) in the map.
    if(precache)
    {
#if __JDOOM__
        static const mobjtype_precachedata_t types[] = {
            { GM_ANY,   MT_SKULL },
            // Missiles:
            { GM_ANY,   MT_BRUISERSHOT },
            { GM_ANY,   MT_TROOPSHOT },
            { GM_ANY,   MT_HEADSHOT },
            { GM_ANY,   MT_ROCKET },
            { GM_ANY^GM_DOOM_SHAREWARE, MT_PLASMA },
            { GM_ANY^GM_DOOM_SHAREWARE, MT_BFG },
            { GM_DOOM2, MT_ARACHPLAZ },
            { GM_DOOM2, MT_FATSHOT },
            // Potentially dropped weapons:
            { GM_ANY,   MT_CLIP },
            { GM_ANY,   MT_SHOTGUN },
            { GM_ANY,   MT_CHAINGUN },
            // Misc effects:
            { GM_DOOM2, MT_FIRE },
            { GM_ANY,   MT_TRACER },
            { GM_ANY,   MT_SMOKE },
            { GM_DOOM2, MT_FATSHOT },
            { GM_ANY,   MT_BLOOD },
            { GM_ANY,   MT_PUFF },
            { GM_ANY,   MT_TFOG }, // Teleport FX.
            { GM_ANY,   MT_EXTRABFG },
            { GM_ANY,   MT_ROCKETPUFF },
            { 0,        0}
        };
        uint i;
#endif

        R_PrecachePSprites();

#if __JDOOM__
        for(i = 0; types[i].type != 0; ++i)
        {
            if(types[i].gameModeBits & gameModeBits)
                R_PrecacheMobjNum(types[i].type);
        }

        if(IS_NETGAME)
            R_PrecacheMobjNum(MT_IFOG);
#endif
    }

    if(IS_SERVER)
        R_SetAllDoomsdayFlags();

    P_FinalizeMap();

    // It ends.
    mapSetup = false;
}

/**
 * Called during map setup when beginning to load a new map.
 */
static void P_ResetWorldState(void)
{
#if __JHEXEN__
    static int firstFragReset = 1;
#endif
    int i, parm;

    nextMap = 0;

#if __JDOOM__ || __JDOOM64__
    wmInfo.maxFrags = 0;
    wmInfo.parTime = -1;

    // Only used with 666/7 specials
    bossKilled = false;
#endif

#if __JDOOM__
    P_BrainInitForMap();
#endif

#if __JHEXEN__
    SN_StopAllSequences();
#endif

#if __JHERETIC__
    maceSpotCount = 0;
    maceSpots = NULL;
    bossSpotCount = 0;
    bossSpots = NULL;
#endif

    P_PurgeDeferredSpawns();

#if !__JHEXEN__
    totalKills = totalItems = totalSecret = 0;
#endif

    timerGame = 0;
    if(deathmatch)
    {
        parm = CommandLine_Check("-timer");
        if(parm && parm < CommandLine_Count() - 1)
        {
            timerGame = atoi(CommandLine_At(parm + 1)) * 35 * 60;
        }
    }

    for(i = 0; i < MAXPLAYERS; ++i)
    {
        player_t* plr = &players[i];
        ddplayer_t* ddplr = plr->plr;

        ddplr->mo = NULL;
        plr->killCount = plr->secretCount = plr->itemCount = 0;

        if(ddplr->inGame && plr->playerState == PST_DEAD)
            plr->playerState = PST_REBORN;

#if __JHEXEN__
        if(!IS_NETGAME || (IS_NETGAME != 0 && deathmatch != 0) || firstFragReset == 1)
        {
            memset(plr->frags, 0, sizeof(plr->frags));
            firstFragReset = 0;
        }
#else
        memset(plr->frags, 0, sizeof(plr->frags));
#endif

        G_ResetLookOffset(i);
    }

#if __JDOOM__ || __JDOOM64__
    bodyQueueSlot = 0;
#endif

    P_DestroyPlayerStarts();
}

/**
 * Do any map finalization including any game-specific stuff.
 */
static void P_FinalizeMap(void)
{
#if __JDOOM__
    // Adjust slime lower wall textures (a hack!).
    // This will hide the ugly green bright line that would otherwise be
    // visible due to texture repeating and interpolation.
    if(!(gameModeBits & (GM_DOOM2_HACX|GM_DOOM_CHEX)))
    {
        uint i, k;
        material_t* mat = P_ToPtr(DMU_MATERIAL, Materials_ResolveUriCString(MN_TEXTURES_NAME":NUKE24"));
        material_t* bottomMat, *midMat;
        float yoff;
        SideDef* sidedef;
        LineDef* line;

        for(i = 0; i < numlines; ++i)
        {
            line = P_ToPtr(DMU_LINEDEF, i);

            for(k = 0; k < 2; ++k)
            {
                sidedef = P_GetPtrp(line, k == 0? DMU_SIDEDEF0 : DMU_SIDEDEF1);
                if(sidedef)
                {
                    bottomMat = P_GetPtrp(sidedef, DMU_BOTTOM_MATERIAL);
                    midMat = P_GetPtrp(sidedef, DMU_MIDDLE_MATERIAL);

                    if(bottomMat == mat && midMat == NULL)
                    {
                        yoff = P_GetFloatp(sidedef, DMU_BOTTOM_MATERIAL_OFFSET_Y);
                        P_SetFloatp(sidedef, DMU_BOTTOM_MATERIAL_OFFSET_Y, yoff + 1.0f);
                    }
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
#endif

    // Someone may want to do something special now that the map has been fully set up.
    R_SetupMap(DDSMM_FINALIZE, 0);
}

const char* P_GetMapNiceName(void)
{
    const char* lname, *ptr;

    lname = (char*) DD_GetVariable(DD_MAP_NAME);
#if __JHEXEN__
    // In jHexen we can look in the MAPINFO for the map name.
    if(!lname)
        lname = P_GetMapName(gameMap);
#endif

    if(!lname || !lname[0])
        return NULL;

    // Skip the ExMx part.
    ptr = strchr(lname, ':');
    if(ptr)
    {
        lname = ptr + 1;
        while(*lname && isspace(*lname))
            lname++;
    }

    return lname;
}

patchid_t P_FindMapTitlePatch(uint episode, uint map)
{
#if __JDOOM__ || __JDOOM64__
#  if __JDOOM__
    if(!(gameModeBits & (GM_ANY_DOOM2|GM_DOOM_CHEX)))
        map = (episode * 9) + map;
#  endif
    if(map < pMapNamesSize)
        return pMapNames[map];
#endif
    return 0;
}

const char* P_GetMapAuthor(boolean supressGameAuthor)
{
    const char* author = (const char*) DD_GetVariable(DD_MAP_AUTHOR);
    boolean mapIsCustom;
    GameInfo gameInfo;
    ddstring_t* path;
    Uri* uri;

    if(!author || !author[0]) return NULL;

    uri = G_ComposeMapUri(gameEpisode, gameMap);
    path = Uri_Resolved(uri);

    mapIsCustom = P_MapIsCustom(Str_Text(path));

    Str_Delete(path);
    Uri_Delete(uri);

    DD_GameInfo(&gameInfo);
    if((mapIsCustom || supressGameAuthor) && !stricmp(gameInfo.author, author))
        return NULL;
    return author;
}

#if __JDOOM__ || __JDOOM64__ || __JHERETIC__
void P_FindSecrets(void)
{
    uint i;

    totalSecret = 0;

    // Find secret sectors.
    for(i = 0; i < numsectors; ++i)
    {
        Sector* sec = P_ToPtr(DMU_SECTOR, i);
        xsector_t* xsec = P_ToXSector(sec);

        if(xsec->special != 9) continue;

        totalSecret++;
    }

#if __JDOOM64__
    // Find secret lines.
    for(i = 0; i < numlines; ++i)
    {
        LineDef* line  = P_ToPtr(DMU_LINEDEF, i);
        xline_t* xline = P_ToXLine(line);

        if(xline->special != 994) continue;

        totalSecret++;
    }
#endif
}
#endif

void P_SpawnSectorMaterialOriginScrollers(void)
{
    uint i;

    // Clients do not spawn material origin scrollers on their own.
    if(IS_CLIENT) return;

    for(i = 0; i < numsectors; ++i)
    {
        Sector* sec     = P_ToPtr(DMU_SECTOR, i);
        xsector_t* xsec = P_ToXSector(sec);

        if(!xsec->special) continue;

        // A scroller?
        P_SpawnSectorMaterialOriginScroller(sec, PLN_FLOOR, xsec->special);
    }
}

void P_SpawnSideMaterialOriginScrollers(void)
{
    uint i;

    // Clients do not spawn material origin scrollers on their own.
    if(IS_CLIENT) return;

    for(i = 0; i < numlines; ++i)
    {
        LineDef* line  = P_ToPtr(DMU_LINEDEF, i);
        xline_t* xline = P_ToXLine(line);
        SideDef* frontSide;

        if(!xline->special) continue;

        frontSide = P_GetPtrp(line, DMU_SIDEDEF0);
        P_SpawnSideMaterialOriginScroller(frontSide, xline->special);
    }
}

void P_SpawnAllMaterialOriginScrollers(void)
{
    P_SpawnSideMaterialOriginScrollers();
    P_SpawnSectorMaterialOriginScrollers();
}
