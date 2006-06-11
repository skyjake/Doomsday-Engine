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
 * p_setup.c: Common map setup routines.
 * Management of extended map data objects (eg xlines) is done here
 */

// HEADER FILES ------------------------------------------------------------

#if __JDOOM__
#  include "jDoom/doomdef.h"
#  include "jDoom/doomstat.h"
#  include "jDoom/d_config.h"
#  include "jDoom/p_local.h"
#  include "jDoom/s_sound.h"
#  include "jDoom/p_setup.h"
#  include "hu_stuff.h"
#elif __JHERETIC__
#  include <math.h>
#  include <ctype.h>              // has isspace
#  include "jHeretic/Doomdef.h"
#  include "jHeretic/h_stat.h"
#  include "jHeretic/P_local.h"
#  include "jHeretic/Soundst.h"
#  include "jHeretic/p_setup.h"
#elif __JHEXEN__
#  include <math.h>
#  include "h2def.h"
#  include "jHexen/p_local.h"
#  include "jHexen/r_local.h"
#  include "jHexen/p_setup.h"
#  include "Common/p_start.h"
#elif __JSTRIFE__
#  include "h2def.h"
#  include "jStrife/p_local.h"
#endif

#include "Common/dmu_lib.h"
#include "r_common.h"
#include "p_mapsetup.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

void    InitMapInfo(void);
void    P_SpawnThings(void);
void    AM_LevelInit(void);

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

int numthings;

// Our private map data structures
xsector_t *xsectors;
xline_t   *xlines;

// If true we are in the process of setting up a level
boolean levelSetup;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static int oldNumLines;
static int oldNumSectors;

// CODE --------------------------------------------------------------------

/*
 * Converts a line to an xline.
 */
xline_t* P_XLine(line_t* line)
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

/*
 * Converts a sector to an xsector.
 */
xsector_t* P_XSector(sector_t* sector)
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

/*
 * Given a subsector - find its parent xsector.
 */
xsector_t* P_XSectorOfSubsector(subsector_t* sub)
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

/*
 * Doomsday calls this (before any data is read) for each type of map object
 * at the start of the level load process. This is to allow us (the game) to do
 * any initialization we need. For example if we maintain our own data for lines
 * (the xlines) we'll do all allocation and init here.
 *
 * @param   type        (DAM object type) The id of the data type being setup.
 * @param   num         The number of elements of "type" Doomsday is creating.
 */
void P_SetupForMapData(int type, int num)
{
    switch(type)
    {
    case DAM_SECTOR:
        {
        int newnum = oldNumSectors + num;

        if(oldNumSectors > 0)
            xsectors = Z_Realloc(xsectors, newnum * sizeof(xsector_t), PU_LEVEL);
        else
            xsectors = Z_Malloc(newnum * sizeof(xsector_t), PU_LEVEL, 0);

        memset(xsectors + oldNumSectors, 0, num * sizeof(xsector_t));
        oldNumSectors = newnum;
        }
        break;

    case DAM_LINE:
        {
        int newnum = oldNumLines + num;

        if(oldNumLines > 0)
            xlines = Z_Realloc(xlines, newnum * sizeof(xline_t), PU_LEVEL);
        else
            xlines = Z_Malloc(newnum * sizeof(xline_t), PU_LEVEL, 0);

        memset(xlines + oldNumLines, 0, num * sizeof(xline_t));
        oldNumLines = newnum;
        }
        break;

    case DAM_THING:
        {
        int oldNum = numthings;

        numthings += num;

        if(oldNum > 0)
            things = Z_Realloc(things, numthings * sizeof(thing_t), PU_LEVEL);
        else
            things = Z_Malloc(numthings * sizeof(thing_t), PU_LEVEL, 0);

        memset(things + oldNum, 0, num * sizeof(thing_t));
        }
        break;

    default:
        break;
    }
}

/*
 * Loads map and glnode data for the requested episode and map
 */
