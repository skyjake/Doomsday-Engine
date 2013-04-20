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

#include <cmath>

#include "de_base.h"
#include "de_console.h"
#include "de_render.h"
#include "de_graphics.h"
#include "de_misc.h"
#include "de_play.h"
#include "de_defs.h"

#include "gridmap.h"
#include "map/gamemap.h"

#include "map/p_objlink.h"

#define BLOCK_WIDTH                 (128)
#define BLOCK_HEIGHT                (128)

BEGIN_PROF_TIMERS()
  PROF_OBJLINK_SPREAD,
  PROF_OBJLINK_LINK
END_PROF_TIMERS()

struct objlink_t
{
    objlink_t *nextInBlock; /// Next in the same obj block, or NULL.
    objlink_t *nextUsed;
    objlink_t *next; /// Next in list of ALL objlinks.
    objtype_t type;
    void *obj;
};

struct objlinkblock_t
{
    objlink_t *head;
    /// Used to prevent repeated per-frame processing of a block.
    bool doneSpread;
};

struct objlinkblockmap_t
{
    coord_t origin[2]; /// Origin of the blockmap in world coordinates [x,y].
    Gridmap *gridmap;
};

struct contactfinderparams_t
{
    void *obj;
    objtype_t objType;
    coord_t objOrigin[3];
    coord_t objRadius;
    coord_t box[4];
};

struct objcontact_t
{
    objcontact_t *next; /// Next in the BSP leaf.
    objcontact_t *nextUsed; /// Next used contact.
    void *obj;
};

struct objcontactlist_t
{
    objcontact_t *head[NUM_OBJ_TYPES];
};

static objlink_t *objlinks;
static objlink_t *objlinkFirst, *objlinkCursor;

// Each objlink type gets its own blockmap.
static objlinkblockmap_t blockmaps[NUM_OBJ_TYPES];

// List of unused and used contacts.
static objcontact_t *contFirst, *contCursor;

// List of contacts for each BSP leaf.
static objcontactlist_t *bspLeafContacts;

static void spreadInBspLeaf(BspLeaf *bspLeaf, void *parameters);

static inline objlinkblockmap_t *chooseObjlinkBlockmap(objtype_t type)
{
    DENG_ASSERT(VALID_OBJTYPE(type));
    return blockmaps + (int)type;
}

static inline uint toObjlinkBlockmapX(objlinkblockmap_t *obm, coord_t x)
{
    DENG_ASSERT(obm != 0 && x >= obm->origin[0]);
    return uint( (x - obm->origin[0]) / coord_t( BLOCK_WIDTH ) );
}

static inline uint toObjlinkBlockmapY(objlinkblockmap_t *obm, coord_t y)
{
    DENG_ASSERT(obm != 0 && y >= obm->origin[1]);
    return uint( (y - obm->origin[1]) / coord_t( BLOCK_HEIGHT ) );
}

/**
 * Given world coordinates @a x, @a y, determine the objlink blockmap block
 * [x, y] it resides in. If the coordinates are outside the blockmap they
 * are clipped within valid range.
 *
 * @return  @c true if the coordinates specified had to be adjusted.
 */
