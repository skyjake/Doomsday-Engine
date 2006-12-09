/**\file
 *\section Copyright and License Summary
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2006 Jaakko Keränen <skyjake@dengine.net>
 *\author Copyright © 2006 Daniel Swanson <danij@dengine.net>
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

/*
 * p_data.c: Playsim Data Structures, Macros and Constants
 *
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_console.h"
#include "de_play.h"
#include "de_refresh.h"
#include "de_system.h"
#include "de_misc.h"

#include "rend_bias.h"
#include "m_bams.h"

#include <math.h>
#include <stddef.h>

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// Lump order in a map WAD: each map needs a couple of lumps
// to provide a complete scene geometry description.
enum {
    ML_LABEL,                      // A separator, name, ExMx or MAPxx
    ML_THINGS,                     // Monsters, items..
    ML_LINEDEFS,                   // LineDefs, from editing
    ML_SIDEDEFS,                   // SideDefs, from editing
    ML_VERTEXES,                   // Vertices, edited and BSP splits generated
    ML_SEGS,                       // LineSegs, from LineDefs split by BSP
    ML_SSECTORS,                   // SubSectors, list of LineSegs
    ML_NODES,                      // BSP nodes
    ML_SECTORS,                    // Sectors, from editing
    ML_REJECT,                     // LUT, sector-sector visibility
    ML_BLOCKMAP,                   // LUT, motion clipping, walls/grid element
    ML_BEHAVIOR                    // ACS Scripts (not supported currently)
};

// Bad texture record
typedef struct {
    char *name;
    boolean planeTex;
    uint count;
} badtex_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void P_FreeBadTexList(void);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern boolean levelSetup;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

/*
 * These map data arrays are internal to the engine.
 */

uint        numvertexes;
vertex_t   *vertexes;

uint        numsegs;
seg_t      *segs;

uint        numsectors;
sector_t   *sectors;

uint        numsubsectors;
subsector_t *subsectors;

uint        numnodes;
node_t     *nodes;

uint        numlines;
line_t     *lines;

uint        numsides;
side_t     *sides;

// Should we generate new blockmap data if its invalid?
// 0: error out
// 1: generate new
// 2: Always generate new
int         createBMap = 1;

// Should we generate new reject data if its invalid?
// 0: error out
// 1: generate new
// 2: Always generate new
int         createReject = 1;

// mapthings are actually stored & handled game-side
int         numthings;

long       *blockmaplump;           // offsets in blockmap are from here
long       *blockmap;

int         bmapwidth, bmapheight;  // in mapblocks
fixed_t     bmaporgx, bmaporgy;     // origin of block map
linkmobj_t *blockrings;             // for thing rings

byte       *rejectmatrix;           // for fast sight rejection

polyblock_t **polyblockmap;         // polyobj blockmap

nodepile_t  thingnodes, linenodes;  // all kinds of wacky links

ded_mapinfo_t *mapinfo = 0;         // Current mapinfo.
fixed_t     mapgravity;             // Gravity for the current map.
int         mapambient;             // Ambient lightlevel for the current map.

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// for sector->linecount
uint numUniqueLines;

// The following is used in error fixing/detection/reporting:
// missing sidedefs
uint numMissingFronts = 0;
uint *missingFronts = NULL;

// bad texture list
static uint numBadTexNames = 0;
static uint maxBadTexNames = 0;
static badtex_t *badTexNames = NULL;

// CODE --------------------------------------------------------------------

/**
 * Convert propertyType enum constant into a string for error/debug messages.
 */
