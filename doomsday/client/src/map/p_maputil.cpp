/** @file p_maputil.cpp
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

/**
 * Map Utility Routines
 */

#include <math.h>

#include "de_base.h"
#include "de_console.h"
#include "de_play.h"
#include "de_misc.h"

#include "render/r_main.h" // validCount

#define ORDER(x,y,a,b)  ( (x)<(y)? ((a)=(x),(b)=(y)) : ((b)=(x),(a)=(y)) )

// Linkstore is list of pointers gathered when iterating stuff.
// This is pretty much the only way to avoid *all* potential problems
// caused by callback routines behaving badly (moving or destroying
// mobjs). The idea is to get a snapshot of all the objects being
// iterated before any callbacks are called. The hardcoded limit is
// a drag, but I'd like to see you iterating 2048 mobjs/lines in one block.

#define MAXLINKED           2048
#define DO_LINKS(it, end, _Type)   { \
    for(it = linkstore; it < end; it++) \
    { \
        result = callback(reinterpret_cast<_Type>(*it), parameters); \
        if(result) break; \
    } \
}

#undef P_TraceLOS
DENG_EXTERN_C const divline_t* P_TraceLOS(void)
{
    static divline_t emptyLOS;
    if(theMap)
    {
        return GameMap_TraceLOS(theMap);
    }
    return &emptyLOS;
}

#undef P_TraceOpening
DENG_EXTERN_C TraceOpening const *P_TraceOpening(void)
{
    static TraceOpening zeroOpening;
    if(theMap)
    {
        return GameMap_TraceOpening(theMap);
    }
    return &zeroOpening;
}

#undef P_SetTraceOpening
DENG_EXTERN_C void P_SetTraceOpening(LineDef* lineDef)
{
    if(!theMap)
    {
        DEBUG_Message(("Warning: P_SetTraceOpening() attempted with no current map, ignoring."));
        return;
    }
    /// @todo Do not assume linedef is from the CURRENT map.
    GameMap_SetTraceOpening(theMap, lineDef);
}

#undef P_BspLeafAtPoint
DENG_EXTERN_C BspLeaf* P_BspLeafAtPoint(coord_t const point[])
{
    if(!theMap) return NULL;
    return GameMap_BspLeafAtPoint(theMap, point);
}

#undef P_BspLeafAtPointXY
DENG_EXTERN_C BspLeaf* P_BspLeafAtPointXY(coord_t x, coord_t y)
{
    if(!theMap) return NULL;
    return GameMap_BspLeafAtPointXY(theMap, x, y);
}

boolean P_IsPointXYInBspLeaf(coord_t x, coord_t y, BspLeaf const *bspLeaf)
{
    if(!bspLeaf || !bspLeaf->firstHEdge()) return false; // I guess?

    HEdge const *base = bspLeaf->firstHEdge();
    HEdge const *hedge = base;
    do
    {
        HEdge *next = hedge->next;

        Vertex *vi = hedge->HE_v1;
        Vertex *vj = next->HE_v1;

        if(((vi->origin()[VY] - y) * (vj->origin()[VX] - vi->origin()[VX]) -
            (vi->origin()[VX] - x) * (vj->origin()[VY] - vi->origin()[VY])) < 0)
        {
            // Outside the BSP leaf's edges.
            return false;
        }
    } while((hedge = hedge->next) != base);

    return true;
}

boolean P_IsPointInBspLeaf(coord_t const point[], BspLeaf const *bspLeaf)
{
    return P_IsPointXYInBspLeaf(point[VX], point[VY], bspLeaf);
}

boolean P_IsPointXYInSector(coord_t x, coord_t y, Sector const *sector)
{
    if(!sector) return false; // I guess?

    /// @todo Do not assume @a sector is from the current map.
    BspLeaf *bspLeaf = GameMap_BspLeafAtPointXY(theMap, x, y);
    if(bspLeaf->sectorPtr() != sector) return false;

    return P_IsPointXYInBspLeaf(x, y, bspLeaf);
}

boolean P_IsPointInSector(coord_t const point[], Sector const *sector)
{
    return P_IsPointXYInSector(point[VX], point[VY], sector);
}

/**
 * Two links to update:
 * 1) The link to us from the previous node (sprev, always set) will
 *    be modified to point to the node following us.
 * 2) If there is a node following us, set its sprev pointer to point
 *    to the pointer that points back to it (our sprev, just modified).
 */
