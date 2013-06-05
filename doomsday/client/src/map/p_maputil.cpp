/** @file p_maputil.cpp Map Utility Routines
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
#include "de_play.h"
#include "de_misc.h"

#include "map/gamemap.h"
#include "render/r_main.h" // validCount

using namespace de;

#define ORDER(x,y,a,b)  ( (x)<(y)? ((a)=(x),(b)=(y)) : ((b)=(x),(a)=(y)) )

// Linkstore is list of pointers gathered when iterating stuff.
// This is pretty much the only way to avoid *all* potential problems
// caused by callback routines behaving badly (moving or destroying
// mobjs). The idea is to get a snapshot of all the objects being
// iterated before any callbacks are called. The hardcoded limit is
// a drag, but I'd like to see you iterating 2048 mobjs/lines in one block.

#define MAXLINKED           2048
#define DO_LINKS(it, end, _Type)   { \
    for(it = linkStore; it < end; it++) \
    { \
        result = callback(reinterpret_cast<_Type>(*it), parameters); \
        if(result) break; \
    } \
}

#undef P_TraceLOS
DENG_EXTERN_C divline_t const *P_TraceLOS()
{
    static divline_t emptyLOS;
    if(theMap)
    {
        return &theMap->traceLine();
    }
    return &emptyLOS;
}

#undef P_TraceOpening
DENG_EXTERN_C TraceOpening const *P_TraceOpening()
{
    static TraceOpening zeroOpening;
    if(theMap)
    {
        return &theMap->traceOpening();
    }
    return &zeroOpening;
}

#undef P_SetTraceOpening
DENG_EXTERN_C void P_SetTraceOpening(Line *line)
{
    if(!theMap || !line) return;
    /// @todo Do not assume line is from the CURRENT map.
    theMap->setTraceOpening(*line);
}

#undef P_BspLeafAtPoint_FixedPrecision
DENG_EXTERN_C BspLeaf *P_BspLeafAtPoint_FixedPrecision(const_pvec2d_t point)
{
    if(!theMap) return 0;
    return theMap->bspLeafAtPoint_FixedPrecision(point);
}

#undef P_BspLeafAtPoint_FixedPrecisionXY
DENG_EXTERN_C BspLeaf *P_BspLeafAtPoint_FixedPrecisionXY(coord_t x, coord_t y)
{
    if(!theMap) return 0;
    coord_t point[2] = { x, y };
    return theMap->bspLeafAtPoint_FixedPrecision(point);
}

bool P_IsPointInBspLeaf(Vector2d const &point, BspLeaf const &bspLeaf)
{
    if(bspLeaf.isDegenerate())
        return false; // Obviously not.

    de::Mesh const &poly = bspLeaf.poly();
    HEdge const *firstHEdge = poly.firstFace()->hedge();

    HEdge const *hedge = firstHEdge;
    do
    {
        Vertex const &va = hedge->vertex();
        Vertex const &vb = hedge->next().vertex();

        if(((va.origin().y - point.y) * (vb.origin().x - va.origin().x) -
            (va.origin().x - point.x) * (vb.origin().y - va.origin().y)) < 0)
        {
            // Outside the BSP leaf's edges.
            return false;
        }

    } while((hedge = &hedge->next()) != firstHEdge);

    return true;
}

bool P_IsPointInSector(Vector2d const &point, Sector const &sector)
{
    DENG_ASSERT(theMap != 0);

    /// @todo Do not assume @a sector is from the current map.
    BspLeaf *bspLeaf = theMap->bspLeafAtPoint(point);
    if(bspLeaf->sectorPtr() != &sector)
        return false;

    return P_IsPointInBspLeaf(point, *bspLeaf);
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
    if(!P_UnlinkMobjFromLines(mo))
        links |= DDLINK_NOLINE;

    return links;
}

/**
 * Unlinks the mobj from all the lines it's been linked to. Can be called
 * without checking that the list does indeed contain lines.
 */