void P_SetupLevel(int episode, int map, int playermask, skill_t skill)
{
    int     i, flags;
    char    levelId[9];

    // It begins
    levelSetup = true;

    // Reset our local counters for xobjects
    // Used to allow objects to be allocated in a non-continous order
    oldNumLines = 0;
    oldNumSectors = 0;

    // Map thing data for the level IS stored game-side.
    // However, Doomsday tells us how many things there are.
    numthings = 0;

    // The engine manages polyobjects, so reset the count.
    DD_SetInteger(DD_POLYOBJ_COUNT, 0);

    P_ResetWorldState();

    // Let the engine know that we are about to start setting up a
    // level.
    R_SetupLevel(NULL, DDSLF_INITIALIZE);

    // Initialize The Logical Sound Manager.
    S_LevelChange();

#if __JHEXEN__
    S_StartMusic("chess", true);    // Waiting-for-level-load song
#endif

    Z_FreeTags(PU_LEVEL, PU_PURGELEVEL - 1);

    P_InitThinkers();

    P_GetMapLumpName(episode, map, levelId);

    if(!P_LoadMap(levelId))
    {
        Con_Error("P_SetupLevel: Failed loading map \"%s\".\n",levelId);
    }

    // Now the map data has been loaded we can update the
    // global data struct counters
    numthings = DD_GetInteger(DD_THING_COUNT);

    // First job is to zero unused flags if MF_INVALID is set.
    //
    // "This has been found to be necessary because of errors
    //  in Ultimate DOOM's E2M7, where around 1000 linedefs have
    //  the value 0xFE00 masked into the flags value.
    //  There could potentially be many more maps with this problem,
    //  as it is well-known that Hellmaker wads set all bits in
    //  mapthings that it does not understand."
    //      Thanks to Quasar for the heads up.
#if !__JHEXEN__
    // FIXME: This applies only to DOOM format maps but the game doesn't
    // know what format the map is in (and shouldn't really) but the
    // engine doesn't know if the game wants to do this...
    for(i = 0; i < numlines; ++i)
    {
        flags = P_GetInt(DMU_LINE, i, DMU_FLAGS);
        if(flags & ML_INVALID)
        {
            flags &= VALIDMASK;
            P_SetInt(DMU_LINE, i, DMU_FLAGS, flags);
        }
    }
#endif

#if __JHERETIC__
    P_InitAmbientSound();
    P_InitMonsters();
    P_OpenWeapons();
#endif
    P_SpawnThings();

    // killough 3/26/98: Spawn icon landings:
#if __JDOOM__
    if(gamemode == commercial)
        P_SpawnBrainTargets();
#endif

#if __JHERETIC__
    P_CloseWeapons();
#endif

    // DJS - TODO:
    // This needs to be sorted out. R_SetupLevel should be called from the
    // engine but in order to move it there we need to decide how polyobject
    // init/setup is going to be handled.
#if __JHEXEN__
    // Initialize polyobjs.
    Con_Message("Polyobject init\n");
    PO_Init(W_GetNumForName(levelId) + ML_THINGS);   // Initialize the polyobjs

    // Now we can init the server.
    Con_Message("Init server\n");
    R_SetupLevel(levelId, DDSLF_SERVER_ONLY);

    Con_Message("Load ACS scripts\n");
    P_LoadACScripts(W_GetNumForName(levelId) + ML_BEHAVIOR); // ACS object code
#else
    // Now we can init the server.
    Con_Message("Init server\n");
    R_SetupLevel(levelId, DDSLF_SERVER_ONLY);
#endif
    Con_Message("Deal starts\n");
    P_DealPlayerStarts();
    Con_Message("Spawn players\n");
    P_SpawnPlayers();
    Con_Message("Done\n");

    // set up world state
    P_SpawnSpecials();

    // preload graphics
    if(precache)
    {
        R_PrecacheLevel();
        R_PrecachePSprites();
    }

    S_LevelMusic();

    P_FinalizeLevel();

    // Someone may want to do something special now that
    // the level has been fully set up.
    R_SetupLevel(levelId, DDSLF_FINALIZE);

    P_PrintMapBanner(episode, map);

    // It ends
    levelSetup = false;
}

/*
 * Called during level setup when beginning to load a new map.
 */
static void P_ResetWorldState(void)
{
    int i;
    int     parm;

#if __JDOOM__
    wminfo.maxfrags = 0;
    wminfo.partime = 180;

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
    players[consoleplayer].plr->viewz = 1;

    for(i = 0; i < MAXPLAYERS; i++)
    {
        players[i].killcount = players[i].secretcount = players[i].itemcount = 0;
    }

    bodyqueslot = 0;

    P_FreePlayerStarts();

    leveltime = actual_leveltime = 0;
}

/*
 * Do any level finalization including any game-specific stuff.
 */
static void P_FinalizeLevel(void)
{
    AM_LevelInit();

#if __JDOOM__
    // Adjust slime lower wall textures (a hack!).
    // This will hide the ugly green bright line that would otherwise be
    // visible due to texture repeating and interpolation.
    {
    int i, k;
    int     lumpnum = R_TextureNumForName("NUKE24");
    int     bottomTex;
    int     midTex;
    fixed_t yoff;
    side_t* side;
    line_t* line;

    for(i = 0; i < numlines; i++)
    {
        line = P_ToPtr(DMU_LINE, i);
        for(k = 0; k < 2; k++)
        {
            if(P_GetPtrp(line, k == 0? DMU_FRONT_SECTOR : DMU_BACK_SECTOR))
            {
                side = P_GetPtrp(line, k == 0? DMU_SIDE0 : DMU_SIDE1);
                yoff = P_GetFixedp(side, DMU_TEXTURE_OFFSET_Y);
                bottomTex = P_GetIntp(side, DMU_BOTTOM_TEXTURE);
                midTex = P_GetIntp(side, DMU_MIDDLE_TEXTURE);

                if(bottomTex == lumpnum && midTex == 0)
                    P_SetFixedp(side, DMU_TEXTURE_OFFSET_Y, yoff + FRACUNIT);
            }
        }
    }
    }

#elif __JHERETIC__
    // Do some fine tuning with mobj placement and orientation.
    P_MoveThingsOutOfWalls();
    P_TurnGizmosAwayFromDoors();

#elif __JHEXEN__
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

    P_TurnTorchesToFaceWalls();

    // Check if the level is a lightning level.
    P_InitLightning();

    SN_StopAllSequences();
    }
#endif
}

/*
 * Prints a banner to the console containing information
 * pertinent to the current map (eg map name, author...).
 */
static void P_PrintMapBanner(int episode, int map)
{
#if !__JHEXEN__
    char    *lname, *lauthor;

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

    if(lauthor)
        Con_FPrintf(CBLF_LIGHT | CBLF_BLUE, "Author: %s\n", lauthor);
    Con_Printf("\n");
}