boolean P_UnlinkMobjFromSector(mobj_t* mo)
{
    if(!IS_SECTOR_LINKED(mo))
        return false;

    if((*mo->sPrev = mo->sNext))
        mo->sNext->sPrev = mo->sPrev;

    // Not linked any more.
    mo->sNext = NULL;
    mo->sPrev = NULL;

    return true;
}

/**
 * Unlinks a mobj from everything it has been linked to.
 *
 * @param mo            Ptr to the mobj to be unlinked.
 * @return              DDLINK_* flags denoting what the mobj was unlinked
 *                      from (in case we need to re-link).
 */
#undef P_MobjUnlink
DENG_EXTERN_C int P_MobjUnlink(mobj_t* mo)
{
    int links = 0;

    if(P_UnlinkMobjFromSector(mo))
        links |= DDLINK_SECTOR;
    if(P_UnlinkMobjFromBlockmap(mo))
        links |= DDLINK_BLOCKMAP;
    if(!P_UnlinkMobjFromLineDefs(mo))
        links |= DDLINK_NOLINE;

    return links;
}

/**
 * Unlinks the mobj from all the lines it's been linked to. Can be called
 * without checking that the list does indeed contain lines.
 */
boolean GameMap_UnlinkMobjFromLineDefs(GameMap* map, mobj_t* mo)
{
    linknode_t* tn;
    nodeindex_t nix;
    assert(map);

    // Try unlinking from lines.
    if(!mo || !mo->lineRoot)
        return false; // A zero index means it's not linked.

    // Unlink from each line.
    tn = map->mobjNodes.nodes;
    for(nix = tn[mo->lineRoot].next; nix != mo->lineRoot;
        nix = tn[nix].next)
    {
        // Data is the linenode index that corresponds this mobj.
        NP_Unlink((&map->lineNodes), tn[nix].data);
        // We don't need these nodes any more, mark them as unused.
        // Dismissing is a macro.
        NP_Dismiss((&map->lineNodes), tn[nix].data);
        NP_Dismiss((&map->mobjNodes), nix);
    }

    // The mobj no longer has a line ring.
    NP_Dismiss((&map->mobjNodes), mo->lineRoot);
    mo->lineRoot = 0;

    return true;
}

/**
 * @note Caller must ensure a mobj is linked only once to any given linedef.
 *
 * @param map  GameMap instance.
 * @param mo  Mobj to be linked.
 * @param lineDef  LineDef to link the mobj to.
 */
void GameMap_LinkMobjToLineDef(GameMap* map, mobj_t* mo, LineDef* lineDef)
{
    nodeindex_t nodeIndex;
    int lineDefIndex;
    assert(map);

    if(!mo) return;

    lineDefIndex = GameMap_LineDefIndex(map, lineDef);
    if(lineDefIndex < 0) return;

    // Add a node to the mobj's ring.
    nodeIndex = NP_New(&map->mobjNodes, lineDef);
    NP_Link(&map->mobjNodes, nodeIndex, mo->lineRoot);

    // Add a node to the line's ring. Also store the linenode's index
    // into the mobjring's node, so unlinking is easy.
    nodeIndex = map->mobjNodes.nodes[nodeIndex].data = NP_New(&map->lineNodes, mo);
    NP_Link(&map->lineNodes, nodeIndex, map->lineLinks[lineDefIndex]);
}

typedef struct {
    GameMap *map;
    mobj_t *mo;
    AABoxd box;
} linelinker_data_t;

/**
 * The given line might cross the mobj. If necessary, link the mobj into
 * the line's mobj link ring.
 */
int PIT_LinkToLines(LineDef *ld, void *parameters)
{
    linelinker_data_t *p = reinterpret_cast<linelinker_data_t *>(parameters);
    DENG_ASSERT(p);

    // Do the bounding boxes intercept?
    if(p->box.minX >= ld->aaBox.maxX ||
       p->box.minY >= ld->aaBox.maxY ||
       p->box.maxX <= ld->aaBox.minX ||
       p->box.maxY <= ld->aaBox.minY) return false;

    // Line does not cross the mobj's bounding box?
    if(ld->boxOnSide(p->box)) return false;

    // One sided lines will not be linked to because a mobj can't legally cross one.
    if(!ld->L_frontsidedef || !ld->L_backsidedef) return false;

    GameMap_LinkMobjToLineDef(p->map, p->mo, ld);
    return false;
}

/**
 * @note Caller must ensure that the mobj is currently unlinked.
 */
