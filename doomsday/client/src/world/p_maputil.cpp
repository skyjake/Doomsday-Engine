/** @file p_maputil.cpp World map utility routines.
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

#include <de/aabox.h>
#include <de/vector1.h>

#include "BspLeaf"
#include "Line"
#include "Sector"

#include "world/p_object.h"

#include "render/r_main.h" /// validCount, @todo remove me

#include "world/map.h"

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

/**
 * Two links to update:
 * 1) The link to us from the previous node (sprev, always set) will
 *    be modified to point to the node following us.
 * 2) If there is a node following us, set its sprev pointer to point
 *    to the pointer that points back to it (our sprev, just modified).
 */
static bool unlinkMobjFromSector(mobj_t &mo)
{
    if(!IS_SECTOR_LINKED(&mo))
        return false;

    if((*mo.sPrev = mo.sNext))
        mo.sNext->sPrev = mo.sPrev;

    // Not linked any more.
    mo.sNext = 0;
    mo.sPrev = 0;

    return true;
}

/**
 * Unlinks the mobj from all the lines it's been linked to. Can be called
 * without checking that the list does indeed contain lines.
 */
static bool unlinkMobjFromLines(Map &map, mobj_t &mo)
{
    // Try unlinking from lines.
    if(!mo.lineRoot)
        return false; // A zero index means it's not linked.

    // Unlink from each line.
    linknode_t *tn = map.mobjNodes.nodes;
    for(nodeindex_t nix = tn[mo.lineRoot].next; nix != mo.lineRoot;
        nix = tn[nix].next)
    {
        // Data is the linenode index that corresponds this mobj.
        NP_Unlink((&map.lineNodes), tn[nix].data);
        // We don't need these nodes any more, mark them as unused.
        // Dismissing is a macro.
        NP_Dismiss((&map.lineNodes), tn[nix].data);
        NP_Dismiss((&map.mobjNodes), nix);
    }

    // The mobj no longer has a line ring.
    NP_Dismiss((&map.mobjNodes), mo.lineRoot);
    mo.lineRoot = 0;

    return true;
}

/**
 * @note Caller must ensure a mobj is linked only once to any given line.
 *
 * @param map   Map instance.
 * @param mo    Mobj to be linked.
 * @param line  Line to link the mobj to.
 */
static void linkMobjToLine(Map *map, mobj_t *mo, Line *line)
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

struct LineLinkerParams
{
    Map *map;
    mobj_t *mo;
    AABoxd box;
};

/**
 * The given line might cross the mobj. If necessary, link the mobj into
 * the line's mobj link ring.
 */
static int lineLinkerWorker(Line *ld, void *parameters)
{
    LineLinkerParams *p = reinterpret_cast<LineLinkerParams *>(parameters);
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

    linkMobjToLine(p->map, p->mo, ld);
    return false;
}

/**
 * @note Caller must ensure that the mobj is currently unlinked.
 */
static void linkMobjToLines(Map &map, mobj_t &mo)
{
    // Get a new root node.
    mo.lineRoot = NP_New(&map.mobjNodes, NP_ROOT_NODE);

    // Set up a line iterator for doing the linking.
    LineLinkerParams parm;
    parm.map = &map;
    parm.mo  = &mo;

    vec2d_t point;
    V2d_Set(point, mo.origin[VX] - mo.radius, mo.origin[VY] - mo.radius);
    V2d_InitBox(parm.box.arvec2, point);
    V2d_Set(point, mo.origin[VX] + mo.radius, mo.origin[VY] + mo.radius);
    V2d_AddToBox(parm.box.arvec2, point);

    validCount++;
    map.allLinesBoxIterator(parm.box, lineLinkerWorker, &parm);
}

int Map::unlink(mobj_t &mo)
{
    int links = 0;

    if(unlinkMobjFromSector(mo))
        links |= DDLINK_SECTOR;
    if(unlinkMobjInBlockmap(mo))
        links |= DDLINK_BLOCKMAP;
    if(!unlinkMobjFromLines(*this, mo))
        links |= DDLINK_NOLINE;

    return links;
}

void Map::link(mobj_t &mo, byte flags)
{
    // Link into the sector.
    mo.bspLeaf = bspLeafAtPoint_FixedPrecision(mo.origin);

    if(flags & DDLINK_SECTOR)
    {
        // Unlink from the current sector, if any.
        Sector &sec = mo.bspLeaf->sector();

        if(mo.sPrev)
            unlinkMobjFromSector(mo);

        // Link the new mobj to the head of the list.
        // Prev pointers point to the pointer that points back to us.
        // (Which practically disallows traversing the list backwards.)

        if((mo.sNext = sec.firstMobj()))
            mo.sNext->sPrev = &mo.sNext;

        *(mo.sPrev = &sec._mobjList) = &mo;
    }

    // Link into blockmap?
    if(flags & DDLINK_BLOCKMAP)
    {
        // Unlink from the old block, if any.
        unlinkMobjInBlockmap(mo);
        linkMobjInBlockmap(mo);
    }

    // Link into lines.
    if(!(flags & DDLINK_NOLINE))
    {
        // Unlink from any existing lines.
        unlinkMobjFromLines(*this, mo);

        // Link to all contacted lines.
        linkMobjToLines(*this, mo);
    }

    // If this is a player - perform additional tests to see if they have
    // entered or exited the void.
    if(mo.dPlayer && mo.dPlayer->mo)
    {
        ddplayer_t *player = mo.dPlayer;
        Sector &sector = player->mo->bspLeaf->sector();

        player->inVoid = true;
        if(sector.pointInside(player->mo->origin))
        {
#ifdef __CLIENT__
            if(player->mo->origin[VZ] <  sector.ceiling().visHeight() + 4 &&
               player->mo->origin[VZ] >= sector.floor().visHeight())
#else
            if(player->mo->origin[VZ] <  sector.ceiling().height() + 4 &&
               player->mo->origin[VZ] >= sector.floor().height())
#endif
            {
                player->inVoid = false;
            }
        }
    }
}

int Map::mobjLinesIterator(mobj_t *mo, int (*callback) (Line *, void *),
                           void *parameters) const
{
    void *linkStore[MAXLINKED];
    void **end = linkStore, **it;
    int result = false;

    linknode_t *tn = mobjNodes.nodes;
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

int Map::mobjSectorsIterator(mobj_t *mo, int (*callback) (Sector *, void *),
                             void *parameters) const
{
    int result = false;
    void *linkStore[MAXLINKED];
    void **end = linkStore, **it;

    nodeindex_t nix;
    linknode_t *tn = mobjNodes.nodes;

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

int Map::lineMobjsIterator(Line *line, int (*callback) (mobj_t *, void *),
                           void *parameters) const
{
    void *linkStore[MAXLINKED];
    void **end = linkStore;

    nodeindex_t root = lineLinks[line->indexInMap()];
    linknode_t *ln = lineNodes.nodes;

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

int Map::sectorTouchingMobjsIterator(Sector *sector,
    int (*callback) (mobj_t *, void *), void *parameters) const
{
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
    linknode_t const *ln = lineNodes.nodes;
    foreach(Line::Side *side, sector->sides())
    {
        nodeindex_t root = lineLinks[side->line().indexInMap()];

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
