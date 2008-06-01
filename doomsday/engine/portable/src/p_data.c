/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2008 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2008 Daniel Swanson <danij@dengine.net>
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
uint        numVertexes;
vertex_t   *vertexes;

uint        numSegs;
seg_t      *segs;

uint        numSectors;
sector_t   *sectors;

uint        numSSectors;
subsector_t *ssectors;

uint        numNodes;
node_t     *nodes;

uint        numLineDefs;
linedef_t  *lineDefs;

uint        numSideDefs;
sidedef_t  *sideDefs;

watchedplanelist_t *watchedPlaneList;
watchedsurfacelist_t *watchedSurfaceList;

blockmap_t *BlockMap;
blockmap_t *SSecBlockMap;
linkmobj_t *blockrings;             // for mobj rings

byte       *rejectMatrix;           // for fast sight rejection

nodepile_t *mobjNodes, *lineNodes;  // all kinds of wacky links

float       mapGravity;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static gamemap_t *currentMap = NULL;

// Bad texture list
static uint numBadTexNames = 0;
static uint maxBadTexNames = 0;
static badtex_t *badTexNames = NULL;

// Game-specific, map object type definitions.
static uint numGameMapObjDefs;
static gamemapobjdef_t *gameMapObjDefs;

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
    uint                i;
    subsector_t       **ssecp;
    sidedef_t          *front = NULL, *back = NULL;

    // Update the z positions of the degenmobjs for this sector.
    sector->planes[plane]->soundOrg.pos[VZ] = sector->planes[plane]->height;

    if(plane == PLN_FLOOR || plane == PLN_CEILING)
        sector->soundOrg.pos[VZ] =
            (sector->SP_ceilheight - sector->SP_floorheight) / 2;

    for(i = 0; i < sector->lineDefCount; ++i)
    {
        front = sector->lineDefs[i]->L_frontside;
        back  = sector->lineDefs[i]->L_backside;

        if(!front || !front->sector || !back || !back->sector)
            continue;

        /**
         * Do as in the original Doom if the texture has not been defined -
         * extend the floor/ceiling to fill the space (unless its the
         * skyflat), or if there is a midtexture use that instead.
         *
         * \fixme $nplanes.
         */
        if(plane == PLN_FLOOR)
        {
            // Check for missing lowers.
            if(front->sector->SP_floorheight < back->sector->SP_floorheight &&
               !front->SW_bottommaterial)
            {
                if(!R_IsSkySurface(&front->sector->SP_floorsurface))
                {
                    Surface_SetMaterial(&front->SW_bottomsurface,
                        front->sector->SP_floormaterial);
                    front->SW_bottomflags |= SUF_TEXFIX;
                }

                if(back->SW_bottomflags & SUF_TEXFIX)
                {
                    Surface_SetMaterial(&back->SW_bottomsurface, NULL);
                    back->SW_bottomflags &= ~SUF_TEXFIX;
                }
            }
            else if(front->sector->SP_floorheight > back->sector->SP_floorheight &&
                    !back->SW_bottommaterial)
            {
                if(!R_IsSkySurface(&back->sector->SP_floorsurface))
                {
                    Surface_SetMaterial(&back->SW_bottomsurface,
                        back->sector->SP_floormaterial);
                    back->SW_bottomflags |= SUF_TEXFIX;
                }

                if(front->SW_bottomflags & SUF_TEXFIX)
                {
                    Surface_SetMaterial(&front->SW_bottomsurface, NULL);
                    front->SW_bottomflags &= ~SUF_TEXFIX;
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
                    // Preference a middle texture when doing replacements as
                    // this could be a mid tex door hack.
                   /* if(front->SW_middlematerial->texture)
                    {
                        front->flags |= SDF_MIDTEXUPPER;
                        Surface_SetMaterial(front->SW_topsurface,
                            front->SW_middlematerial);
                    }
                    else*/
                    {
                        Surface_SetMaterial(&front->SW_topsurface,
                            front->sector->SP_ceilmaterial);
                    }
                    front->SW_topflags |= SUF_TEXFIX;
                }

                if(back->SW_topflags & SUF_TEXFIX)
                {
                    Surface_SetMaterial(&back->SW_topsurface, NULL);
                    back->SW_topflags &= ~SUF_TEXFIX;
                }
            }
            else if(front->sector->SP_ceilheight < back->sector->SP_ceilheight &&
                    !back->SW_topmaterial)
            {
                if(!R_IsSkySurface(&back->sector->SP_ceilsurface))
                {
                    // Preference a middle texture when doing replacements as
                    // this could be a mid tex door hack.
                   /* if(front->SW_middlematerial->texture)
                    {
                        back->flags |= SDF_MIDTEXUPPER;
                        Surface_SetMaterial(back->SW_topsurface,
                            back->SW_middlematerial);
                    }
                    else*/
                    {
                        Surface_SetMaterial(&back->SW_topsurface,
                            back->sector->SP_ceilmaterial);
                    }
                    back->SW_topflags |= SUF_TEXFIX;
                }

                if(front->SW_topflags & SUF_TEXFIX)
                {
                    Surface_SetMaterial(&front->SW_topsurface, NULL);
                    front->SW_topflags &= ~SUF_TEXFIX;
                }
            }
        }
    }

    // Inform the shadow bias of changed geometry.
    ssecp = sector->ssectors;
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