void GameMap_LinkMobjToLineDefs(GameMap* map, mobj_t* mo)
{
    linelinker_data_t p;
    vec2d_t point;
    assert(map);

    // Get a new root node.
    mo->lineRoot = NP_New(&map->mobjNodes, NP_ROOT_NODE);

    // Set up a line iterator for doing the linking.
    p.map = map;
    p.mo = mo;
    V2d_Set(point, mo->origin[VX] - mo->radius, mo->origin[VY] - mo->radius);
    V2d_InitBox(p.box.arvec2, point);
    V2d_Set(point, mo->origin[VX] + mo->radius, mo->origin[VY] + mo->radius);
    V2d_AddToBox(p.box.arvec2, point);

    validCount++;
    P_AllLinesBoxIterator(&p.box, PIT_LinkToLines, (void*)&p);
}

/**
 * Links a mobj into both a block and a BSP leaf based on it's (x,y).
 * Sets mobj->bspLeaf properly. Calling with flags==0 only updates
 * the BspLeaf pointer. Can be called without unlinking first.
 */
#undef P_MobjLink
DENG_EXTERN_C void P_MobjLink(mobj_t *mo, byte flags)
{
    // Link into the sector.
    mo->bspLeaf = P_BspLeafAtPoint(mo->origin);

    if(flags & DDLINK_SECTOR)
    {
        // Unlink from the current sector, if any.
        Sector &sec = mo->bspLeaf->sector();

        if(mo->sPrev)
            P_UnlinkMobjFromSector(mo);

        // Link the new mobj to the head of the list.
        // Prev pointers point to the pointer that points back to us.
        // (Which practically disallows traversing the list backwards.)

        if((mo->sNext = sec.mobjList))
            mo->sNext->sPrev = &mo->sNext;

        *(mo->sPrev = &sec.mobjList) = mo;
    }

    // Link into blockmap?
    if(flags & DDLINK_BLOCKMAP)
    {
        // Unlink from the old block, if any.
        P_UnlinkMobjFromBlockmap(mo);
        P_LinkMobjInBlockmap(mo);
    }

    // Link into lines.
    if(!(flags & DDLINK_NOLINE))
    {
        // Unlink from any existing lines.
        P_UnlinkMobjFromLineDefs(mo);

        // Link to all contacted lines.
        P_LinkMobjToLineDefs(mo);
    }

    // If this is a player - perform additional tests to see if they have
    // entered or exited the void.
    if(mo->dPlayer && mo->dPlayer->mo)
    {
        ddplayer_t *player = mo->dPlayer;
        Sector &sec = player->mo->bspLeaf->sector();

        player->inVoid = true;
        if(P_IsPointXYInSector(player->mo->origin[VX], player->mo->origin[VY], &sec) &&
           (player->mo->origin[VZ] <  sec.SP_ceilvisheight + 4 &&
            player->mo->origin[VZ] >= sec.SP_floorvisheight))
            player->inVoid = false;
    }
}

/**
 * The callback function will be called once for each line that crosses
 * trough the object. This means all the lines will be two-sided.
 */
int GameMap_MobjLinesIterator(GameMap *map, mobj_t *mo,
    int (*callback) (LineDef *, void *), void* parameters)
{
    DENG_ASSERT(map);

    void *linkstore[MAXLINKED];
    void **end = linkstore, **it;
    int result = false;

    linknode_t *tn = map->mobjNodes.nodes;
    if(mo->lineRoot)
    {
        nodeindex_t nix;
        for(nix = tn[mo->lineRoot].next; nix != mo->lineRoot;
            nix = tn[nix].next)
            *end++ = tn[nix].ptr;

        DO_LINKS(it, end, LineDef*);
    }
    return result;
}

/**
 * Increment validCount before calling this routine. The callback function
 * will be called once for each sector the mobj is touching (totally or
 * partly inside). This is not a 3D check; the mobj may actually reside
 * above or under the sector.
 */
