/** @file p_objlink.cpp
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
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

#include <math.h>

#include "de_base.h"
#include "de_console.h"
#include "de_render.h"
#include "de_graphics.h"
#include "de_misc.h"
#include "de_play.h"
#include "de_defs.h"

#include "gridmap.h"

#define BLOCK_WIDTH                 (128)
#define BLOCK_HEIGHT                (128)

BEGIN_PROF_TIMERS()
  PROF_OBJLINK_SPREAD,
  PROF_OBJLINK_LINK
END_PROF_TIMERS()

typedef struct objlink_s {
    struct objlink_s* nextInBlock; /// Next in the same obj block, or NULL.
    struct objlink_s* nextUsed;
    struct objlink_s* next; /// Next in list of ALL objlinks.
    objtype_t type;
    void* obj;
} objlink_t;

typedef struct {
    objlink_t* head;
    /// Used to prevent repeated per-frame processing of a block.
    boolean  doneSpread;
} objlinkblock_t;

typedef struct {
    coord_t origin[2]; /// Origin of the blockmap in world coordinates [x,y].
    Gridmap* gridmap;
} objlinkblockmap_t;

typedef struct {
    void* obj;
    objtype_t objType;
    coord_t objOrigin[3];
    coord_t objRadius;
    coord_t box[4];
} contactfinderparams_t;

typedef struct objcontact_s {
    struct objcontact_s* next; /// Next in the BSP leaf.
    struct objcontact_s* nextUsed; /// Next used contact.
    void* obj;
} objcontact_t;

typedef struct {
    objcontact_t* head[NUM_OBJ_TYPES];
} objcontactlist_t;

static void processSeg(HEdge* hedge, void* data);

static objlink_t* objlinks = NULL;
static objlink_t* objlinkFirst = NULL, *objlinkCursor = NULL;

// Each objlink type gets its own blockmap.
static objlinkblockmap_t blockmaps[NUM_OBJ_TYPES];

// List of unused and used contacts.
static objcontact_t* contFirst = NULL, *contCursor = NULL;

// List of contacts for each BSP leaf.
static objcontactlist_t* bspLeafContacts = NULL;

static __inline objlinkblockmap_t* chooseObjlinkBlockmap(objtype_t type)
{
    assert(VALID_OBJTYPE(type));
    return blockmaps + (int)type;
}

static __inline uint toObjlinkBlockmapX(objlinkblockmap_t* obm, coord_t x)
{
    assert(obm && x >= obm->origin[0]);
    return (uint)((x - obm->origin[0]) / (coord_t)BLOCK_WIDTH);
}

static __inline uint toObjlinkBlockmapY(objlinkblockmap_t* obm, coord_t y)
{
    assert(obm && y >= obm->origin[1]);
    return (uint)((y - obm->origin[1]) / (coord_t)BLOCK_HEIGHT);
}

/**
 * Given world coordinates @a x, @a y, determine the objlink blockmap block
 * [x, y] it resides in. If the coordinates are outside the blockmap they
 * are clipped within valid range.
 *
 * @return  @c true if the coordinates specified had to be adjusted.
 */
static boolean toObjlinkBlockmapCell(objlinkblockmap_t* obm, uint coords[2],
    coord_t x, coord_t y)
{
    boolean adjusted = false;
    coord_t max[2];
    uint size[2];
    assert(obm);

    Gridmap_Size(obm->gridmap, size);
    max[0] = obm->origin[0] + size[0] * BLOCK_WIDTH;
    max[1] = obm->origin[1] + size[1] * BLOCK_HEIGHT;

    if(x < obm->origin[0])
    {
        coords[VX] = 0;
        adjusted = true;
    }
    else if(x >= max[0])
    {
        coords[VX] = size[0]-1;
        adjusted = true;
    }
    else
    {
        coords[VX] = toObjlinkBlockmapX(obm, x);
    }

    if(y < obm->origin[1])
    {
        coords[VY] = 0;
        adjusted = true;
    }
    else if(y >= max[1])
    {
        coords[VY] = size[1]-1;
        adjusted = true;
    }
    else
    {
        coords[VY] = toObjlinkBlockmapY(obm, y);
    }
    return adjusted;
}