const char* value_Str(int val)
{
    static char valStr[40];
    struct val_s {
        int val;
        const char* str;
    } valuetypes[] =
    {
        { DDVT_BOOL, "DDVT_BOOL" },
        { DDVT_BYTE, "DDVT_BYTE" },
        { DDVT_SHORT, "DDVT_SHORT" },
        { DDVT_INT, "DDVT_INT" },
        { DDVT_FIXED, "DDVT_FIXED" },
        { DDVT_ANGLE, "DDVT_ANGLE" },
        { DDVT_FLOAT, "DDVT_FLOAT" },
        { DDVT_ULONG, "DDVT_ULONG" },
        { DDVT_PTR, "DDVT_PTR" },
        { DDVT_FLAT_INDEX, "DDVT_FLAT_INDEX" },
        { DDVT_BLENDMODE, "DDVT_BLENDMODE" },
        { DDVT_VERT_PTR, "DDVT_VERT_PTR" },
        { DDVT_LINE_PTR, "DDVT_LINE_PTR" },
        { DDVT_SIDE_PTR, "DDVT_SIDE_PTR" },
        { DDVT_SECT_PTR, "DDVT_SECT_PTR" },
        { DDVT_SEG_PTR, "DDVT_SEG_PTR" },
        { 0, NULL }
    };
    uint        i;

    for(i = 0; valuetypes[i].str; ++i)
        if(valuetypes[i].val == val)
            return valuetypes[i].str;

    sprintf(valStr, "(unnamed %i)", val);
    return valStr;
}

void P_InitData(void)
{
    P_InitMapDataFormats();
    P_InitMapUpdate();
}

/**
 * TODO: Consolidate with R_UpdatePlanes?
 */
void P_PlaneChanged(sector_t *sector, uint plane)
{
    uint        i, k;
    subsector_t *sub;
    seg_t      *seg;
    side_t     *front = NULL, *back = NULL;

    // Update the z positions of the degenmobjs for this sector.
    sector->planes[plane]->soundorg.pos[VZ] = sector->planes[plane]->height;

    if(plane == PLN_FLOOR || plane == PLN_CEILING)
        sector->soundorg.pos[VZ] =
            (sector->SP_ceilheight - sector->SP_floorheight) / 2;

    for(i = 0; i < sector->linecount; ++i)
    {
        front = sector->Lines[i]->sides[0];
        back  = sector->Lines[i]->sides[1];

        if(!front || !front->sector ||
           front->sector->planes[plane]->linked ||
           !back || !back->sector ||
           back->sector->planes[plane]->linked)
            continue;

        // Do as in the original Doom if the texture has not been defined -
        // extend the floor/ceiling to fill the space (unless its the skyflat),
        // or if there is a midtexture use that instead.
        if(plane == PLN_FLOOR)
        {
            // Check for missing lowers.
            if(front->sector->SP_floorheight < back->sector->SP_floorheight &&
               front->SW_bottompic == 0)
            {
                if(!R_IsSkySurface(&front->sector->SP_floorsurface))
                {
                    front->SW_bottomflags |= SUF_TEXFIX;

                    front->SW_bottompic =
                        front->sector->SP_floorpic;

                    front->SW_bottomisflat =
                        front->sector->SP_floorisflat;
                }

                if(back->SW_bottomflags & SUF_TEXFIX)
                {
                    back->SW_bottomflags &= ~SUF_TEXFIX;
                    back->SW_bottompic = 0;
                }
            }
            else if(front->sector->SP_floorheight > back->sector->SP_floorheight &&
               back->SW_bottompic == 0)
            {
                if(!R_IsSkySurface(&back->sector->SP_floorsurface))
                {
                    back->SW_bottomflags |= SUF_TEXFIX;

                    back->SW_bottompic =
                        back->sector->SP_floorpic;

                    back->SW_bottomisflat =
                        back->sector->SP_floorisflat;
                }

                if(front->SW_bottomflags & SUF_TEXFIX)
                {
                    front->SW_bottomflags &= ~SUF_TEXFIX;
                    front->SW_bottompic = 0;
                }
            }
        }
        else
        {
            // Check for missing uppers.
            if(front->sector->SP_ceilheight > back->sector->SP_ceilheight &&
               front->SW_toppic == 0)
            {
                if(!R_IsSkySurface(&front->sector->SP_ceilsurface))
                {
                    front->SW_topflags |= SUF_TEXFIX;
                    // Preference a middle texture when doing replacements as
                    // this could be a mid tex door hack.
                   /* if(front->SW_middlepic)
                    {
                        front->flags |= SDF_MIDTEXUPPER;
                        front->SW_toppic = front->SW_middlepic;
                        front->SW_topisflat = front->SW_middleisflat;
                    }
                    else*/
                    {
                        front->SW_toppic =
                            front->sector->SP_ceilpic;

                        front->SW_topisflat =
                            front->sector->SP_ceilisflat;
                    }
                }

                if(back->SW_topflags & SUF_TEXFIX)
                {
                    back->SW_topflags &= ~SUF_TEXFIX;
                    back->SW_toppic = 0;
                }
            }
            else if(front->sector->SP_ceilheight < back->sector->SP_ceilheight &&
               back->SW_toppic == 0)
            {
                if(!R_IsSkySurface(&back->sector->SP_ceilsurface))
                {
                    back->SW_topflags |= SUF_TEXFIX;
                    // Preference a middle texture when doing replacements as
                    // this could be a mid tex door hack.
                   /* if(front->SW_middlepic)
                    {
                        back->flags |= SDF_MIDTEXUPPER;
                        back->SW_toppic = back->SW_middlepic;
                        back->SW_topisflat = back->SW_middleisflat;
                    }
                    else*/
                    {
                        back->SW_toppic =
                            back->sector->SP_ceilpic;

                        back->SW_topisflat =
                            back->sector->SP_ceilisflat;
                    }
                }

                if(front->SW_topflags & SUF_TEXFIX)
                {
                    front->SW_topflags &= ~SUF_TEXFIX;
                    front->SW_toppic = 0;
                }
            }
        }
    }

    for(i = 0; i < sector->subscount; ++i)
    {
        sub = sector->subsectors[i];

        for(k = 0; k < sub->linecount; ++k)
        {
            seg = SEG_PTR(k + sub->firstline);

            // Inform the shadow bias of changed geometry.
            SB_SegHasMoved(seg);
        }

        // Inform the shadow bias of changed geometry.
        SB_PlaneHasMoved(sub, plane);
    }
}