int GameMap_MobjSectorsIterator(GameMap *map, mobj_t *mo,
    int (*callback) (Sector *, void *), void *parameters)
{
    DENG_ASSERT(map);

    int result = false;
    void *linkstore[MAXLINKED];
    void **end = linkstore, **it;

    nodeindex_t nix;
    linknode_t *tn = map->mobjNodes.nodes;

    // Always process the mobj's own sector first.
    Sector &ownSec = mo->bspLeaf->sector();
    *end++ = &ownSec;
    ownSec.validCount = validCount;

    // Any good lines around here?
    if(mo->lineRoot)
    {
        for(nix = tn[mo->lineRoot].next; nix != mo->lineRoot;
            nix = tn[nix].next)
        {
            LineDef *ld = (LineDef *) tn[nix].ptr;

            // All these lines are two-sided. Try front side.
            Sector &frontSec = ld->frontSector();
            if(frontSec.validCount != validCount)
            {
                *end++ = &frontSec;
                frontSec.validCount = validCount;
            }

            // And then the back side.
            if(ld->L_backsidedef)
            {
                Sector &backSec = ld->backSector();
                if(backSec.validCount != validCount)
                {
                    *end++ = &backSec;
                    backSec.validCount = validCount;
                }
            }
        }
    }

    DO_LINKS(it, end, Sector *);
    return result;
}

int GameMap_LineMobjsIterator(GameMap* map, LineDef* lineDef,
    int (*callback) (mobj_t*, void*), void* parameters)
{
    void* linkstore[MAXLINKED];
    void** end = linkstore, **it;
    nodeindex_t root, nix;
    linknode_t* ln;
    int result = false;
    assert(map);

    root = map->lineLinks[GameMap_LineDefIndex(map, lineDef)];
    ln = map->lineNodes.nodes;

    for(nix = ln[root].next; nix != root; nix = ln[nix].next)
        *end++ = ln[nix].ptr;

    DO_LINKS(it, end, mobj_t *);
    return result;
}

/**
 * Increment validCount before using this. 'func' is called for each mobj
 * that is (even partly) inside the sector. This is not a 3D test, the
 * mobjs may actually be above or under the sector.
 *
 * (Lovely name; actually this is a combination of SectorMobjs and
 * a bunch of LineMobjs iterations.)
 */
int GameMap_SectorTouchingMobjsIterator(GameMap* map, Sector* sector,
    int (*callback) (mobj_t*, void*), void* parameters)
{
    uint i;
    void* linkstore[MAXLINKED];
    void** end = linkstore, **it;
    mobj_t* mo;
    LineDef* li;
    nodeindex_t root, nix;
    linknode_t* ln;
    int result = false;
    assert(map);

    ln = map->lineNodes.nodes;

    // First process the mobjs that obviously are in the sector.
    for(mo = sector->mobjList; mo; mo = mo->sNext)
    {
        if(mo->validCount == validCount)
            continue;

        mo->validCount = validCount;
        *end++ = mo;
    }

    // Then check the sector's lines.
    for(i = 0; i < sector->lineDefCount; ++i)
    {
        li = sector->lineDefs[i];

        // Iterate all mobjs on the line.
        root = map->lineLinks[GameMap_LineDefIndex(map, li)];
        for(nix = ln[root].next; nix != root; nix = ln[nix].next)
        {
            mo = (mobj_t *) ln[nix].ptr;
            if(mo->validCount == validCount)
                continue;

            mo->validCount = validCount;
            *end++ = mo;
        }
    }

    DO_LINKS(it, end, mobj_t *);
    return result;
}

/**
 * Looks for lines in the given block that intercept the given trace to add
 * to the intercepts list.
 * A line is crossed if its endpoints are on opposite sides of the trace.
 *
 * @return  Non-zero if current iteration should stop.
 */
int PIT_AddLineDefIntercepts(LineDef* lineDef, void* /*parameters*/)
{
    /// @todo Do not assume lineDef is from the current map.
    const divline_t* traceLOS = GameMap_TraceLOS(theMap);
    float distance;
    divline_t dl;
    int s1, s2;

    // Is this line crossed?
    // Avoid precision problems with two routines.
    if(traceLOS->direction[VX] >  FRACUNIT * 16 || traceLOS->direction[VY] >  FRACUNIT * 16 ||
       traceLOS->direction[VX] < -FRACUNIT * 16 || traceLOS->direction[VY] < -FRACUNIT * 16)
    {
        s1 = Divline_PointOnSide(traceLOS, lineDef->v1Origin());
        s2 = Divline_PointOnSide(traceLOS, lineDef->v2Origin());
    }
    else
    {
        s1 = lineDef->pointOnSide(FIX2FLT(traceLOS->origin[VX]), FIX2FLT(traceLOS->origin[VY])) < 0;
        s2 = lineDef->pointOnSide(FIX2FLT(traceLOS->origin[VX] + traceLOS->direction[VX]),
                                  FIX2FLT(traceLOS->origin[VY] + traceLOS->direction[VY])) < 0;
    }
    if(s1 == s2) return false;

    // Calculate interception point.
    lineDef->configureDivline(dl);
    distance = FIX2FLT(Divline_Intersection(&dl, traceLOS));
    // On the correct side of the trace origin?
    if(!(distance < 0))
    {
        P_AddIntercept(ICPT_LINE, distance, lineDef);
    }
    // Continue iteration.
    return false;
}