boolean GameMap_UnlinkMobjFromLines(GameMap* map, mobj_t* mo)
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
 * @note Caller must ensure a mobj is linked only once to any given line.
 *
 * @param map  GameMap instance.
 * @param mo  Mobj to be linked.
 * @param line  Line to link the mobj to.
 */
void GameMap_LinkMobjToLine(GameMap *map, mobj_t *mo, Line *line)
{
    DENG_ASSERT(map);

    if(!mo || !line) return;

    // Add a node to the mobj's ring.
    nodeindex_t nodeIndex = NP_New(&map->mobjNodes, line);
    NP_Link(&map->mobjNodes, nodeIndex, mo->lineRoot);

    // Add a node to the line's ring. Also store the linenode's index
    // into the mobjring's node, so unlinking is easy.
    nodeIndex = map->mobjNodes.nodes[nodeIndex].data = NP_New(&map->lineNodes, mo);
    NP_Link(&map->lineNodes, nodeIndex, map->lineLinks[line->indexInMap()]);
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
int PIT_LinkToLines(Line *ld, void *parameters)
{
    linelinker_data_t *p = reinterpret_cast<linelinker_data_t *>(parameters);
    DENG_ASSERT(p);

    // Do the bounding boxes intercept?
    if(p->box.minX >= ld->aaBox().maxX ||
       p->box.minY >= ld->aaBox().maxY ||
       p->box.maxX <= ld->aaBox().minX ||
       p->box.maxY <= ld->aaBox().minY) return false;

    // Line does not cross the mobj's bounding box?
    if(ld->boxOnSide(p->box)) return false;

    // Lines with only one sector will not be linked to because a mobj can't
    // legally cross one.
    if(!ld->hasFrontSector() || !ld->hasBackSector()) return false;

    GameMap_LinkMobjToLine(p->map, p->mo, ld);
    return false;
}

/**
 * @note Caller must ensure that the mobj is currently unlinked.
 */
void GameMap_LinkMobjToLines(GameMap* map, mobj_t* mo)
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
    mo->bspLeaf = P_BspLeafAtPoint_FixedPrecision(mo->origin);

    if(flags & DDLINK_SECTOR)
    {
        // Unlink from the current sector, if any.
        Sector &sec = mo->bspLeaf->sector();

        if(mo->sPrev)
            P_UnlinkMobjFromSector(mo);

        // Link the new mobj to the head of the list.
        // Prev pointers point to the pointer that points back to us.
        // (Which practically disallows traversing the list backwards.)

        if((mo->sNext = sec.firstMobj()))
            mo->sNext->sPrev = &mo->sNext;

        *(mo->sPrev = &sec._mobjList) = mo;
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
        P_UnlinkMobjFromLines(mo);

        // Link to all contacted lines.
        P_LinkMobjToLines(mo);
    }

    // If this is a player - perform additional tests to see if they have
    // entered or exited the void.
    if(mo->dPlayer && mo->dPlayer->mo)
    {
        ddplayer_t *player = mo->dPlayer;
        Sector &sector = player->mo->bspLeaf->sector();

        player->inVoid = true;
        if(P_IsPointInSector(player->mo->origin, sector) &&
           (player->mo->origin[VZ] <  sector.ceiling().visHeight() + 4 &&
            player->mo->origin[VZ] >= sector.floor().visHeight()))
            player->inVoid = false;
    }
}

/**
 * The callback function will be called once for each line that crosses
 * trough the object. This means all the lines will be two-sided.
 */
