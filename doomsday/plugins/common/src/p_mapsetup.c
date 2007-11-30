/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2007 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2007 Daniel Swanson <danij@dengine.net>
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
 * p_setup.c: Common map setup routines.
 *
 * Management of extended map data objects (eg xlines) is done here
 */

// HEADER FILES ------------------------------------------------------------

#include <math.h>
#include <ctype.h>  // has isspace

#if  __DOOM64TC__
#  include "doom64tc.h"
#elif __WOLFTC__
#  include "wolftc.h"
#elif __JDOOM__
#  include "jdoom.h"
#elif __JHERETIC__
#  include "jheretic.h"
#  include "hu_stuff.h"
#elif __JHEXEN__
#  include "jhexen.h"
#  include "p_start.h"
#elif __JSTRIFE__
#  include "jstrife.h"
#endif

#include "dmu_lib.h"
#include "r_common.h"
#include "p_mapsetup.h"
#include "am_map.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

void    P_SpawnThings(void);

#if __JHERETIC__
void P_TurnGizmosAwayFromDoors();
void P_MoveThingsOutOfWalls();
#elif __JHEXEN__
void P_TurnTorchesToFaceWalls();
#endif

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void P_ResetWorldState(void);
static void P_FinalizeLevel(void);
static void P_PrintMapBanner(int episode, int map);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

#if __JDOOM__
extern boolean bossKilled;
#endif

extern int actual_leveltime;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// Our private map data structures
xsector_t *xsectors;
xline_t   *xlines;

// If true we are in the process of setting up a level
boolean levelSetup;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

/**
 * Converts a line to an xline.
 */