int PIT_AddMobjIntercepts(mobj_t* mo, void* /*paramaters*/)
{
    const divline_t* traceLOS;
    vec2d_t from, to;
    coord_t distance;
    divline_t dl;
    int s1, s2;

    if(mo->dPlayer && (mo->dPlayer->flags & DDPF_CAMERA))
        return false; // $democam: ssshh, keep going, we're not here...

    // Check a corner to corner crossection for hit.
    /// @todo Do not assume mobj is from the current map.
    traceLOS = GameMap_TraceLOS(theMap);
    if((traceLOS->direction[VX] ^ traceLOS->direction[VY]) > 0)
    {
        // \ Slope
        V2d_Set(from, mo->origin[VX] - mo->radius,
                      mo->origin[VY] + mo->radius);
        V2d_Set(to,   mo->origin[VX] + mo->radius,
                      mo->origin[VY] - mo->radius);
    }
    else
    {
        // / Slope
        V2d_Set(from, mo->origin[VX] - mo->radius,
                      mo->origin[VY] - mo->radius);
        V2d_Set(to,   mo->origin[VX] + mo->radius,
                      mo->origin[VY] + mo->radius);
    }

    // Is this line crossed?
    s1 = Divline_PointOnSide(traceLOS, from);
    s2 = Divline_PointOnSide(traceLOS, to);
    if(s1 == s2) return false;

    // Calculate interception point.
    dl.origin[VX] = FLT2FIX((float)from[VX]);
    dl.origin[VY] = FLT2FIX((float)from[VY]);
    dl.direction[VX] = FLT2FIX((float)(to[VX] - from[VX]));
    dl.direction[VY] = FLT2FIX((float)(to[VY] - from[VY]));
    distance = FIX2FLT(Divline_Intersection(&dl, traceLOS));
    // On the correct side of the trace origin?
    if(!(distance < 0))
    {
        P_AddIntercept(ICPT_MOBJ, distance, mo);
    }
    // Continue iteration.
    return false;
}

void P_LinkMobjInBlockmap(mobj_t* mo)
{
    /// @todo Do not assume mobj is from the current map.
    if(!theMap) return;
    GameMap_LinkMobj(theMap, mo);
}

boolean P_UnlinkMobjFromBlockmap(mobj_t* mo)
{
    /// @todo Do not assume mobj is from the current map.
    if(!theMap) return false;
    return GameMap_UnlinkMobj(theMap, mo);
}

void P_LinkMobjToLineDefs(mobj_t* mo)
{
    /// @todo Do not assume mobj is from the current map.
    if(!theMap) return;
    GameMap_LinkMobjToLineDefs(theMap, mo);
}

boolean P_UnlinkMobjFromLineDefs(mobj_t* mo)
{
    /// @todo Do not assume mobj is from the current map.
    if(!theMap) return false;
    return GameMap_UnlinkMobjFromLineDefs(theMap, mo);
}

#undef P_MobjLinesIterator
DENG_EXTERN_C int P_MobjLinesIterator(mobj_t* mo, int (*callback) (LineDef*, void*), void* parameters)
{
    /// @todo Do not assume mobj is in the current map.
    if(!theMap) return false; // Continue iteration.
    return GameMap_MobjLinesIterator(theMap, mo, callback, parameters);
}

#undef P_MobjSectorsIterator
DENG_EXTERN_C int P_MobjSectorsIterator(mobj_t* mo, int (*callback) (Sector*, void*), void* parameters)
{
    /// @todo Do not assume mobj is in the current map.
    if(!theMap) return false; // Continue iteration.
    return GameMap_MobjSectorsIterator(theMap, mo, callback, parameters);
}

#undef P_LineMobjsIterator
DENG_EXTERN_C int P_LineMobjsIterator(LineDef* lineDef, int (*callback) (mobj_t*, void*), void* parameters)
{
    /// @todo Do not assume lineDef is in the current map.
    if(!theMap) return false; // Continue iteration.
    return GameMap_LineMobjsIterator(theMap, lineDef, callback, parameters);
}