int GameMap_MobjLinesIterator(GameMap *map, mobj_t *mo,
    int (*callback) (Line *, void *), void* parameters)
{
    DENG_ASSERT(map);

    void *linkStore[MAXLINKED];
    void **end = linkStore, **it;
    int result = false;

    linknode_t *tn = map->mobjNodes.nodes;
    if(mo->lineRoot)
    {
        nodeindex_t nix;
        for(nix = tn[mo->lineRoot].next; nix != mo->lineRoot;
            nix = tn[nix].next)
            *end++ = tn[nix].ptr;

        DO_LINKS(it, end, Line*);
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
    void *linkStore[MAXLINKED];
    void **end = linkStore, **it;

    nodeindex_t nix;
    linknode_t *tn = map->mobjNodes.nodes;

    // Always process the mobj's own sector first.
    Sector &ownSec = mo->bspLeaf->sector();
    *end++ = &ownSec;
    ownSec.setValidCount(validCount);

    // Any good lines around here?
    if(mo->lineRoot)
    {
        for(nix = tn[mo->lineRoot].next; nix != mo->lineRoot;
            nix = tn[nix].next)
        {
            Line *ld = (Line *) tn[nix].ptr;

            // All these lines have sectors on both sides.
            // First, try the front.
            Sector &frontSec = ld->frontSector();
            if(frontSec.validCount() != validCount)
            {
                *end++ = &frontSec;
                frontSec.setValidCount(validCount);
            }

            // And then the back.
            if(ld->hasBackSector())
            {
                Sector &backSec = ld->backSector();
                if(backSec.validCount() != validCount)
                {
                    *end++ = &backSec;
                    backSec.setValidCount(validCount);
                }
            }
        }
    }

    DO_LINKS(it, end, Sector *);
    return result;
}

int GameMap_LineMobjsIterator(GameMap *map, Line *line,
    int (*callback) (mobj_t *, void *), void *parameters)
{
    DENG_ASSERT(map);

    void *linkStore[MAXLINKED];
    void **end = linkStore;

    nodeindex_t root = map->lineLinks[line->indexInMap()];
    linknode_t *ln = map->lineNodes.nodes;

    for(nodeindex_t nix = ln[root].next; nix != root; nix = ln[nix].next)
    {
        DENG_ASSERT(end < &linkStore[MAXLINKED]);
        *end++ = ln[nix].ptr;
    }

    int result = false;
    void **it;
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
int GameMap_SectorTouchingMobjsIterator(GameMap *map, Sector *sector,
    int (*callback) (mobj_t *, void *), void *parameters)
{
    DENG_ASSERT(map);

    /// @todo Fixme: Remove fixed limit (use QVarLengthArray if necessary).
    void *linkStore[MAXLINKED];
    void **end = linkStore;

    // Collate mobjs that obviously are in the sector.
    for(mobj_t *mo = sector->firstMobj(); mo; mo = mo->sNext)
    {
        if(mo->validCount == validCount) continue;
        mo->validCount = validCount;

        DENG_ASSERT(end < &linkStore[MAXLINKED]);
        *end++ = mo;
    }

    // Collate mobjs linked to the sector's lines.
    linknode_t const *ln = map->lineNodes.nodes;
    foreach(Line::Side *side, sector->sides())
    {
        nodeindex_t root = map->lineLinks[side->line().indexInMap()];

        for(nodeindex_t nix = ln[root].next; nix != root; nix = ln[nix].next)
        {
            mobj_t *mo = (mobj_t *) ln[nix].ptr;

            if(mo->validCount == validCount) continue;
            mo->validCount = validCount;

            DENG_ASSERT(end < &linkStore[MAXLINKED]);
            *end++ = mo;
        }
    }

    // Process all collected mobjs.
    int result = false;
    void **it;
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
int PIT_AddLineIntercepts(Line *line, void * /*parameters*/)
{
    /// @todo Do not assume line is from the current map.
    divline_t const &traceLos = theMap->traceLine();
    int s1, s2;

    fixed_t lineFromX[2] = { DBL2FIX(line->fromOrigin().x), DBL2FIX(line->fromOrigin().y) };
    fixed_t lineToX[2]   = { DBL2FIX(  line->toOrigin().x), DBL2FIX(  line->toOrigin().y) };

    // Is this line crossed?
    // Avoid precision problems with two routines.
    if(traceLos.direction[VX] >  FRACUNIT * 16 || traceLos.direction[VY] >  FRACUNIT * 16 ||
       traceLos.direction[VX] < -FRACUNIT * 16 || traceLos.direction[VY] < -FRACUNIT * 16)
    {
        s1 = V2x_PointOnLineSide(lineFromX, traceLos.origin, traceLos.direction);
        s2 = V2x_PointOnLineSide(lineToX,   traceLos.origin, traceLos.direction);
    }
    else
    {
        s1 = line->pointOnSide(FIX2FLT(traceLos.origin[VX]), FIX2FLT(traceLos.origin[VY])) < 0;
        s2 = line->pointOnSide(FIX2FLT(traceLos.origin[VX] + traceLos.direction[VX]),
                               FIX2FLT(traceLos.origin[VY] + traceLos.direction[VY])) < 0;
    }
    if(s1 == s2) return false;

    fixed_t lineDirectionX[2] = { DBL2FIX(line->direction().x), DBL2FIX(line->direction().y) };

    // On the correct side of the trace origin?
    float distance = FIX2FLT(V2x_Intersection(lineFromX, lineDirectionX,
                                              traceLos.origin, traceLos.direction));
    if(!(distance < 0))
    {
        P_AddIntercept(ICPT_LINE, distance, line);
    }

    // Continue iteration.
    return false;
}

int PIT_AddMobjIntercepts(mobj_t *mo, void * /*parameters*/)
{
    if(mo->dPlayer && (mo->dPlayer->flags & DDPF_CAMERA))
        return false; // $democam: ssshh, keep going, we're not here...

    // Check a corner to corner crossection for hit.
    /// @todo Do not assume mobj is from the current map.
    divline_t const &traceLos = theMap->traceLine();
    vec2d_t from, to;
    if((traceLos.direction[VX] ^ traceLos.direction[VY]) > 0)
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
    if(Divline_PointOnSide(&traceLos, from) == Divline_PointOnSide(&traceLos, to))
        return false;

    // Calculate interception point.
    divline_t dl;
    dl.origin[VX] = DBL2FIX(from[VX]);
    dl.origin[VY] = DBL2FIX(from[VY]);
    dl.direction[VX] = DBL2FIX(to[VX] - from[VX]);
    dl.direction[VY] = DBL2FIX(to[VY] - from[VY]);
    coord_t distance = FIX2FLT(Divline_Intersection(&dl, &traceLos));

    // On the correct side of the trace origin?
    if(!(distance < 0))
    {
        P_AddIntercept(ICPT_MOBJ, distance, mo);
    }

    // Continue iteration.
    return false;
}

void P_LinkMobjInBlockmap(mobj_t *mo)
{
    /// @todo Do not assume mobj is from the current map.
    if(!theMap || !mo) return;
    theMap->linkMobj(*mo);
}

boolean P_UnlinkMobjFromBlockmap(mobj_t *mo)
{
    /// @todo Do not assume mobj is from the current map.
    if(!theMap || !mo) return false;
    return theMap->unlinkMobj(*mo);
}

void P_LinkMobjToLines(mobj_t *mo)
{
    /// @todo Do not assume mobj is from the current map.
    if(!theMap) return;
    GameMap_LinkMobjToLines(theMap, mo);
}

boolean P_UnlinkMobjFromLines(mobj_t *mo)
{
    /// @todo Do not assume mobj is from the current map.
    if(!theMap) return false;
    return GameMap_UnlinkMobjFromLines(theMap, mo);
}

#undef P_MobjLinesIterator
DENG_EXTERN_C int P_MobjLinesIterator(mobj_t *mo, int (*callback) (Line *, void *), void *parameters)
{
    /// @todo Do not assume mobj is in the current map.
    if(!theMap) return false; // Continue iteration.
    return GameMap_MobjLinesIterator(theMap, mo, callback, parameters);
}

#undef P_MobjSectorsIterator
DENG_EXTERN_C int P_MobjSectorsIterator(mobj_t *mo, int (*callback) (Sector *, void *), void *parameters)
{
    /// @todo Do not assume mobj is in the current map.
    if(!theMap) return false; // Continue iteration.
    return GameMap_MobjSectorsIterator(theMap, mo, callback, parameters);
}

#undef P_LineMobjsIterator
DENG_EXTERN_C int P_LineMobjsIterator(Line *line, int (*callback) (mobj_t *, void *), void *parameters)
{
    /// @todo Do not assume line is in the current map.
    if(!theMap) return false; // Continue iteration.
    return GameMap_LineMobjsIterator(theMap, line, callback, parameters);
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
DENG_EXTERN_C int P_SectorTouchingMobjsIterator(Sector *sector, int (*callback) (mobj_t *, void *), void *parameters)
{
    /// @todo Do not assume sector is in the current map.
    if(!theMap) return false; // Continue iteration.
    return GameMap_SectorTouchingMobjsIterator(theMap, sector, callback, parameters);
}

#undef P_MobjsBoxIterator
DENG_EXTERN_C int P_MobjsBoxIterator(AABoxd const *box,
    int (*callback) (mobj_t *, void *), void *parameters)
{
    if(!theMap || !box) return false; // Continue iteration.
    return theMap->mobjsBoxIterator(*box, callback, parameters);
}

#undef P_PolyobjsBoxIterator
DENG_EXTERN_C int P_PolyobjsBoxIterator(AABoxd const *box,
    int (*callback) (struct polyobj_s *, void *), void *parameters)
{
    if(!theMap || !box) return false; // Continue iteration.
    return theMap->polyobjsBoxIterator(*box, callback, parameters);
}

#undef P_LinesBoxIterator
DENG_EXTERN_C int P_LinesBoxIterator(AABoxd const *box,
    int (*callback) (Line *, void *), void *parameters)
{
    if(!theMap || !box) return false; // Continue iteration.
    return theMap->linesBoxIterator(*box, callback, parameters);
}

#undef P_PolyobjLinesBoxIterator
DENG_EXTERN_C int P_PolyobjLinesBoxIterator(AABoxd const *box,
    int (*callback) (Line *, void *), void *parameters)
{
    if(!theMap || !box) return false; // Continue iteration.
    return theMap->polyobjLinesBoxIterator(*box, callback, parameters);
}

#undef P_BspLeafsBoxIterator
DENG_EXTERN_C int P_BspLeafsBoxIterator(AABoxd const *box, Sector *sector,
    int (*callback) (BspLeaf *, void *), void *parameters)
{
    if(!theMap || !box) return false; // Continue iteration.
    return theMap->bspLeafsBoxIterator(*box, sector, callback, parameters);
}

#undef P_AllLinesBoxIterator
DENG_EXTERN_C int P_AllLinesBoxIterator(AABoxd const *box,
    int (*callback) (Line *, void *), void *parameters)
{
    if(!theMap || !box) return false; // Continue iteration.
    return theMap->allLinesBoxIterator(*box, callback, parameters);
}

#undef P_PathTraverse2
DENG_EXTERN_C int P_PathTraverse2(const_pvec2d_t from, const_pvec2d_t to,
    int flags, traverser_t callback, void *parameters)
{
    if(!theMap) return false; // Continue iteration.
    return theMap->pathTraverse(from, to, flags, callback, parameters);
}

#undef P_PathTraverse
DENG_EXTERN_C int P_PathTraverse(const_pvec2d_t from, const_pvec2d_t to,
    int flags, traverser_t callback)
{
    if(!theMap) return false; // Continue iteration.
    return theMap->pathTraverse(from, to, flags, callback);
}

#undef P_PathXYTraverse2
DENG_EXTERN_C int P_PathXYTraverse2(coord_t fromX, coord_t fromY,
    coord_t toX, coord_t toY, int flags, traverser_t callback, void* paramaters)
{
    if(!theMap) return false; // Continue iteration.
    return theMap->pathTraverse(fromX, fromY, toX, toY, flags, callback, paramaters);
}

#undef P_PathXYTraverse
DENG_EXTERN_C int P_PathXYTraverse(coord_t fromX, coord_t fromY, coord_t toX, coord_t toY, int flags,
    traverser_t callback)
{
    if(!theMap) return false; // Continue iteration.
    return theMap->pathTraverse(fromX, fromY, toX, toY, flags, callback);
}