static __inline void linkContact(objcontact_t* con, objcontact_t** list, uint index)
{
    con->next = list[index];
    list[index] = con;
}

static void linkContactToBspLeaf(objcontact_t* node, objtype_t type, uint index)
{
    linkContact(node, &bspLeafContacts[index].head[type], 0);
}

/**
 * Create a new objcontact. If there are none available in the list of
 * used objects a new one will be allocated and linked to the global list.
 */
static objcontact_t* allocObjContact(void)
{
    objcontact_t* con;
    if(!contCursor)
    {
        con = (objcontact_t *) Z_Malloc(sizeof *con, PU_APPSTATIC, NULL);

        // Link to the list of objcontact nodes.
        con->nextUsed = contFirst;
        contFirst = con;
    }
    else
    {
        con = contCursor;
        contCursor = contCursor->nextUsed;
    }
    con->obj = NULL;
    return con;
}

static objlink_t* allocObjlink(void)
{
    objlink_t* link;
    if(!objlinkCursor)
    {
        link = (objlink_t *) Z_Malloc(sizeof *link, PU_APPSTATIC, NULL);

        // Link the link to the global list.
        link->nextUsed = objlinkFirst;
        objlinkFirst = link;
    }
    else
    {
        link = objlinkCursor;
        objlinkCursor = objlinkCursor->nextUsed;
    }
    link->nextInBlock = NULL;
    link->obj = NULL;

    // Link it to the list of in-use objlinks.
    link->next = objlinks;
    objlinks = link;

    return link;
}

void R_InitObjlinkBlockmapForMap(void)
{
    GameMap* map = theMap;
    coord_t min[2], max[2];
    uint width, height;
    int i;

    // Determine the dimensions of the objlink blockmaps in blocks.
    GameMap_Bounds(map, min, max);
    width  = (uint)ceil((max[VX] - min[VX]) / (coord_t)BLOCK_WIDTH);
    height = (uint)ceil((max[VY] - min[VY]) / (coord_t)BLOCK_HEIGHT);

    // Create the blockmaps.
    for(i = 0; i < NUM_OBJ_TYPES; ++i)
    {
        objlinkblockmap_t* obm = chooseObjlinkBlockmap((objtype_t)i);
        obm->origin[0] = min[VX];
        obm->origin[1] = min[VY];
        obm->gridmap = Gridmap_New(width, height, sizeof(objlinkblock_t), PU_MAPSTATIC);
    }

    // Initialize obj => BspLeaf contact lists.
    bspLeafContacts = (objcontactlist_t *) Z_Calloc(sizeof *bspLeafContacts * NUM_BSPLEAFS, PU_MAPSTATIC, 0);
}

void R_DestroyObjlinkBlockmap(void)
{
    int i;
    for(i = 0; i < NUM_OBJ_TYPES; ++i)
    {
        objlinkblockmap_t* obm = chooseObjlinkBlockmap((objtype_t)i);
        if(!obm->gridmap) continue;
        Gridmap_Delete(obm->gridmap);
        obm->gridmap = NULL;
    }
    if(bspLeafContacts)
    {
        Z_Free(bspLeafContacts);
        bspLeafContacts = NULL;
    }
}

int clearObjlinkBlock(void* obj, void* parameters)
{
    DENG_UNUSED(parameters);

    objlinkblock_t* block = (objlinkblock_t*)obj;
    block->head = NULL;
    block->doneSpread = false;
    return false; // Continue iteration.
}

void R_ClearObjlinkBlockmap(objtype_t type)
{
    if(!VALID_OBJTYPE(type))
    {
#if _DEBUG
        Con_Error("R_ClearObjlinkBlockmap: Attempted with invalid type %i.", (int)type);
#endif
        return;
    }
    // Clear all the contact list heads and spread flags.
    Gridmap_Iterate(chooseObjlinkBlockmap(type)->gridmap, clearObjlinkBlock);
}

