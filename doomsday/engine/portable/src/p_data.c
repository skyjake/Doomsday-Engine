/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2007 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2007 Daniel Swanson <danij@dengine.net>
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
 * p_data.c: Playsim Data Structures, Macros and Constants
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_console.h"
#include "de_network.h"
#include "de_play.h"
#include "de_render.h"
#include "de_refresh.h"
#include "de_system.h"
#include "de_misc.h"

#include "rend_bias.h"
#include "m_bams.h"

#include <math.h>
#include <stddef.h>

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// Bad texture record.
typedef struct {
    char       *name;
    boolean     planeTex;
    uint    count;
} badtex_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern boolean levelSetup;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

/**
 * These map data arrays are internal to the engine.
 */
char        levelid[9]; // Name by which the game referred to the current map.
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

watchedplanelist_t *watchedPlaneList;

// mapthings are actually stored & handled game-side
uint        numthings;

blockmap_t *BlockMap;
blockmap_t *SSecBlockMap;
linkmobj_t *blockrings;             // for mobj rings

byte       *rejectmatrix;           // for fast sight rejection

nodepile_t *mobjnodes, *linenodes;  // all kinds of wacky links

float       mapGravity;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static gamemap_t *currentMap = NULL;

// Bad texture list
static uint numBadTexNames = 0;
static uint maxBadTexNames = 0;
static badtex_t *badTexNames = NULL;

// CODE --------------------------------------------------------------------

void P_InitData(void)
{
    P_InitMapUpdate();
}

/**
 * \todo Consolidate with R_UpdatePlanes?
 */