void P_PolyobjChanged(polyobj_t *po)
{
    uint                i;
    seg_t             **segPtr = po->segs;

    for(i = 0; i < po->numSegs; ++i, segPtr++)
    {
        seg_t              *seg = *segPtr;

        // Shadow bias must be told.
        SB_SegHasMoved(seg);
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
    static char         uid[256];
    filename_t          base;
    int                 lump = W_GetNumForName(mapID);

    M_ExtractFileBase(W_LumpSourceFile(lump), base);

    sprintf(uid, "%s|%s|%s|%s", mapID,
            base, (W_IsFromIWAD(lump) ? "iwad" : "pwad"),
            (char *) gx.GetVariable(DD_GAME_MODE));

    strlwr(uid);
    return uid;
}

/**
 * @return              Ptr to the current map.
 */
gamemap_t *P_GetCurrentMap(void)
{
    return currentMap;
}

void P_SetCurrentMap(gamemap_t *map)
{
    strncpy(levelid, map->levelID, sizeof(levelid));

    numVertexes = map->numVertexes;
    vertexes = map->vertexes;

    numSegs = map->numSegs;
    segs = map->segs;

    numSectors = map->numSectors;
    sectors = map->sectors;

    numSSectors = map->numSSectors;
    ssectors = map->ssectors;

    numNodes = map->numNodes;
    nodes = map->nodes;

    numLineDefs = map->numLineDefs;
    lineDefs = map->lineDefs;

    numSideDefs = map->numSideDefs;
    sideDefs = map->sideDefs;

    watchedPlaneList = &map->watchedPlaneList;
    watchedSurfaceList = &map->watchedSurfaceList;

    numPolyObjs = map->numPolyObjs;
    polyObjs = map->polyObjs;

    rejectMatrix = map->rejectMatrix;
    mobjNodes = &map->mobjNodes;
    lineNodes = &map->lineNodes;
    linelinks = map->lineLinks;

    BlockMap = map->blockMap;
    SSecBlockMap = map->ssecBlockMap;
    blockrings = map->blockRings;

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

    return map->levelID;
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
    min[VX] = map->bBox[BOXLEFT];
    min[VY] = map->bBox[BOXBOTTOM];

    max[VX] = map->bBox[BOXRIGHT];
    max[VY] = map->bBox[BOXTOP];
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
    uint                startTime = Sys_GetRealTime();

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
 * @param levelId       Identifier of the map to be loaded (eg "E1M1").
 *
 * @return              @c true, if the map was loaded successfully.
 */
boolean P_LoadMap(const char *mapID)
{
    uint                i;

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
        for(i = 0; i < DDMAXPLAYERS; ++i)
        {
            player_t           *plr = &ddPlayers[i];

            if(!(plr->shared.flags & DDPF_LOCAL) && clients[i].connected)
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
        uint                i;
        gamemap_t          *map = P_GetCurrentMap();

        Cl_Reset();
        RL_DeleteLists();
        Rend_CalcLightModRange(NULL);

        // Invalidate old cmds and init player values.
        for(i = 0; i < DDMAXPLAYERS; ++i)
        {
            player_t           *plr = &ddPlayers[i];

            if(isServer && plr->shared.inGame)
                clients[i].runTime = SECONDS_TO_TICKS(gameTime);

            plr->extraLight = plr->targetExtraLight = 0;
            plr->extraLightCounter = 0;
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
            R_InitRendVerticesPool();
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

/**
 * Look up a mapobj definition.
 *
 * @param identifer     If objName is @c NULL, compare using this unique identifier.
 * @param objName       If not @c NULL, compare using this unique name.
 */
gamemapobjdef_t *P_GetGameMapObjDef(int identifier, const char *objName,
                                    boolean canCreate)
{
    uint                i;
    size_t              len;
    gamemapobjdef_t    *def;

    if(objName)
        len = strlen(objName);

    // Is this a known game object?
    for(i = 0; i < numGameMapObjDefs; ++i)
    {
        def = &gameMapObjDefs[i];

        if(objName && objName[0])
        {
            if(!strnicmp(objName, def->name, len))
            {   // Found it!
                return def;
            }
        }
        else
        {
            if(identifier == def->identifier)
            {   // Found it!
                return def;
            }
        }
    }

    if(!canCreate)
        return NULL; // Not a known game map object.

    if(identifier == 0)
        return NULL; // Not a valid indentifier.

    if(!objName || !objName[0])
        return NULL; // Must have a name.

    // Ensure the name is unique.
    for(i = 0; i < numGameMapObjDefs; ++i)
    {
        def = &gameMapObjDefs[i];
        if(!strnicmp(objName, def->name, len))
        {   // Oh dear, a duplicate.
            return NULL;
        }
    }

    gameMapObjDefs =
        M_Realloc(gameMapObjDefs, ++numGameMapObjDefs * sizeof(*gameMapObjDefs));

    def = &gameMapObjDefs[numGameMapObjDefs - 1];
    def->identifier = identifier;
    def->name = M_Malloc(len+1);
    strncpy(def->name, objName, len);
    def->name[len] = '\0';
    def->numProps = 0;
    def->props = NULL;

    return def;
}

/**
 * Called by the game to register the map object types it wishes us to make
 * public via the MPE interface.
 */
boolean P_RegisterMapObj(int identifier, const char *name)
{
    if(P_GetGameMapObjDef(identifier, name, true))
        return true; // Success.

    return false;
}

/**
 * Called by the game to add a new property to a previously registered
 * map object type definition.
 */
boolean P_RegisterMapObjProperty(int identifier, int propIdentifier,
                                 const char *propName, valuetype_t type)
{
    uint                    i;
    size_t                  len;
    mapobjprop_t           *prop;
    gamemapobjdef_t        *def = P_GetGameMapObjDef(identifier, NULL, false);

    if(!def) // Not a valid identifier.
    {
        Con_Error("P_RegisterMapObjProperty: Unknown mapobj identifier %i.",
                  identifier);
    }

    if(propIdentifier == 0) // Not a valid identifier.
        Con_Error("P_RegisterMapObjProperty: 0 not valid for propIdentifier.");

    if(!propName || !propName[0]) // Must have a name.
        Con_Error("P_RegisterMapObjProperty: Cannot register without name.");

    // Screen out value types we don't currently support for gmos.
    switch(type)
    {
    case DDVT_BYTE:
    case DDVT_SHORT:
    case DDVT_INT:
    case DDVT_FIXED:
    case DDVT_ANGLE:
    case DDVT_FLOAT:
        break;

    default:
        Con_Error("P_RegisterMapObjProperty: Unknown/not supported value type %i.",
                  type);
    }

    // Next, make sure propIdentifer and propName are unique.
    len = strlen(propName);
    for(i = 0; i < def->numProps; ++i)
    {
        prop = &def->props[i];

        if(prop->identifier == propIdentifier)
            Con_Error("P_RegisterMapObjProperty: propIdentifier %i not unique for %s.",
                      propIdentifier, def->name);

        if(!strnicmp(propName, prop->name, len))
            Con_Error("P_RegisterMapObjProperty: propName \"%s\" not unique for %s.",
                      propName, def->name);
    }

    // Looks good! Add it to the list of properties.
    def->props = M_Realloc(def->props, ++def->numProps * sizeof(*def->props));

    prop = &def->props[def->numProps - 1];
    prop->identifier = propIdentifier;
    prop->name = M_Malloc(len + 1);
    strncpy(prop->name, propName, len);
    prop->name[len] = '\0';
    prop->type = type;

    return true; // Success!
}

/**
 * Called during init to initialize the map obj defs.
 */
void P_InitGameMapObjDefs(void)
{
    gameMapObjDefs = NULL;
    numGameMapObjDefs = 0;
}

/**
 * Called at shutdown to free all memory allocated for the map obj defs.
 */
void P_ShutdownGameMapObjDefs(void)
{
    uint                i, j;

    if(gameMapObjDefs)
    {
        for(i = 0; i < numGameMapObjDefs; ++i)
        {
            gamemapobjdef_t    *def = &gameMapObjDefs[i];

            for(j = 0; j < def->numProps; ++j)
            {
                mapobjprop_t       *prop = &def->props[i];

                M_Free(prop->name);
            }

            M_Free(def->name);
            M_Free(def->props);
        }

        M_Free(gameMapObjDefs);
    }

    gameMapObjDefs = NULL;
    numGameMapObjDefs = 0;
}

static valuetable_t *getDBTable(valuedb_t *db, valuetype_t type,
                                boolean canCreate)
{
    uint                i;
    valuetable_t       *tbl;

    if(!db)
        return NULL;

    for(i = 0; i < db->numTables; ++i)
    {
        tbl = db->tables[i];
        if(tbl->type == type)
        {   // Found it!
            return tbl;
        }
    }

    if(!canCreate)
        return NULL;

    // We need to add a new value table to the db.
    db->tables = M_Realloc(db->tables, ++db->numTables * sizeof(*db->tables));
    tbl = db->tables[db->numTables - 1] = M_Malloc(sizeof(valuetable_t));

    tbl->data = NULL;
    tbl->type = type;
    tbl->numElms = 0;

    return tbl;
}

static uint insertIntoDB(valuedb_t *db, valuetype_t type, void *data)
{
    valuetable_t       *tbl = getDBTable(db, type, true);

    // Insert the new value.
    switch(type)
    {
    case DDVT_BYTE:
        tbl->data = M_Realloc(tbl->data, ++tbl->numElms);
        ((byte*) tbl->data)[tbl->numElms - 1] = *((byte*) data);
        break;

    case DDVT_SHORT:
        tbl->data = M_Realloc(tbl->data, ++tbl->numElms * sizeof(short));
        ((short*) tbl->data)[tbl->numElms - 1] = *((short*) data);
        break;

    case DDVT_INT:
        tbl->data = M_Realloc(tbl->data, ++tbl->numElms * sizeof(int));
        ((int*) tbl->data)[tbl->numElms - 1] = *((int*) data);
        break;

    case DDVT_FIXED:
        tbl->data = M_Realloc(tbl->data, ++tbl->numElms * sizeof(fixed_t));
        ((fixed_t*) tbl->data)[tbl->numElms - 1] = *((fixed_t*) data);
        break;

    case DDVT_ANGLE:
        tbl->data = M_Realloc(tbl->data, ++tbl->numElms * sizeof(angle_t));
        ((angle_t*) tbl->data)[tbl->numElms - 1] = *((angle_t*) data);
        break;

    case DDVT_FLOAT:
        tbl->data = M_Realloc(tbl->data, ++tbl->numElms * sizeof(float));
        ((float*) tbl->data)[tbl->numElms - 1] = *((float*) data);
        break;

    default:
        Con_Error("insetIntoDB: Unknown value type %d.", type);
    }

    return tbl->numElms - 1;
}

static void* getPtrToDBElm(valuedb_t *db, valuetype_t type, uint elmIdx)
{
    valuetable_t       *tbl = getDBTable(db, type, false);

    if(!tbl)
        Con_Error("getPtrToDBElm: Table for type %i not found.", (int) type);

    // Sanity check: ensure the elmIdx is in bounds.
    if(elmIdx < 0 || elmIdx >= tbl->numElms)
        Con_Error("P_GetGMOByte: valueIdx out of bounds.");

    switch(tbl->type)
    {
    case DDVT_BYTE:
        return &(((byte*) tbl->data)[elmIdx]);

    case DDVT_SHORT:
        return &(((short*)tbl->data)[elmIdx]);

    case DDVT_INT:
        return &(((int*) tbl->data)[elmIdx]);

    case DDVT_FIXED:
        return &(((fixed_t*) tbl->data)[elmIdx]);

    case DDVT_ANGLE:
        return &(((angle_t*) tbl->data)[elmIdx]);

    case DDVT_FLOAT:
        return &(((float*) tbl->data)[elmIdx]);

    default:
        Con_Error("P_GetGMOByte: Invalid table type %i.", tbl->type);
    }

    // Should never reach here.
    return NULL;
}

/**
 * Destroy the given game map obj database.
 */
void P_DestroyGameMapObjDB(gameobjdata_t *moData)
{
    uint                i;

    if(moData->objs)
    {
        for(i = 0; i < moData->numObjs; ++i)
        {
            gamemapobj_t       *gmo = moData->objs[i];

            if(gmo->props)
                M_Free(gmo->props);

            M_Free(gmo);
        }

        M_Free(moData->objs);
    }
    moData->objs = NULL;
    moData->numObjs = 0;

    if(moData->db.tables)
    {
        for(i = 0; i < moData->db.numTables; ++i)
        {
            valuetable_t       *tbl = moData->db.tables[i];

            if(tbl->data)
                M_Free(tbl->data);

            M_Free(tbl);
        }

        M_Free(moData->db.tables);
    }
    moData->db.tables = NULL;
    moData->db.numTables = 0;
}

static uint countGameMapObjs(gameobjdata_t *moData, int identifier)
{
    uint                i, n;

    if(!moData)
        return 0;

    for(i = 0, n = 0; i < moData->numObjs; ++i)
    {
        gamemapobj_t       *gmo = moData->objs[i];

        if(gmo->def->identifier == identifier)
            n++;
    }

    return n;
}

uint P_CountGameMapObjs(int identifier)
{
    gamemap_t          *map = P_GetCurrentMap();

    return countGameMapObjs(&map->gameObjData, identifier);
}

gamemapobj_t* P_GetGameMapObj(gameobjdata_t *moData, gamemapobjdef_t *def,
                              uint elmIdx, boolean canCreate)
{
    uint                i;
    gamemapobj_t       *gmo;

    // Have we already created this gmo?
    for(i = 0; i < moData->numObjs; ++i)
    {
        gmo = moData->objs[i];
        if(gmo->def == def && gmo->elmIdx == elmIdx)
            return gmo; // Yep, return it.
    }

    if(!canCreate)
        return NULL;

    // It is a new gamemapobj.
    moData->objs =
        M_Realloc(moData->objs, ++moData->numObjs * sizeof(*moData->objs));

    gmo = moData->objs[moData->numObjs - 1] = M_Malloc(sizeof(*gmo));
    gmo->def = def;
    gmo->elmIdx = elmIdx;
    gmo->numProps = 0;
    gmo->props = NULL;

    return gmo;
}

void P_AddGameMapObjValue(gameobjdata_t *moData, gamemapobjdef_t *gmoDef,
                          uint propIdx, uint elmIdx, valuetype_t type,
                          void *data)
{
    uint                i;
    customproperty_t   *prop;
    gamemapobj_t       *gmo = P_GetGameMapObj(moData, gmoDef, elmIdx, true);

    if(!gmo)
        Con_Error("addGameMapObj: Failed creation.");

    // Check whether this is a new value or whether we are updating an
    // existing one.
    for(i = 0; i < gmo->numProps; ++i)
    {
        if(gmo->props[i].idx == propIdx)
        {   // We are updating.
            //if(gmo->props[i].type == type)
            //    updateInDB(map->values, type, gmo->props[i].valueIdx, data);
            //else
                Con_Error("addGameMapObj: Value type change not currently supported.");
            return;
        }
    }

    // Its a new value.
    gmo->props = M_Realloc(gmo->props, ++gmo->numProps * sizeof(*gmo->props));

    prop = &gmo->props[gmo->numProps - 1];
    prop->idx = propIdx;
    prop->type = type;
    prop->valueIdx = insertIntoDB(&moData->db, type, data);
}

static void* getGMOPropValue(gameobjdata_t *data, int identifier,
                             uint elmIdx, int propIdentifier,
                             valuetype_t *type)
{
    uint                i;
    gamemapobjdef_t    *def;
    gamemapobj_t       *gmo;

    if((def = P_GetGameMapObjDef(identifier, NULL, false)) == NULL)
        Con_Error("P_GetGMOByte: Invalid identifier %i.", identifier);

    if((gmo = P_GetGameMapObj(data, def, elmIdx, false)) == NULL)
        Con_Error("P_GetGMOByte: There is no element %i of type %s.", elmIdx,
                  def->name);

    // Find the requested property.
    for(i = 0; i < gmo->numProps; ++i)
    {
        customproperty_t   *prop = &gmo->props[i];

        if(def->props[prop->idx].identifier == propIdentifier)
        {
            void               *ptr;

            if(NULL ==
               (ptr = getPtrToDBElm(&data->db, prop->type, prop->valueIdx)))
                Con_Error("P_GetGMOByte: Failed db look up.");

            if(type)
                *type = prop->type;

            return ptr;
        }
    }

    return NULL;
}

static void setValue(void *dst, valuetype_t dstType, void *src,
                     valuetype_t srcType)
{
    if(dstType == DDVT_FIXED)
    {
        fixed_t        *d = dst;

        switch(srcType)
        {
        case DDVT_BYTE:
            *d = (*((byte*) src) << FRACBITS);
            break;
        case DDVT_INT:
            *d = (*((int*) src) << FRACBITS);
            break;
        case DDVT_FIXED:
            *d = *((fixed_t*) src);
            break;
        case DDVT_FLOAT:
            *d = FLT2FIX(*((float*) src));
            break;
        default:
            Con_Error("SetValue: DDVT_FIXED incompatible with value type %s.\n",
                      value_Str(srcType));
        }
    }
    else if(dstType == DDVT_FLOAT)
    {
        float          *d = dst;

        switch(srcType)
        {
        case DDVT_BYTE:
            *d = *((byte*) src);
            break;
        case DDVT_INT:
            *d = *((int*) src);
            break;
        case DDVT_SHORT:
            *d = *((short*) src);
            break;
        case DDVT_FIXED:
            *d = FIX2FLT(*((fixed_t*) src));
            break;
        case DDVT_FLOAT:
            *d = *((float*) src);
            break;
        default:
            Con_Error("SetValue: DDVT_FLOAT incompatible with value type %s.\n",
                      value_Str(srcType));
        }
    }
    else if(dstType == DDVT_BYTE)
    {
        byte           *d = dst;

        switch(srcType)
        {
        case DDVT_BYTE:
            *d = *((byte*) src);
            break;
        case DDVT_INT:
            *d = *((int*) src);
            break;
        case DDVT_FLOAT:
            *d = (byte) *((float*) src);
            break;
        default:
            Con_Error("SetValue: DDVT_BYTE incompatible with value type %s.\n",
                      value_Str(srcType));
        }
    }
    else if(dstType == DDVT_INT)
    {
        int            *d = dst;

        switch(srcType)
        {
        case DDVT_BYTE:
            *d = *((byte*) src);
            break;
        case DDVT_INT:
            *d = *((int*) src);
            break;
        case DDVT_SHORT:
            *d = *((short*) src);
            break;
        case DDVT_FLOAT:
            *d = *((float*) src);
            break;
        case DDVT_FIXED:
            *d = (*((fixed_t*) src) >> FRACBITS);
            break;
        default:
            Con_Error("SetValue: DDVT_INT incompatible with value type %s.\n",
                      value_Str(srcType));
        }
    }
    else if(dstType == DDVT_SHORT)
    {
        short          *d = dst;

        switch(srcType)
        {
        case DDVT_BYTE:
            *d = *((byte*) src);
            break;
        case DDVT_INT:
            *d = *((int*) src);
            break;
        case DDVT_SHORT:
            *d = *((short*) src);
            break;
        case DDVT_FLOAT:
            *d = *((float*) src);
            break;
        case DDVT_FIXED:
            *d = (*((fixed_t*) src) >> FRACBITS);
            break;
        default:
            Con_Error("SetValue: DDVT_SHORT incompatible with value type %s.\n",
                      value_Str(srcType));
        }
    }
    else if(dstType == DDVT_ANGLE)
    {
        angle_t        *d = dst;

        switch(srcType)
        {
        case DDVT_ANGLE:
            *d = *((angle_t*) src);
            break;
        default:
            Con_Error("SetValue: DDVT_ANGLE incompatible with value type %s.\n",
                      value_Str(srcType));
        }
    }
    else
    {
        Con_Error("SetValue: unknown value type %d.\n", dstType);
    }
}

byte P_GetGMOByte(int identifier, uint elmIdx, int propIdentifier)
{
    valuetype_t         type;
    gamemap_t          *map = P_GetCurrentMap();
    void               *ptr;
    byte                returnVal;

    ptr = getGMOPropValue(&map->gameObjData, identifier, elmIdx,
                          propIdentifier, &type);
    // Handle some basic type conversions.
    setValue(&returnVal, DDVT_BYTE, ptr, type);
    return returnVal;
}

short P_GetGMOShort(int identifier, uint elmIdx, int propIdentifier)
{
    valuetype_t         type;
    gamemap_t          *map = P_GetCurrentMap();
    void               *ptr;
    short               returnVal;

    ptr = getGMOPropValue(&map->gameObjData, identifier, elmIdx,
                          propIdentifier, &type);
    // Handle some basic type conversions.
    setValue(&returnVal, DDVT_SHORT, ptr, type);
    return returnVal;
}

int P_GetGMOInt(int identifier, uint elmIdx, int propIdentifier)
{
    valuetype_t         type;
    gamemap_t          *map = P_GetCurrentMap();
    void               *ptr;
    int                 returnVal;

    ptr = getGMOPropValue(&map->gameObjData, identifier, elmIdx,
                          propIdentifier, &type);
    // Handle some basic type conversions.
    setValue(&returnVal, DDVT_INT, ptr, type);
    return returnVal;
}

fixed_t P_GetGMOFixed(int identifier, uint elmIdx, int propIdentifier)
{
    valuetype_t         type;
    gamemap_t          *map = P_GetCurrentMap();
    void               *ptr;
    fixed_t             returnVal;

    ptr = getGMOPropValue(&map->gameObjData, identifier, elmIdx,
                          propIdentifier, &type);
    // Handle some basic type conversions.
    setValue(&returnVal, DDVT_FIXED, ptr, type);
    return returnVal;
}

angle_t P_GetGMOAngle(int identifier, uint elmIdx, int propIdentifier)
{
    valuetype_t         type;
    gamemap_t          *map = P_GetCurrentMap();
    void               *ptr;
    angle_t             returnVal;

    ptr = getGMOPropValue(&map->gameObjData, identifier, elmIdx,
                          propIdentifier, &type);
    // Handle some basic type conversions.
    setValue(&returnVal, DDVT_ANGLE, ptr, type);
    return returnVal;
}

float P_GetGMOFloat(int identifier, uint elmIdx, int propIdentifier)
{
    valuetype_t         type;
    gamemap_t          *map = P_GetCurrentMap();
    void               *ptr;
    float               returnVal;

    ptr = getGMOPropValue(&map->gameObjData, identifier, elmIdx,
                          propIdentifier, &type);
    // Handle some basic type conversions.
    setValue(&returnVal, DDVT_FLOAT, ptr, type);
    return returnVal;
}