/**
 * Increment validCount before using this. 'func' is called for each mobj
 * that is (even partly) inside the sector. This is not a 3D test, the
 * mobjs may actually be above or under the sector.
 *
 * (Lovely name; actually this is a combination of SectorMobjs and
 * a bunch of LineMobjs iterations.)
 */
#undef P_SectorTouchingMobjsIterator
DENG_EXTERN_C int P_SectorTouchingMobjsIterator(Sector* sector, int (*callback) (mobj_t*, void*), void* parameters)
{
    /// @todo Do not assume sector is in the current map.
    if(!theMap) return false; // Continue iteration.
    return GameMap_SectorTouchingMobjsIterator(theMap, sector, callback, parameters);
}

#undef P_MobjsBoxIterator
DENG_EXTERN_C int P_MobjsBoxIterator(const AABoxd* box, int (*callback) (mobj_t*, void*), void* parameters)
{
    if(!theMap) return false; // Continue iteration.
    return GameMap_MobjsBoxIterator(theMap, box, callback, parameters);
}

#undef P_PolyobjsBoxIterator
DENG_EXTERN_C int P_PolyobjsBoxIterator(const AABoxd* box, int (*callback) (struct polyobj_s*, void*), void* parameters)
{
    if(!theMap) return false; // Continue iteration.
    return GameMap_PolyobjsBoxIterator(theMap, box, callback, parameters);
}

#undef P_LinesBoxIterator
DENG_EXTERN_C int P_LinesBoxIterator(const AABoxd* box, int (*callback) (LineDef*, void*), void* parameters)
{
    if(!theMap) return false; // Continue iteration.
    return GameMap_LineDefsBoxIterator(theMap, box, callback, parameters);
}

#undef P_PolyobjLinesBoxIterator
DENG_EXTERN_C int P_PolyobjLinesBoxIterator(const AABoxd* box, int (*callback) (LineDef*, void*), void* parameters)
{
    if(!theMap) return false; // Continue iteration.
    return GameMap_PolyobjLinesBoxIterator(theMap, box, callback, parameters);
}

#undef P_BspLeafsBoxIterator
DENG_EXTERN_C int P_BspLeafsBoxIterator(const AABoxd* box, Sector* sector,
    int (*callback) (BspLeaf*, void*), void* parameters)
{
    if(!theMap) return false; // Continue iteration.
    return GameMap_BspLeafsBoxIterator(theMap, box, sector, callback, parameters);
}

#undef P_AllLinesBoxIterator
DENG_EXTERN_C int P_AllLinesBoxIterator(const AABoxd* box, int (*callback) (LineDef*, void*), void* parameters)
{
    if(!theMap) return false; // Continue iteration.
    return GameMap_AllLineDefsBoxIterator(theMap, box, callback, parameters);
}

#undef P_PathTraverse2
DENG_EXTERN_C int P_PathTraverse2(coord_t const from[2], coord_t const to[2], int flags, traverser_t callback,
    void* paramaters)
{
    if(!theMap) return false; // Continue iteration.
    return GameMap_PathTraverse2(theMap, from, to, flags, callback, paramaters);
}

#undef P_PathTraverse
DENG_EXTERN_C int P_PathTraverse(coord_t const from[2], coord_t const to[2], int flags, traverser_t callback)
{
    if(!theMap) return false; // Continue iteration.
    return GameMap_PathTraverse(theMap, from, to, flags, callback);
}

#undef P_PathXYTraverse2
DENG_EXTERN_C int P_PathXYTraverse2(coord_t fromX, coord_t fromY, coord_t toX, coord_t toY, int flags,
    traverser_t callback, void* paramaters)
{
    if(!theMap) return false; // Continue iteration.
    return GameMap_PathXYTraverse2(theMap, fromX, fromY, toX, toY, flags, callback, paramaters);
}

#undef P_PathXYTraverse
DENG_EXTERN_C int P_PathXYTraverse(coord_t fromX, coord_t fromY, coord_t toX, coord_t toY, int flags,
    traverser_t callback)
{
    if(!theMap) return false; // Continue iteration.
    return GameMap_PathXYTraverse(theMap, fromX, fromY, toX, toY, flags, callback);
}

#undef P_CheckLineSight
DENG_EXTERN_C boolean P_CheckLineSight(coord_t const from[3], coord_t const to[3], coord_t bottomSlope,
    coord_t topSlope, int flags)
{
    if(!theMap) return false; // I guess?
    return GameMap_CheckLineSight(theMap, from, to, bottomSlope, topSlope, flags);
}