xline_t* P_ToXLine(line_t *line)
{
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

/**
 * Converts a sector to an xsector.
 */
xsector_t* P_ToXSector(sector_t *sector)
{
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

/**
 * Given a subsector - find its parent xsector.
 */
xsector_t* P_ToXSectorOfSubsector(subsector_t *sub)
{
    sector_t* sec = P_GetPtrp(sub, DMU_SECTOR);

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

/**
 * Given the index of an xline, return it.
 *
 * \note: This routine cannot be used with dummy lines!
 *
 * @param               Index of the xline to return.
 *
 * @return              Ptr to xline_t.
 */
xline_t* P_GetXLine(uint index)
{
    if(index >= numlines)
        return NULL;

    return &xlines[index];
}

/**
 * Given the index of an xsector, return it.
 *
 * \note: This routine cannot be used with dummy sectors!
 *
 * @param               Index of the xsector to return.
 *
 * @return              Ptr to xsector_t.
 */
xsector_t* P_GetXSector(uint index)
{
    if(index >= numsectors)
        return NULL;

    return &xsectors[index];
}

/**
 * Doomsday calls this (before any data is read) for each type of map object
 * at the start of the level load process. This is to allow us (the game) to do
 * any initialization we need. For example if we maintain our own data for lines
 * (the xlines) we'll do all allocation and init here.
 *
 * @param   type        (DAM object type) The id of the data type being setup.
 * @param   num         The number of elements of "type" Doomsday is creating.
 */
void P_SetupForMapData(int type, uint num)
{
    switch(type)
    {
    case DAM_SECTOR:
        xsectors = Z_Calloc(num * sizeof(xsector_t), PU_LEVEL, 0);
        break;

    case DAM_LINE:
        xlines = Z_Calloc(num * sizeof(xline_t), PU_LEVEL, 0);
        break;

    case DAM_THING:
        things = Z_Calloc(num * sizeof(spawnspot_t), PU_LEVEL, 0);
        break;

    default:
        break;
    }
}

typedef struct setuplevelparam_s {
    int episode;
    int map;
    int playerMask; // Unused?
    skillmode_t skill;
} setuplevelparam_t;

int P_SetupLevelWorker(void *ptr)
{
    setuplevelparam_t *param = ptr;

#if !__DOOM64TC__
# if __JDOOM__ || __JHERETIC__
    uint        i;
    int         flags;
# endif
#endif
    char        levelId[9];

    // It begins...
    levelSetup = true;

    // The engine manages polyobjects, so reset the count.
    DD_SetInteger(DD_POLYOBJ_COUNT, 0);
    P_ResetWorldState();

    // Let the engine know that we are about to start setting up a
    // level.
    R_SetupLevel(DDSLM_INITIALIZE, 0);

    // Initialize The Logical Sound Manager.
    S_LevelChange();

#if __JHEXEN__
    S_StartMusic("chess", true); // Waiting-for-level-load song
#endif

    Z_FreeTags(PU_LEVEL, PU_PURGELEVEL - 1);
    P_InitThinkers();

    P_GetMapLumpName(param->episode, param->map, levelId);
    if(!P_LoadMap(levelId))
    {
        Con_Error("P_SetupLevel: Failed loading map \"%s\".\n",levelId);
    }

    P_SpawnThings();

    /**
     * First job is to zero unused flags if MF_INVALID is set.
     *
     * \attention "This has been found to be necessary because of errors
     *  in Ultimate DOOM's E2M7, where around 1000 linedefs have
     *  the value 0xFE00 masked into the flags value.
     *  There could potentially be many more maps with this problem,
     *  as it is well-known that Hellmaker wads set all bits in
     *  mapthings that it does not understand."
     *  Thanks to Quasar for the heads up.
     */
#if !__JHEXEN__
    /**
     * \fixme This applies only to DOOM format maps but the game doesn't
     * know what format the map is in (and shouldn't really) but the
     * engine doesn't know if the game wants to do this...
     */
# if !__DOOM64TC__
    /**
     * \attention DJS - Can't do this with Doom64TC as it has used the ML_INVALID bit
     * for another purpose... doh!
     */
    for(i = 0; i < numlines; ++i)
    {
        flags = P_GetInt(DMU_LINE, i, DMU_FLAGS);
        if(flags & ML_INVALID)
        {
            flags &= VALIDMASK;
            P_SetInt(DMU_LINE, i, DMU_FLAGS, flags);
        }
    }
# endif
#endif

#if __JHERETIC__
    P_InitAmbientSound();
#endif

#if __JHEXEN__
    Con_Message("Load ACS scripts\n");
    // \fixme Custom map data format support
    P_LoadACScripts(W_GetNumForName(levelId) + 11 /*ML_BEHAVIOR*/); // ACS object code
#endif

    P_DealPlayerStarts(0);
    P_SpawnPlayers();

    // Set up world state.
    P_SpawnSpecials();

    // Preload graphics.
    if(precache)
    {
        R_PrecacheLevel();
        R_PrecachePSprites();
    }

    S_LevelMusic();
    P_FinalizeLevel();

    // Someone may want to do something special now that the level has been
    // fully set up.
    R_SetupLevel(DDSLM_FINALIZE, 0);

    P_PrintMapBanner(param->episode, param->map);

    // It ends.
    levelSetup = false;

    Con_BusyWorkerEnd();
    return 0;
}

/**
 * Loads map and glnode data for the requested episode and map.
 */
void P_SetupLevel(int episode, int map, int playerMask, skillmode_t skill)
{
    setuplevelparam_t param;

    param.episode = episode;
    param.map = map;
    param.playerMask = playerMask; // Unused?
    param.skill = skill;

    DD_Executef(true, "texreset raw"); // Delete raw images to save memory.

    // \todo Use progress bar mode and update progress during the setup.
    Con_Busy(BUSYF_ACTIVITY | /*BUSYF_PROGRESS_BAR |*/ (verbose? BUSYF_CONSOLE_OUTPUT : 0),
             P_SetupLevelWorker, &param);

    R_SetupLevel(DDSLM_AFTER_BUSY, 0);

#if __JHEXEN__
    {
    int i;
    // Load colormap and set the fullbright flag
    i = P_GetMapFadeTable(gamemap);
    if(i == W_GetNumForName("COLORMAP"))
    {
        // We don't want fog in this case.
        GL_UseFog(false);
    }
    else
    {
        // Probably fog ... don't use fullbright sprites
        if(i == W_GetNumForName("FOGMAP"))
        {
            // Tell the renderer to turn on the fog.
            GL_UseFog(true);
        }
    }
    }
#endif
}

/**
 * Called during level setup when beginning to load a new map.
 */
static void P_ResetWorldState(void)
{
    int         i;
    int         parm;

#if __JDOOM__
    wminfo.maxfrags = 0;
    wminfo.partime = -1;

    // Only used with 666/7 specials
    bossKilled = false;

    // Brain info
    numbraintargets = 0;
    numbraintargets_alloc = -1;
    brain.targeton = 0;
    brain.easy = 0;           // killough 3/26/98: always init easy to 0
#endif

    // clear special respawning que
#if __JDOOM__ || __JHERETIC__
    iquehead = iquetail = 0;
#endif

#if !__JHEXEN__
    totalkills = totalitems = totalsecret = 0;
#endif

    TimerGame = 0;
    if(deathmatch)
    {
        parm = ArgCheck("-timer");
        if(parm && parm < Argc() - 1)
        {
            TimerGame = atoi(Argv(parm + 1)) * 35 * 60;
        }
    }

    // Initial height of PointOfView; will be set by player think.
    players[consoleplayer].plr->viewZ = 1;

    for(i = 0; i < MAXPLAYERS; ++i)
    {
        players[i].killcount =
            players[i].secretcount = players[i].itemcount = 0;
    }

    bodyqueslot = 0;

    P_FreePlayerStarts();

    leveltime = actual_leveltime = 0;
}

/**
 * Do any level finalization including any game-specific stuff.
 */
static void P_FinalizeLevel(void)
{
    AM_InitForLevel();

#if __JDOOM__
    // Adjust slime lower wall textures (a hack!).
    // This will hide the ugly green bright line that would otherwise be
    // visible due to texture repeating and interpolation.
    {
    uint    i, k;
    int     materialID = R_MaterialNumForName("NUKE24", MAT_TEXTURE);
    int     bottomTex;
    int     midTex;
    float   yoff;
    side_t* side;
    line_t* line;

    for(i = 0; i < numlines; ++i)
    {
        line = P_ToPtr(DMU_LINE, i);
        for(k = 0; k < 2; k++)
        {
            if(P_GetPtrp(line, k == 0? DMU_FRONT_SECTOR : DMU_BACK_SECTOR))
            {
                side = P_GetPtrp(line, k == 0? DMU_SIDE0 : DMU_SIDE1);
                yoff = P_GetFloatp(side, DMU_BOTTOM_MATERIAL_OFFSET_Y);
                bottomTex = P_GetIntp(side, DMU_BOTTOM_MATERIAL);
                midTex = P_GetIntp(side, DMU_MIDDLE_MATERIAL);

                if(bottomTex == materialID && midTex == 0)
                    P_SetFloatp(side, DMU_BOTTOM_MATERIAL_OFFSET_Y, yoff + 1.0f);
            }
        }
#if __DOOM64TC__
        // kaiser - convert the unused xg linetypes to the new line type.
        // DJS - Mega kludge. Update the wad instead!
        switch(P_ToXLine(line)->special)
        {
        case 2011:
            P_ToXLine(line)->special = 415;
            break;

        case 2012:
            P_ToXLine(line)->special = 0;
            break;

        case 2013:
            P_ToXLine(line)->special = 0;
            break;

        case 432:
            P_SetSectorColor(line);
            break;

        default:
            break;
        }
#endif
    }
    }

#elif __JHERETIC__
    // Do some fine tuning with mobj placement and orientation.
    P_MoveThingsOutOfWalls();
    P_TurnGizmosAwayFromDoors();

#elif __JHEXEN__
    P_TurnTorchesToFaceWalls();

    // Check if the level should have lightening.
    P_InitLightning();

    SN_StopAllSequences();
#endif
}

char *P_GetMapNiceName(void)
{
    char       *lname, *ptr;

    lname = (char *) DD_GetVariable(DD_MAP_NAME);

    if(lname)
    {
        // Skip the ExMx part.
        ptr = strchr(lname, ':');
        if(ptr)
        {
            lname = ptr + 1;
            while(*lname && isspace(*lname))
                lname++;
        }
    }
#if __JHEXEN__
    // In jHexen we can look in the MAPINFO for the map name.
    if(!lname)
        lname = P_GetMapName(gamemap);
#endif

    return lname;
}

/**
 * Prints a banner to the console containing information pertinent to the
 * current map (e.g. map name, author...).
 */
static void P_PrintMapBanner(int episode, int map)
{
#if !__JHEXEN__
    char       *lname, *lauthor;

    // Retrieve the name and author strings from the engine.
    lname = (char *) DD_GetVariable(DD_MAP_NAME);
    lauthor = (char *) DD_GetVariable(DD_MAP_AUTHOR);
#else
    char lname[64];
    boolean lauthor = false;

    sprintf(lname, "Map %d (%d): %s", P_GetMapWarpTrans(map), map,
            P_GetMapName(map));
#endif

    Con_Printf("\n");
    if(lname)
        Con_FPrintf(CBLF_LIGHT | CBLF_BLUE, "%s\n", lname);

#if !__JHEXEN__
    if(lauthor)
        Con_FPrintf(CBLF_LIGHT | CBLF_BLUE, "Author: %s\n", lauthor);
#endif
    Con_Printf("\n");
}