/**
 * When a change is made, this must be called to inform the engine of
 * it.  Repercussions include notifications to the renderer, network...
 */
void P_FloorChanged(sector_t *sector)
{
    P_PlaneChanged(sector, PLN_FLOOR);
}

void P_CeilingChanged(sector_t *sector)
{
    P_PlaneChanged(sector, PLN_CEILING);
}

void P_PolyobjChanged(polyobj_t *po)
{
    uint        i;
    seg_t     **seg = po->segs;

    for(i = 0; i < po->numsegs; ++i, ++seg)
    {
        // Shadow bias must be told.
        SB_SegHasMoved(*seg);
    }
}

/**
 * Begin the process of loading a new map.
 * Can be accessed by the games via the public API.
 *
 * @param levelId   Identifier of the map to be loaded (eg "E1M1").
 *
 * @return boolean  (True) If we loaded the map successfully.
 */
boolean P_LoadMap(char *levelId)
{
    // It would be very cool if map loading happened in another
    // thread. That way we could be keeping ourselves busy while
    // the intermission is played...

    // We could even try to divide a HUB up into zones, so that
    // when a player enters a zone we could begin loading the map(s)
    // reachable through exits in that zone (providing they have
    // enough free memory of course) so that transitions are
    // (potentially) seamless :-)

    // Attempt to load the map
    if(P_LoadMapData(levelId))
    {
        // ALL the map data was loaded/generated successfully.
        P_CheckLevel(levelId, false);

        // Inform the game of our success.
        return true;
    }
    else
    {
        // Unsuccessful... inform the game.
        return false;
    }
}

/**
 * If we encountered any problems during setup - announce them
 * to the user. If the errors couldn't be repaired or we cant
 * continue safely - an error dialog is presented.
 *
 * TODO: latter on this will be expanded to check for various
 *       doom.exe renderer hacks and other stuff.
 *
 * @param silent        (True) Don't announce non-critical errors.
 *
 * @return boolean      (True) We can continue setting up the level.
 */