void R_ClearObjlinksForFrame(void)
{
    int i;
    for(i = 0; i < NUM_OBJ_TYPES; ++i)
    {
        objlinkblockmap_t* obm = chooseObjlinkBlockmap((objtype_t)i);
        if(!obm->gridmap) continue;
        R_ClearObjlinkBlockmap((objtype_t)i);
    }

    // Start reusing objlinks.
    objlinkCursor = objlinkFirst;
    objlinks = NULL;
}

void R_ObjlinkCreate(void* obj, objtype_t type)
{
    objlink_t* link = allocObjlink();
    link->obj = obj;
    link->type = type;
}

int RIT_LinkObjToBspLeaf(BspLeaf* bspLeaf, void* paramaters)
{
    const linkobjtobspleafparams_t* p = (linkobjtobspleafparams_t*) paramaters;
    objcontact_t* con = allocObjContact();

    con->obj = p->obj;
    // Link the contact list for this bspLeaf.
    linkContactToBspLeaf(con, p->type, GET_BSPLEAF_IDX(bspLeaf));

    return false; // Continue iteration.
}

/**
 * Attempt to spread the obj from the given contact from the source
 * BspLeaf and into the (relative) back BspLeaf.
 *
 * @param bspLeaf  BspLeaf to attempt to spread over to.
 * @param parameters  @ref contactfinderparams_t
 *
 * @return  Always @c true. (This function is also used as an iterator.)
 */
static void spreadInBspLeaf(BspLeaf* bspLeaf, void* paramaters)
{
    HEdge* hedge;
    if(!bspLeaf || !bspLeaf->hedge) return;

    hedge = bspLeaf->hedge;
    do
    {
        processSeg(hedge, paramaters);
    } while((hedge = hedge->next) != bspLeaf->hedge);
}

static void processSeg(HEdge *hedge, void *parameters)
{
    contactfinderparams_t *parms = (contactfinderparams_t *) parameters;
    DENG2_ASSERT(hedge && parms);

    BspLeaf *leaf = hedge->bspLeaf;
    BspLeaf *backLeaf = hedge->twin? hedge->twin->bspLeaf : 0;
    linkobjtobspleafparams_t loParams;
    coord_t distance;

    // There must be a back leaf to spread to.
    if(!backLeaf) return;

    // Which way does the spread go?
    if(!(leaf->validCount == validCount && backLeaf->validCount != validCount))
    {
        return; // Not eligible for spreading.
    }

    // Is the leaf on the back side outside the origin's AABB?
    if(backLeaf->aaBox.maxX <= parms->box[BOXLEFT]   ||
       backLeaf->aaBox.minX >= parms->box[BOXRIGHT]  ||
       backLeaf->aaBox.maxY <= parms->box[BOXBOTTOM] ||
       backLeaf->aaBox.minY >= parms->box[BOXTOP]) return;

    // Do not spread if the sector on the back side is closed with no height.
    if(backLeaf->sector && backLeaf->sector->SP_ceilheight <= backLeaf->sector->SP_floorheight) return;
    if(backLeaf->sector && leaf->sector &&
       (backLeaf->sector->SP_ceilheight  <= leaf->sector->SP_floorheight ||
        backLeaf->sector->SP_floorheight >= leaf->sector->SP_ceilheight)) return;

    // Too far from the object?
    distance = hedge->pointOnSide(parms->objOrigin) / hedge->length;
    if(fabs(distance) >= parms->objRadius) return;

    // Don't spread if the middle material covers the opening.
    if(hedge->lineDef)
    {
        // On which side of the line are we? (distance is from hedge to origin).
        byte lineSide = hedge->side ^ (distance < 0);
        LineDef *line = hedge->lineDef;
        Sector *frontSec  = lineSide == FRONT? leaf->sector : backLeaf->sector;
        Sector *backSec   = lineSide == FRONT? backLeaf->sector : leaf->sector;
        SideDef *frontDef = line->L_sidedef(lineSide);
        SideDef *backDef  = line->L_sidedef(lineSide^1);

        if(backSec && !backDef) return; // One-sided window.

        if(R_MiddleMaterialCoversOpening(line->flags, frontSec, backSec, frontDef, backDef,
                                         false /*do not ignore material opacity*/)) return;
    }

    // During next step, obj will continue spreading from there.
    backLeaf->validCount = validCount;

    // Link up a new contact with the back BSP leaf.
    loParams.obj  = parms->obj;
    loParams.type = parms->objType;
    RIT_LinkObjToBspLeaf(backLeaf, &loParams);

    spreadInBspLeaf(backLeaf, parms);
}