void P_PlaneChanged(sector_t *sector, uint plane)
{
    uint        i;
    subsector_t **ssecp;
    side_t     *front = NULL, *back = NULL;

    // Update the z positions of the degenmobjs for this sector.
    sector->planes[plane]->soundorg.pos[VZ] = sector->planes[plane]->height;

    if(plane == PLN_FLOOR || plane == PLN_CEILING)
        sector->soundorg.pos[VZ] =
            (sector->SP_ceilheight - sector->SP_floorheight) / 2;

    for(i = 0; i < sector->linecount; ++i)
    {
        front = sector->Lines[i]->L_frontside;
        back  = sector->Lines[i]->L_backside;

        if(!front || !front->sector || !back || !back->sector)
            continue;

        /**
         * Do as in the original Doom if the texture has not been defined -
         * extend the floor/ceiling to fill the space (unless its the skyflat),
         * or if there is a midtexture use that instead.
         */
        if(plane == PLN_FLOOR)
        {
            // Check for missing lowers.
            if(front->sector->SP_floorheight < back->sector->SP_floorheight &&
               !front->SW_bottommaterial)
            {
                if(!R_IsSkySurface(&front->sector->SP_floorsurface))
                {
                    front->SW_bottomflags |= SUF_TEXFIX;

                    front->SW_bottommaterial =
                        front->sector->SP_floormaterial;
                }

                if(back->SW_bottomflags & SUF_TEXFIX)
                {
                    back->SW_bottomflags &= ~SUF_TEXFIX;
                    back->SW_bottommaterial = NULL;
                }
            }
            else if(front->sector->SP_floorheight > back->sector->SP_floorheight &&
                    !back->SW_bottommaterial)
            {
                if(!R_IsSkySurface(&back->sector->SP_floorsurface))
                {
                    back->SW_bottomflags |= SUF_TEXFIX;

                    back->SW_bottommaterial =
                        back->sector->SP_floormaterial;
                }

                if(front->SW_bottomflags & SUF_TEXFIX)
                {
                    front->SW_bottomflags &= ~SUF_TEXFIX;
                    front->SW_bottommaterial = NULL;
                }
            }
        }
        else
        {
            // Check for missing uppers.
            if(front->sector->SP_ceilheight > back->sector->SP_ceilheight &&
               !front->SW_topmaterial)
            {
                if(!R_IsSkySurface(&front->sector->SP_ceilsurface))
                {
                    front->SW_topflags |= SUF_TEXFIX;
                    // Preference a middle texture when doing replacements as
                    // this could be a mid tex door hack.
                   /* if(front->SW_middlematerial->texture)
                    {
                        front->flags |= SDF_MIDTEXUPPER;
                        front->SW_topmaterial = front->SW_middlematerial;
                    }
                    else*/
                    {
                        front->SW_topmaterial =
                            front->sector->SP_ceilmaterial;
                    }
                }

                if(back->SW_topflags & SUF_TEXFIX)
                {
                    back->SW_topflags &= ~SUF_TEXFIX;
                    back->SW_topmaterial = NULL;
                }
            }
            else if(front->sector->SP_ceilheight < back->sector->SP_ceilheight &&
                    !back->SW_topmaterial)
            {
                if(!R_IsSkySurface(&back->sector->SP_ceilsurface))
                {
                    back->SW_topflags |= SUF_TEXFIX;
                    // Preference a middle texture when doing replacements as
                    // this could be a mid tex door hack.
                   /* if(front->SW_middlematerial->texture)
                    {
                        back->flags |= SDF_MIDTEXUPPER;
                        back->SW_topmaterial = back->SW_middlematerial;
                    }
                    else*/
                    {
                        back->SW_topmaterial =
                            back->sector->SP_ceilmaterial;
                    }
                }

                if(front->SW_topflags & SUF_TEXFIX)
                {
                    front->SW_topflags &= ~SUF_TEXFIX;
                    front->SW_topmaterial = NULL;
                }
            }
        }
    }

    // Inform the shadow bias of changed geometry.
    ssecp = sector->subsectors;
    while(*ssecp)
    {
        subsector_t *ssec = *ssecp;
        seg_t     **segp = ssec->segs;

        while(*segp)
        {
            seg_t      *seg = *segp;
            SB_SegHasMoved(seg);
            *segp++;
        }

        SB_PlaneHasMoved(ssec, plane);
        *ssecp++;
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
 * Generate a 'unique' identifier for the map.  This identifier
 * contains information about the map tag (E3M3), the WAD that
 * contains the map (DOOM.IWAD), and the game mode (doom-ultimate).
 *
 * The entire ID string will be in lowercase letters.
 */
const char *P_GenerateUniqueMapID(const char *mapID)
{
    static char uid[256];
    filename_t  base;
    int         lump = W_GetNumForName(mapID);

    M_ExtractFileBase(W_LumpSourceFile(lump), base);

    sprintf(uid, "%s|%s|%s|%s", mapID,
            base, (W_IsFromIWAD(lump) ? "iwad" : "pwad"),
            (char *) gx.GetVariable(DD_GAME_MODE));

    strlwr(uid);
    return uid;
}

/**
 * @return          Ptr to the current map.
 */
gamemap_t *P_GetCurrentMap(void)
{
    return currentMap;
}

void P_SetCurrentMap(gamemap_t *map)
{
    strncpy(levelid, map->levelid, sizeof(levelid));

    numthings = map->numthings;

    numvertexes = map->numvertexes;
    vertexes = map->vertexes;

    numsegs = map->numsegs;
    segs = map->segs;

    numsectors = map->numsectors;
    sectors = map->sectors;

    numsubsectors = map->numsubsectors;
    subsectors = map->subsectors;

    numnodes = map->numnodes;
    nodes = map->nodes;

    numlines = map->numlines;
    lines = map->lines;

    numsides = map->numsides;
    sides = map->sides;

    watchedPlaneList = &map->watchedPlaneList;

    po_NumPolyobjs = map->numpolyobjs;
    polyobjs = map->polyobjs;

    rejectmatrix = map->rejectmatrix;
    mobjnodes = &map->mobjnodes;
    linenodes = &map->linenodes;
    linelinks = map->linelinks;

    BlockMap = map->blockMap;
    SSecBlockMap = map->ssecBlockMap;
    blockrings = map->blockrings;

    mapGravity = map->globalGravity;

    currentMap = map;
}

/**
 * This ID is the name of the lump tag that marks the beginning of map
 * data, e.g. "MAP03" or "E2M8".
 */
const char *P_GetMapID(gamemap_t *map)
{
    if(!map)
        return NULL;

    return map->levelid;
}

/**
 * Return the 'unique' identifier of the map.
 */
const char *P_GetUniqueMapID(gamemap_t *map)
{
    if(!map)
        return NULL;

    return map->uniqueID;
}

void P_GetMapBounds(gamemap_t *map, float *min, float *max)
{
    min[VX] = map->bbox[BOXLEFT];
    min[VY] = map->bbox[BOXBOTTOM];

    max[VX] = map->bbox[BOXRIGHT];
    max[VY] = map->bbox[BOXTOP];
}

/**
 * Get the ambient light level of the specified map.
 */
int P_GetMapAmbientLightLevel(gamemap_t *map)
{
    if(!map)
        return 0;

    return map->ambientLightLevel;
}

static void spawnParticleGeneratorsForMap(const char *mapID)
{
    uint        startTime = Sys_GetRealTime();

    // Spawn all type-triggered particle generators.
    // Let's hope there aren't too many...
    P_SpawnTypeParticleGens();
    P_SpawnMapParticleGens(mapID);

    // How much time did we spend?
    VERBOSE(Con_Message
            ("spawnParticleGeneratorsForMap: Done in %.2f seconds.\n",
             (Sys_GetRealTime() - startTime) / 1000.0f));
}

/**
 * Begin the process of loading a new map.
 * Can be accessed by the games via the public API.
 *
 * @param levelId   Identifier of the map to be loaded (eg "E1M1").
 *
 * @return          @c true, if the map was loaded successfully.
 */
boolean P_LoadMap(const char *mapID)
{
    uint            i;

    if(!mapID || !mapID[0])
        return false; // Yeah, ok... :P

    Con_Message("P_LoadMap: \"%s\"\n", mapID);

    // It would be very cool if map loading happened in another
    // thread. That way we could be keeping ourselves busy while
    // the intermission is played...

    // We could even try to divide a HUB up into zones, so that
    // when a player enters a zone we could begin loading the map(s)
    // reachable through exits in that zone (providing they have
    // enough free memory of course) so that transitions are
    // (potentially) seamless :-)

    if(isServer)
    {
        // Whenever the map changes, remote players must tell us when
        // they're ready to begin receiving frames.
        for(i = 0; i < MAXPLAYERS; ++i)
        {
            if(!(players[i].flags & DDPF_LOCAL) && clients[i].connected)
            {
#ifdef _DEBUG
                Con_Printf("Cl%i NOT READY ANY MORE!\n", i);
#endif
                clients[i].ready = false;
            }
        }
    }

    if(DAM_AttemptMapLoad(mapID))
    {
        uint            i;
        gamemap_t      *map = P_GetCurrentMap();

        Cl_Reset();
        RL_DeleteLists();
        Rend_CalcLightRangeModMatrix(NULL);

        // Invalidate old cmds and init player values.
        for(i = 0; i < MAXPLAYERS; ++i)
        {
            ddplayer_t      *plr = &players[i];

            if(isServer && plr->ingame)
                clients[i].runTime = SECONDS_TO_TICKS(gameTime);
        }

        // Make sure that the next frame doesn't use a filtered viewer.
        R_ResetViewer();

        // Texture animations should begin from their first step.
        R_ResetAnimGroups();

        // Init Particle Generator links.
        PG_InitForLevel();

        LO_InitForMap(); // Lumobj management.
        DL_InitForMap(); // Projected lumobjs (dynlights) management.

        spawnParticleGeneratorsForMap(P_GetMapID(map));

        // Tell shadow bias to initialize the bias light sources.
        SB_InitForMap(P_GetUniqueMapID(map));

        // Initialize the lighting grid.
        LG_Init();

        if(!isDedicated)
            R_InitRendPolyPool();
        return true;
    }

    return false;
}

void P_RegisterUnknownTexture(const char *name, boolean planeTex)
{
    uint        i;
    char        namet[9];
    boolean     known = false;

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

void P_PrintMissingTextureList(void)
{
    // Announce any bad texture names we came across when loading the map.
    // Non-critical as a "MISSING" texture will be drawn in place of them.
    if(numBadTexNames)
    {
        uint        i;

        Con_Message("  [110] Warning: Found %u bad texture name(s):\n",
                    numBadTexNames);

        for(i = 0; i < numBadTexNames; ++i)
            Con_Message(" %4u x \"%s\"\n", badTexNames[i].count,
                        badTexNames[i].name);
    }
}

/**
 * Frees memory we allocated for bad texture name collection.
 */
void P_FreeBadTexList(void)
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

/**
 * Checks texture *name to see if it is a valid name.
 * Or decompose it into a colormap reference, color value etc...
 *
 * Returns the id of the texture else -1. If we are currently doing level
 * setup and the name isn't valid - add the name to the "bad textures" list
 * so it can be presented to the user (in P_CheckMap()) instead of loads of
 * duplicate missing texture messages from Doomsday.
 *
 * @param planeTex      @c true == 'name' comes from a map data field
 *                      intended for plane textures (Flats).
 */
int P_CheckTexture(char *name, boolean planeTex, int dataType,
                   unsigned int element, int property)
{
    int         id;

    id = R_MaterialNumForName(name, (planeTex? MAT_FLAT : MAT_TEXTURE));

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
        P_RegisterUnknownTexture(name, planeTex);
    }

    return id;
}