boolean P_CheckLevel(char *levelID, boolean silent)
{
    uint        i, printCount;
    boolean     canContinue = !numMissingFronts;

    Con_Message("P_CheckLevel: Checking %s for errors...\n", levelID);

    // If we are missing any front sidedefs announce them to the user.
    // Critical
    if(numMissingFronts)
    {
        Con_Message(" ![100] %d linedef(s) missing a front sidedef:\n",
                    numMissingFronts);

        printCount = 0;
        for(i = 0; i < numlines; ++i)
        {
            if(missingFronts[i] == 1)
            {
                Con_Printf("%s%d,", printCount? " ": "   ", i);

                if((++printCount) > 9)
                {   // print 10 per line then wrap.
                    printCount = 0;
                    Con_Printf("\n ");
                }
            }
        }
        Con_Printf("\n");
    }

    // Announce any bad texture names we came across when loading the map.
    // Non-critical as a "MISSING" texture will be drawn in place of them.
    if(numBadTexNames && !silent)
    {
        Con_Message("  [110] %d bad texture name(s):\n", numBadTexNames);

        for(i = 0; i < numBadTexNames; ++i)
            Con_Message("   Found %d unknown %s \"%s\"\n", badTexNames[i].count,
                        (badTexNames[i].planeTex)? "Flat" : "Texture",
                        badTexNames[i].name);
    }

    // Dont need this stuff anymore
    if(missingFronts != NULL)
        M_Free(missingFronts);

    P_FreeBadTexList();

    if(!canContinue)
    {
        Con_Message("\nP_CheckLevel: Critical errors encountered "
                    "(marked with '!').\n  You will need to fix these errors in "
                    "order to play this map.\n");
        return false;
    }

    return true;
}

/**
 * Checks texture *name to see if it is a valid name.
 * Or decompose it into a colormap reference, color value etc...
 *
 * Returns the id of the texture else -1. If we are currently doing level
 * setup and the name isn't valid - add the name to the "bad textures" list
 * so it can be presented to the user (in P_CheckMap()) instead of loads of
 * duplicate missing texture messages from Doomsday.
 *
 * @param planeTex      <code>true</code> @name comes from a map data field
 *                      intended for plane textures (Flats).
 */
int P_CheckTexture(char *name, boolean planeTex, int dataType,
                   unsigned int element, int property)
{
    int         id;
    uint        i;
    char        namet[9];
    boolean     known = false;

    if(planeTex)
        id = R_FlatNumForName(name);
    else
        id = R_TextureNumForName(name);

    // At this point we don't know WHAT it is.
    // Perhaps the game knows what to do?
    if(id == -1 && gx.HandleMapDataPropertyValue)
        id = gx.HandleMapDataPropertyValue(element, dataType, property,
                                           DDVT_FLAT_INDEX, name);

    // Hmm, must be a bad texture name then...?
    // During level setup we collect this info so we can
    // present it to the user latter on.
    if(levelSetup && id == -1)
    {
        namet[8] = 0;
        memcpy(namet, name, 8);

        // Do we already know about it?
        if(numBadTexNames > 0)
        {
            for(i = 0; i < numBadTexNames && !known; ++i)
            {
                if(!strcmp(badTexNames[i].name, namet) &&
                    badTexNames[i].planeTex == planeTex)
                {
                    // Yep we already know about it.
                    known = true;
                    badTexNames[i].count++;
                }
            }
        }

        if(!known)
        {   // A new unknown texture. Add it to the list
            if(++numBadTexNames > maxBadTexNames)
            {
                // Allocate more memory
                maxBadTexNames *= 2;
                if(maxBadTexNames < numBadTexNames)
                    maxBadTexNames = numBadTexNames;

                badTexNames = M_Realloc(badTexNames, sizeof(badtex_t)
                                                    * maxBadTexNames);
            }

            badTexNames[numBadTexNames -1].name = M_Malloc(strlen(namet) +1);
            strcpy(badTexNames[numBadTexNames -1].name, namet);

            badTexNames[numBadTexNames -1].planeTex = planeTex;
            badTexNames[numBadTexNames -1].count = 1;
        }
    }

    return id;
}

/**
 * Frees memory we allocated for bad texture name collection.
 */
static void P_FreeBadTexList(void)
{
    uint        i;

    if(badTexNames != NULL)
    {
        for(i = 0; i < numBadTexNames; ++i)
        {
            M_Free(badTexNames[i].name);
            badTexNames[i].name = NULL;
        }

        M_Free(badTexNames);
        badTexNames = NULL;

        numBadTexNames = maxBadTexNames = 0;
    }
}