static bool toObjlinkBlockmapCell(objlinkblockmap_t *obm, uint coords[2],
    coord_t x, coord_t y)
{
    DENG_ASSERT(obm != 0 && coords != 0);

    uint size[2];
    Gridmap_Size(obm->gridmap, size);

    coord_t max[2] = { obm->origin[0] + size[0] * BLOCK_WIDTH,
                       obm->origin[1] + size[1] * BLOCK_HEIGHT };

    bool adjusted = false;
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

static inline void linkContact(objcontact_t *con, objcontact_t **list, uint index)
{
    con->next = list[index];
    list[index] = con;
}

static void linkContactToBspLeaf(objcontact_t *node, objtype_t type, uint index)
{
    linkContact(node, &bspLeafContacts[index].head[type], 0);
}

/**
 * Create a new objcontact. If there are none available in the list of
 * used objects a new one will be allocated and linked to the global list.
 */
static objcontact_t *allocObjContact()
{
    objcontact_t *con;
    if(!contCursor)
    {
        con = (objcontact_t *) Z_Malloc(sizeof *con, PU_APPSTATIC, 0);

        // Link to the list of objcontact nodes.
        con->nextUsed = contFirst;
        contFirst = con;
    }
    else
    {
        con = contCursor;
        contCursor = contCursor->nextUsed;
    }
    con->obj = 0;
    return con;
}

static objlink_t *allocObjlink()
{
    objlink_t *link;
    if(!objlinkCursor)
    {
        link = (objlink_t *) Z_Malloc(sizeof *link, PU_APPSTATIC, 0);

        // Link the link to the global list.
        link->nextUsed = objlinkFirst;
        objlinkFirst = link;
    }
    else
    {
        link = objlinkCursor;
        objlinkCursor = objlinkCursor->nextUsed;
    }
    link->nextInBlock = 0;
    link->obj = 0;

    // Link it to the list of in-use objlinks.
    link->next = objlinks;
    objlinks = link;

    return link;
}

void R_InitObjlinkBlockmapForMap()
{
    // Determine the dimensions of the objlink blockmaps in blocks.
    coord_t min[2], max[2];
    theMap->bounds(min, max);

    uint width  = uint( de::ceil((max[VX] - min[VX]) / coord_t( BLOCK_WIDTH  )) );
    uint height = uint( de::ceil((max[VY] - min[VY]) / coord_t( BLOCK_HEIGHT )) );

    // Create the blockmaps.
    for(int i = 0; i < NUM_OBJ_TYPES; ++i)
    {
        objlinkblockmap_t *obm = chooseObjlinkBlockmap(objtype_t( i ));
        obm->origin[0] = min[VX];
        obm->origin[1] = min[VY];
        obm->gridmap = Gridmap_New(width, height, sizeof(objlinkblock_t), PU_MAPSTATIC);
    }

    // Initialize obj => BspLeaf contact lists.
    bspLeafContacts = (objcontactlist_t *) Z_Calloc(sizeof *bspLeafContacts * theMap->bspLeafCount(),
                                                    PU_MAPSTATIC, 0);
}

void R_DestroyObjlinkBlockmap()
{
    for(int i = 0; i < NUM_OBJ_TYPES; ++i)
    {
        objlinkblockmap_t *obm = chooseObjlinkBlockmap(objtype_t( i ));
        if(!obm->gridmap) continue;

        Gridmap_Delete(obm->gridmap);
        obm->gridmap = 0;
    }

    if(bspLeafContacts)
    {
        Z_Free(bspLeafContacts);
        bspLeafContacts = 0;
    }
}

int clearObjlinkBlock(void *obj, void *parameters)
{
    DENG_UNUSED(parameters);

    objlinkblock_t *block = (objlinkblock_t *)obj;
    block->head = 0;
    block->doneSpread = false;

    return false; // Continue iteration.
}

void R_ClearObjlinkBlockmap(objtype_t type)
{
    DENG_ASSERT(VALID_OBJTYPE(type));
    // Clear all the contact list heads and spread flags.
    Gridmap_Iterate(chooseObjlinkBlockmap(type)->gridmap, clearObjlinkBlock);
}

void R_ClearObjlinksForFrame()
{
    for(int i = 0; i < NUM_OBJ_TYPES; ++i)
    {
        objlinkblockmap_t *obm = chooseObjlinkBlockmap(objtype_t(i));
        if(!obm->gridmap)
            continue;

        R_ClearObjlinkBlockmap(objtype_t(i));
    }

    // Start reusing objlinks.
    objlinkCursor = objlinkFirst;
    objlinks = NULL;
}

void R_ObjlinkCreate(void *obj, objtype_t type)
{
    objlink_t *link = allocObjlink();
    link->obj = obj;
    link->type = type;
}

int RIT_LinkObjToBspLeaf(BspLeaf *bspLeaf, void *parameters)
{
    linkobjtobspleafparams_t const *p = (linkobjtobspleafparams_t *) parameters;
    objcontact_t *con = allocObjContact();

    con->obj = p->obj;
    // Link the contact list for this bspLeaf.
    linkContactToBspLeaf(con, p->type, theMap->bspLeafIndex(bspLeaf));

    return false; // Continue iteration.
}

static void processHEdge(HEdge *hedge, void *parameters)
{
    contactfinderparams_t *parms = (contactfinderparams_t *) parameters;
    DENG2_ASSERT(hedge != 0 && parms != 0);

    // There must be a back leaf to spread to.
    if(!hedge->hasTwin()) return;

    BspLeaf *leaf     = &hedge->bspLeaf();
    BspLeaf *backLeaf = &hedge->twin().bspLeaf();

    // Which way does the spread go?
    if(!(leaf->validCount() == validCount && backLeaf->validCount() != validCount))
    {
        return; // Not eligible for spreading.
    }

    // Is the leaf on the back side outside the origin's AABB?
    if(backLeaf->aaBox().maxX <= parms->box[BOXLEFT]   ||
       backLeaf->aaBox().minX >= parms->box[BOXRIGHT]  ||
       backLeaf->aaBox().maxY <= parms->box[BOXBOTTOM] ||
       backLeaf->aaBox().minY >= parms->box[BOXTOP])
        return;

    // Too far from the object?
    coord_t distance = hedge->pointOnSide(parms->objOrigin) / hedge->length();
    if(de::abs(distance) >= parms->objRadius)
        return;

    // Do not spread if the sector on the back side is closed with no height.
    if(backLeaf->hasSector())
    {
        Sector const &frontSec = leaf->sector();
        Sector const &backSec  = backLeaf->sector();

        if(backSec.ceiling().height() <= backSec.floor().height())
            return;

        if(leaf->hasSector() &&
           (backSec.ceiling().height() <= frontSec.floor().height() ||
            backSec.floor().height() >= frontSec.ceiling().height()))
            return;
    }

    // Don't spread if the middle material covers the opening.
    if(hedge->hasLineSide())
    {
        // On which side of the line are we? (distance is from hedge to origin).
        Line::Side &side = hedge->line().side(hedge->lineSideId() ^ (distance < 0));

        Sector *frontSec = side.isFront()? leaf->sectorPtr() : backLeaf->sectorPtr();
        Sector *backSec  = side.isFront()? backLeaf->sectorPtr() : leaf->sectorPtr();

        // One-way window?
        if(backSec && !side.back().hasSections())
            return;

        if(R_MiddleMaterialCoversOpening(side, frontSec, backSec))
            return;
    }

    // During next step, obj will continue spreading from there.
    backLeaf->setValidCount(validCount);

    // Link up a new contact with the back BSP leaf.
    linkobjtobspleafparams_t loParms;
    loParms.obj  = parms->obj;
    loParms.type = parms->objType;
    RIT_LinkObjToBspLeaf(backLeaf, &loParms);

    spreadInBspLeaf(backLeaf, parms);
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
static void spreadInBspLeaf(BspLeaf *bspLeaf, void *parameters)
{
    if(!bspLeaf || !bspLeaf->firstHEdge()) return;

    HEdge *base = bspLeaf->firstHEdge();
    HEdge *hedge = base;
    do
    {
        processHEdge(hedge, parameters);
    } while((hedge = &hedge->next()) != base);
}

/**
 * Create a contact for the objlink in all the BspLeafs the linked obj is
 * contacting (tests done on bounding boxes and the BSP leaf spread test).
 *
 * @param oLink Ptr to objlink to find BspLeaf contacts for.
 */
static void findContacts(objlink_t *link)
{
    DENG_ASSERT(link != 0);

    coord_t radius;
    pvec3d_t origin;
    BspLeaf **bspLeafAdr;

    switch(link->type)
    {
#ifdef __CLIENT__
    case OT_LUMOBJ: {
        lumobj_t *lum = (lumobj_t *) link->obj;
        // Only omni lights spread.
        if(lum->type != LT_OMNI) return;

        origin = lum->origin;
        radius = LUM_OMNI(lum)->radius;
        bspLeafAdr = &lum->bspLeaf;
        break; }
#endif
    case OT_MOBJ: {
        mobj_t *mo = (mobj_t *) link->obj;

        origin = mo->origin;
        radius = R_VisualRadius(mo);
        bspLeafAdr = &mo->bspLeaf;
        break; }

    default:
        DENG_ASSERT(false);
    }

    // Do the BSP leaf spread. Begin from the obj's own BspLeaf.
    (*bspLeafAdr)->setValidCount(++validCount);

    contactfinderparams_t cfParms;
    cfParms.obj            = link->obj;
    cfParms.objType        = link->type;
    V3d_Copy(cfParms.objOrigin, origin);
    // Use a slightly smaller radius than what the obj really is.
    cfParms.objRadius      = radius * .98f;

    cfParms.box[BOXLEFT]   = cfParms.objOrigin[VX] - radius;
    cfParms.box[BOXRIGHT]  = cfParms.objOrigin[VX] + radius;
    cfParms.box[BOXBOTTOM] = cfParms.objOrigin[VY] - radius;
    cfParms.box[BOXTOP]    = cfParms.objOrigin[VY] + radius;

    // Always contact the obj's own BspLeaf.
    linkobjtobspleafparams_t loParms;
    loParms.obj  = link->obj;
    loParms.type = link->type;
    RIT_LinkObjToBspLeaf(*bspLeafAdr, &loParms);

    spreadInBspLeaf(*bspLeafAdr, &cfParms);
}

/**
 * Spread contacts in the object => BspLeaf objlink blockmap to all
 * other BspLeafs within the block.
 *
 * @param obm        Objlink blockmap.
 * @param bspLeaf    BspLeaf to spread the contacts of.
 * @param maxRadius  Maximum radius for the spread.
 */
void R_ObjlinkBlockmapSpreadInBspLeaf(objlinkblockmap_t *obm, BspLeaf const *bspLeaf,
    float maxRadius)
{
    DENG_ASSERT(obm != 0);
    if(!bspLeaf) return; // Wha?

    uint minBlock[2];
    toObjlinkBlockmapCell(obm, minBlock, bspLeaf->aaBox().minX - maxRadius,
                                         bspLeaf->aaBox().minY - maxRadius);

    uint maxBlock[2];
    toObjlinkBlockmapCell(obm, maxBlock, bspLeaf->aaBox().maxX + maxRadius,
                                         bspLeaf->aaBox().maxY + maxRadius);

    for(uint y = minBlock[1]; y <= maxBlock[1]; ++y)
    for(uint x = minBlock[0]; x <= maxBlock[0]; ++x)
    {
        objlinkblock_t *block = (objlinkblock_t *) Gridmap_CellXY(obm->gridmap, x, y, true/*can allocate a block*/);
        if(block->doneSpread) continue;

        objlink_t *iter = block->head;
        while(iter)
        {
            findContacts(iter);
            iter = iter->nextInBlock;
        }
        block->doneSpread = true;
    }
}

static inline float maxRadius(objtype_t type)
{
    DENG_ASSERT(VALID_OBJTYPE(type));
#ifdef __CLIENT__
    if(type == OT_LUMOBJ) return loMaxRadius;
#endif
    // Must be OT_MOBJ
    return DDMOBJ_RADIUS_MAX;
}

void R_InitForBspLeaf(BspLeaf* bspLeaf)
{
BEGIN_PROF( PROF_OBJLINK_SPREAD );

    for(int i = 0; i < NUM_OBJ_TYPES; ++i)
    {
        objlinkblockmap_t *obm = chooseObjlinkBlockmap(objtype_t(i));
        R_ObjlinkBlockmapSpreadInBspLeaf(obm, bspLeaf, maxRadius(objtype_t(i)));
    }

END_PROF( PROF_OBJLINK_SPREAD );
}

/// @pre  Coordinates held by @a blockXY are within valid range.
static void linkObjlinkInBlockmap(objlinkblockmap_t *obm, objlink_t *link, uint blockXY[2])
{
    if(!obm || !link || !blockXY) return; // Wha?

    objlinkblock_t *block = (objlinkblock_t *) Gridmap_CellXY(obm->gridmap, blockXY[0], blockXY[1], true/*can allocate a block*/);
    link->nextInBlock = block->head;
    block->head = link;
}

void R_LinkObjs()
{
    uint block[2];
    pvec3d_t origin;

BEGIN_PROF( PROF_OBJLINK_LINK );

    // Link objlinks into the objlink blockmap.
    objlink_t *link = objlinks;
    while(link)
    {
        switch(link->type)
        {
#ifdef __CLIENT__
        case OT_LUMOBJ:     origin = ((lumobj_t *)link->obj)->origin; break;
#endif
        case OT_MOBJ:       origin = ((mobj_t *)link->obj)->origin; break;

        default: DENG_ASSERT(false); // Invalid type.
        }

        objlinkblockmap_t *obm = chooseObjlinkBlockmap(link->type);
        if(!toObjlinkBlockmapCell(obm, block, origin[VX], origin[VY]))
        {
            linkObjlinkInBlockmap(obm, link, block);
        }
        link = link->next;
    }

END_PROF( PROF_OBJLINK_LINK );
}

void R_InitForNewFrame()
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
    {
        std::memset(bspLeafContacts, 0, theMap->bspLeafCount() * sizeof *bspLeafContacts);
    }
}

int R_IterateBspLeafContacts2(BspLeaf *bspLeaf, objtype_t type,
    int (*callback) (void *object, void *parameters), void *parameters)
{
    DENG_ASSERT(VALID_OBJTYPE(type));
    objcontactlist_t const &conList = bspLeafContacts[theMap->bspLeafIndex(bspLeaf)];

    for(objcontact_t *con = conList.head[type]; con; con = con->next)
    {
        int result = callback(con->obj, parameters);
        if(result) return result;
    }
    return false; // Continue iteration.
}

int R_IterateBspLeafContacts(BspLeaf *bspLeaf, objtype_t type,
    int (*callback) (void *object, void *parameters))
{
    return R_IterateBspLeafContacts2(bspLeaf, type, callback, NULL/*no parameters*/);
}