/**
 * Create a contact for the objlink in all the BspLeafs the linked obj is
 * contacting (tests done on bounding boxes and the BSP leaf spread test).
 *
 * @param oLink Ptr to objlink to find BspLeaf contacts for.
 */
static void findContacts(objlink_t* link)
{
    contactfinderparams_t cfParams;
    linkobjtobspleafparams_t loParams;
    coord_t radius;
    pvec3d_t origin;
    BspLeaf** ssecAdr;

    switch(link->type)
    {
#ifdef __CLIENT__
    case OT_LUMOBJ: {
        lumobj_t* lum = (lumobj_t*) link->obj;
        // Only omni lights spread.
        if(lum->type != LT_OMNI) return;

        origin = lum->origin;
        radius = LUM_OMNI(lum)->radius;
        ssecAdr = &lum->bspLeaf;
        break;
      }
#endif
    case OT_MOBJ: {
        mobj_t* mo = (mobj_t*) link->obj;

        origin = mo->origin;
        radius = R_VisualRadius(mo);
        ssecAdr = &mo->bspLeaf;
        break;
      }
    default:
        Con_Error("findContacts: Invalid objtype %i.", (int) link->type);
        exit(1); // Unreachable.
    }

    // Do the BSP leaf spread. Begin from the obj's own BspLeaf.
    (*ssecAdr)->validCount = ++validCount;

    cfParams.obj = link->obj;
    cfParams.objType = link->type;
    V3d_Copy(cfParams.objOrigin, origin);
    // Use a slightly smaller radius than what the obj really is.
    cfParams.objRadius = radius * .98f;

    cfParams.box[BOXLEFT]   = cfParams.objOrigin[VX] - radius;
    cfParams.box[BOXRIGHT]  = cfParams.objOrigin[VX] + radius;
    cfParams.box[BOXBOTTOM] = cfParams.objOrigin[VY] - radius;
    cfParams.box[BOXTOP]    = cfParams.objOrigin[VY] + radius;

    // Always contact the obj's own BspLeaf.
    loParams.obj = link->obj;
    loParams.type = link->type;
    RIT_LinkObjToBspLeaf(*ssecAdr, &loParams);

    spreadInBspLeaf(*ssecAdr, &cfParams);
}

/**
 * Spread contacts in the object => BspLeaf objlink blockmap to all
 * other BspLeafs within the block.
 *
 * @param obm        Objlink blockmap.
 * @param bspLeaf    BspLeaf to spread the contacts of.
 * @param maxRadius  Maximum radius for the spread.
 */
void R_ObjlinkBlockmapSpreadInBspLeaf(objlinkblockmap_t* obm, const BspLeaf* bspLeaf, float maxRadius)
{
    uint minBlock[2], maxBlock[2], x, y;
    objlink_t* iter;
    assert(obm);

    if(!bspLeaf) return; // Wha?

    toObjlinkBlockmapCell(obm, minBlock, bspLeaf->aaBox.minX - maxRadius,
                                         bspLeaf->aaBox.minY - maxRadius);

    toObjlinkBlockmapCell(obm, maxBlock, bspLeaf->aaBox.maxX + maxRadius,
                                         bspLeaf->aaBox.maxY + maxRadius);

    for(y = minBlock[1]; y <= maxBlock[1]; ++y)
        for(x = minBlock[0]; x <= maxBlock[0]; ++x)
        {
            objlinkblock_t* block = (objlinkblock_t *) Gridmap_CellXY(obm->gridmap, x, y, true/*can allocate a block*/);
            if(block->doneSpread) continue;

            iter = block->head;
            while(iter)
            {
                findContacts(iter);
                iter = iter->nextInBlock;
            }
            block->doneSpread = true;
        }
}

static __inline float maxRadius(objtype_t type)
{
#ifdef __CLIENT__
    assert(VALID_OBJTYPE(type));
    if(type == OT_MOBJ) return DDMOBJ_RADIUS_MAX;
    // Must be OT_LUMOBJ
    return loMaxRadius;
#else
    return DDMOBJ_RADIUS_MAX;
#endif
}

void R_InitForBspLeaf(BspLeaf* bspLeaf)
{
    int i;
BEGIN_PROF( PROF_OBJLINK_SPREAD );

    for(i = 0; i < NUM_OBJ_TYPES; ++i)
    {
        objlinkblockmap_t* obm = chooseObjlinkBlockmap((objtype_t)i);
        R_ObjlinkBlockmapSpreadInBspLeaf(obm, bspLeaf, maxRadius((objtype_t)i));
    }

END_PROF( PROF_OBJLINK_SPREAD );
}

/// @pre  Coordinates held by @a blockXY are within valid range.
static void linkObjlinkInBlockmap(objlinkblockmap_t* obm, objlink_t* link, uint blockXY[2])
{
    objlinkblock_t* block;
    if(!obm || !link || !blockXY) return; // Wha?
    block = (objlinkblock_t *) Gridmap_CellXY(obm->gridmap, blockXY[0], blockXY[1], true/*can allocate a block*/);
    link->nextInBlock = block->head;
    block->head = link;
}

void R_LinkObjs(void)
{
    objlinkblockmap_t* obm;
    objlink_t* link;
    uint block[2];
    pvec3d_t origin;

BEGIN_PROF( PROF_OBJLINK_LINK );

    // Link objlinks into the objlink blockmap.
    link = objlinks;
    while(link)
    {
        switch(link->type)
        {
#ifdef __CLIENT__
        case OT_LUMOBJ:     origin = ((lumobj_t*)link->obj)->origin; break;
#endif
        case OT_MOBJ:       origin = ((mobj_t*)link->obj)->origin; break;
        default:
            Con_Error("R_LinkObjs: Invalid objtype %i.", (int) link->type);
            exit(1); // Unreachable.
        }

        obm = chooseObjlinkBlockmap(link->type);
        if(!toObjlinkBlockmapCell(obm, block, origin[VX], origin[VY]))
        {
            linkObjlinkInBlockmap(obm, link, block);
        }
        link = link->next;
    }

END_PROF( PROF_OBJLINK_LINK );
}

void R_InitForNewFrame(void)
{
#ifdef DD_PROFILE
    static int i;

    if(++i > 40)
    {
        i = 0;
        PRINT_PROF(PROF_OBJLINK_SPREAD);
        PRINT_PROF(PROF_OBJLINK_LINK);
    }
#endif

    // Start reusing nodes from the first one in the list.
    contCursor = contFirst;
    if(bspLeafContacts)
        memset(bspLeafContacts, 0, NUM_BSPLEAFS * sizeof *bspLeafContacts);
}

int R_IterateBspLeafContacts2(BspLeaf* bspLeaf, objtype_t type,
    int (*callback) (void* object, void* paramaters), void* paramaters)
{
    objcontact_t* con = bspLeafContacts[GET_BSPLEAF_IDX(bspLeaf)].head[type];
    int result = false; // Continue iteration.
    while(con)
    {
        result = callback(con->obj, paramaters);
        if(result) break;
        con = con->next;
    }
    return result;
}

int R_IterateBspLeafContacts(BspLeaf* bspLeaf, objtype_t type,
    int (*callback) (void* object, void* paramaters))
{
    return R_IterateBspLeafContacts2(bspLeaf, type, callback, NULL/*no paramaters*/);
}
