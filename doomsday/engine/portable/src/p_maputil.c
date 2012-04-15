/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2012 Daniel Swanson <danij@dengine.net>
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
 * p_maputil.c: Map Utility Routines
 */

// HEADER FILES ------------------------------------------------------------

#include <math.h>

#include "de_base.h"
#include "de_console.h"
#include "de_play.h"
#include "de_refresh.h"
#include "de_misc.h"

// MACROS ------------------------------------------------------------------

#define ORDER(x,y,a,b)  ( (x)<(y)? ((a)=(x),(b)=(y)) : ((b)=(x),(a)=(y)) )

// Linkstore is list of pointers gathered when iterating stuff.
// This is pretty much the only way to avoid *all* potential problems
// caused by callback routines behaving badly (moving or destroying
// mobjs). The idea is to get a snapshot of all the objects being
// iterated before any callbacks are called. The hardcoded limit is
// a drag, but I'd like to see you iterating 2048 mobjs/lines in one block.

#define MAXLINKED           2048
#define DO_LINKS(it, end)   { \
    for(it = linkstore; it < end; it++) \
    { \
        result = callback(*it, parameters); \
        if(result) break; \
    } \
}

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

float P_AccurateDistanceFixed(fixed_t dx, fixed_t dy)
{
    float               fx = FIX2FLT(dx), fy = FIX2FLT(dy);

    return (float) sqrt(fx * fx + fy * fy);
}

float P_AccurateDistance(float dx, float dy)
{
    return (float) sqrt(dx * dx + dy * dy);
}

/**
 * Gives an estimation of distance (not exact).
 */
float P_ApproxDistance(float dx, float dy)
{
    dx = fabs(dx);
    dy = fabs(dy);

    return dx + dy - ((dx < dy ? dx : dy) / 2);
}

/**
 * Gives an estimation of 3D distance (not exact).
 * The Z axis aspect ratio is corrected.
 */
float P_ApproxDistance3(float dx, float dy, float dz)
{
    return P_ApproxDistance(P_ApproxDistance(dx, dy), dz * 1.2f);
}

/**
 * Either end or fixpoint must be specified. The distance is measured
 * (approximately) in 3D. Start must always be specified.
 */
float P_MobjPointDistancef(mobj_t* start, mobj_t* end, float* fixpoint)
{
    if(!start)
        return 0;

    if(end)
    {
        // Start -> end.
        return M_ApproxDistancef(end->pos[VZ] - start->pos[VZ],
                                 M_ApproxDistancef(end->pos[VX] - start->pos[VX],
                                                   end->pos[VY] - start->pos[VY]));
    }

    if(fixpoint)
    {
        float               sp[3];

        sp[VX] = start->pos[VX];
        sp[VY] = start->pos[VY],
        sp[VZ] = start->pos[VZ];

        return M_ApproxDistancef(fixpoint[VZ] - sp[VZ],
                                 M_ApproxDistancef(fixpoint[VX] - sp[VX],
                                                   fixpoint[VY] - sp[VY]));
    }

    return 0;
}

float P_PointOnLineSide(float x, float y, float lX, float lY, float lDX, float lDY)
{
    return (lY - y) * lDX - (lX - x) * lDY;
}

/// @note Part of the Doomsday public API.
int P_PointOnLineDefSide(float const xy[2], const LineDef* lineDef)
{
    if(!xy || !lineDef)
    {
        DEBUG_Message(("P_PointOnLineDefSide: Invalid arguments, returning zero.\n"));
        return 0;
    }
    return LineDef_PointOnSide(lineDef, xy);
}

/// @note Part of the Doomsday public API.
int P_PointXYOnLineDefSide(float x, float y, const LineDef* lineDef)
{
    if(!lineDef)
    {
        DEBUG_Message(("P_PointXYOnLineDefSide: Invalid arguments, returning zero.\n"));
        return 0;
    }
    return LineDef_PointXYOnSide(lineDef, x, y);
}

/**
 * Where is the given point in relation to the line.
 *
 * @param pointX        X coordinate of the point.
 * @param pointY        Y coordinate of the point.
 * @param lineDX        X delta of the line.
 * @param lineDY        Y delta of the line.
 * @param linePerp      Perpendicular d of the line.
 * @param lineLength    Length of the line.
 *
 * @return              @c <0= on left side.
 *                      @c  0= intersects.
 *                      @c >0= on right side.
 */
int P_PointOnLinedefSide2(double pointX, double pointY, double lineDX,
                       double lineDY, double linePerp, double lineLength,
                       double epsilon)
{
    double              perp =
        M_PerpDist(lineDX, lineDY, linePerp, lineLength, pointX, pointY);

    if(fabs(perp) <= epsilon)
        return 0;

    return (perp < 0? -1 : +1);
}

int P_BoxOnLineSide3(const AABox* aaBox, double lineSX, double lineSY,
    double lineDX, double lineDY, double linePerp, double lineLength, double epsilon)
{
#define IFFY_LEN                4.0

    double x1 = (double)aaBox->minX - IFFY_LEN * 1.5;
    double y1 = (double)aaBox->minY - IFFY_LEN * 1.5;
    double x2 = (double)aaBox->maxX + IFFY_LEN * 1.5;
    double y2 = (double)aaBox->maxY + IFFY_LEN * 1.5;
    int p1, p2;

    if(FEQUAL(lineDX, 0))
    {
        // Horizontal.
        p1 = (x1 > lineSX? +1 : -1);
        p2 = (x2 > lineSX? +1 : -1);

        if(lineDY < 0)
        {
            p1 = -p1;
            p2 = -p2;
        }
    }
    else if(FEQUAL(lineDY, 0))
    {
        // Vertical.
        p1 = (y1 < lineSY? +1 : -1);
        p2 = (y2 < lineSY? +1 : -1);

        if(lineDX < 0)
        {
            p1 = -p1;
            p2 = -p2;
        }
    }
    else if(lineDX * lineDY > 0)
    {
        // Positive slope.
        p1 = P_PointOnLinedefSide2(x1, y2, lineDX, lineDY, linePerp, lineLength, epsilon);
        p2 = P_PointOnLinedefSide2(x2, y1, lineDX, lineDY, linePerp, lineLength, epsilon);
    }
    else
    {
        // Negative slope.
        p1 = P_PointOnLinedefSide2(x1, y1, lineDX, lineDY, linePerp, lineLength, epsilon);
        p2 = P_PointOnLinedefSide2(x2, y2, lineDX, lineDY, linePerp, lineLength, epsilon);
    }

    if(p1 == p2) return p1;
    return 0;

#undef IFFY_LEN
}

/**
 * Considers the line to be infinite.
 *
 * @return              @c  0 = completely in front of the line.
 * @return              @c  1 = completely behind the line.
 *                      @c -1 = box crosses the line.
 */
int P_BoxOnLineSide2(float xl, float xh, float yl, float yh,
                     const LineDef* ld)
{
    int                 a = 0, b = 0;

    switch(ld->slopeType)
    {
    default: // Shut up compiler.
      case ST_HORIZONTAL:
        a = yh > ld->L_v1pos[VY];
        b = yl > ld->L_v1pos[VY];
        if(ld->dX < 0)
        {
            a ^= 1;
            b ^= 1;
        }
        break;

      case ST_VERTICAL:
        a = xh < ld->L_v1pos[VX];
        b = xl < ld->L_v1pos[VX];
        if(ld->dY < 0)
        {
            a ^= 1;
            b ^= 1;
        }
        break;

      case ST_POSITIVE:
        a = P_PointXYOnLineDefSide(xl, yh, ld);
        b = P_PointXYOnLineDefSide(xh, yl, ld);
        break;

    case ST_NEGATIVE:
        a = P_PointXYOnLineDefSide(xh, yh, ld);
        b = P_PointXYOnLineDefSide(xl, yl, ld);
        break;
    }

    if(a == b)
        return a;

    return -1;
}

int P_BoxOnLineSide(const AABoxf* box, const LineDef* ld)
{
    return P_BoxOnLineSide2(box->minX, box->maxX,
                            box->minY, box->maxY, ld);
}

/**
 * @return              @c 0 if point is in front of the line, else @c 1.
 */
int P_PointOnDivlineSide(float fx, float fy, const divline_t* line)
{
    fixed_t             x = FLT2FIX(fx);
    fixed_t             y = FLT2FIX(fy);

    if(!line->dX)
    {
        return (x <= line->pos[VX])? line->dY > 0 : line->dY < 0;
    }
    else if(!line->dY)
    {
        return (y <= line->pos[VY])? line->dX < 0 : line->dX > 0;
    }
    else
    {
        fixed_t             dX = x - line->pos[VX];
        fixed_t             dY = y - line->pos[VY];

        // Try to quickly decide by comparing signs.
        if((line->dY ^ line->dX ^ dX ^ dY) & 0x80000000)
        {   // Left is negative.
            return ((line->dY ^ dX) & 0x80000000)? 1 : 0;
        }
        else
        {   // if left >= right return 1 else 0.
            return FixedMul(dY >> 8, line->dX >> 8) >=
                FixedMul(line->dY >> 8, dX >> 8);
        }
    }
}

/// @note Part of the Doomsday public API.
void P_MakeDivline(const LineDef* line, divline_t* dl)
{
    if(!line || !dl)
    {
        DEBUG_Message(("P_MakeDivline: Invalid arguments.\n"));
        return;
    }
    LineDef_SetDivline(line, dl);
}

/**
 * @return              Fractional intercept point along the first divline.
 */
float P_InterceptVector(const divline_t* v2, const divline_t* v1)
{
    float               frac = 0;
    fixed_t             den = FixedMul(v1->dY >> 8, v2->dX) -
        FixedMul(v1->dX >> 8, v2->dY);

    if(den)
    {
        fixed_t             f;

        f = FixedMul((v1->pos[VX] - v2->pos[VX]) >> 8, v1->dY) +
            FixedMul((v2->pos[VY] - v1->pos[VY]) >> 8, v1->dX);

        f = FixedDiv(f, den);

        frac = FIX2FLT(f);
    }

    return frac;
}

/// @note Part of the Doomsday public API
const divline_t* P_TraceLOS(void)
{
    static divline_t emptyLOS;
    if(theMap)
    {
        return GameMap_TraceLOS(theMap);
    }
    return &emptyLOS;
}

/// @note Part of the Doomsday public API
const TraceOpening* P_TraceOpening(void)
{
    static TraceOpening zeroOpening;
    if(theMap)
    {
        return GameMap_TraceOpening(theMap);
    }
    return &zeroOpening;
}

/// @note Part of the Doomsday public API
void P_SetTraceOpening(LineDef* lineDef)
{
    if(!theMap)
    {
        DEBUG_Message(("Warning: P_SetTraceOpening() attempted with no current map, ignoring."));
        return;
    }
    /// @fixme Do not assume linedef is from the CURRENT map.
    GameMap_SetTraceOpening(theMap, lineDef);
}

/// @note Part of the Doomsday public API
BspLeaf* P_BspLeafAtPointXY(float x, float y)
{
    if(theMap)
    {
        return GameMap_BspLeafAtPointXY(theMap, x, y);
    }
    return NULL;
}

boolean P_IsPointXYInBspLeaf(float x, float y, const BspLeaf* bspLeaf)
{
    Vertex* vi, *vj;
    HEdge* hedge;

    if(!bspLeaf || !bspLeaf->hedge) return false; // I guess?

    hedge = bspLeaf->hedge;
    do
    {
        HEdge* next = hedge->next;

        vi = hedge->HE_v1;
        vj = next->HE_v1;

        if(((vi->pos[VY] - y) * (vj->pos[VX] - vi->pos[VX]) -
            (vi->pos[VX] - x) * (vj->pos[VY] - vi->pos[VY])) < 0)
        {
            // Outside the BSP leaf's edges.
            return false;
        }
    } while((hedge = hedge->next) != bspLeaf->hedge);

    return true;
}

boolean P_IsPointXYInSector(float x, float y, const Sector* sector)
{
    BspLeaf* bspLeaf;
    if(!sector) return false; // I guess?

    /// @fixme Do not assume @a sector is from the current map.
    bspLeaf = GameMap_BspLeafAtPointXY(theMap, x, y);
    if(bspLeaf->sector != sector) return false;

    return P_IsPointXYInBspLeaf(x, y, bspLeaf);
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
int P_MobjUnlink(mobj_t* mo)
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
 * @important Caller must ensure a mobj is linked only once to any given linedef.
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
    GameMap* map;
    mobj_t* mo;
    AABoxf box;
} linelinker_data_t;

/**
 * The given line might cross the mobj. If necessary, link the mobj into
 * the line's mobj link ring.
 */
int PIT_LinkToLines(LineDef* ld, void* parameters)
{
    linelinker_data_t* p = parameters;
    assert(p);

    // Do the bounding boxes intercept?
    if(p->box.minX >= ld->aaBox.maxX ||
       p->box.minY >= ld->aaBox.maxY ||
       p->box.maxX <= ld->aaBox.minX ||
       p->box.maxY <= ld->aaBox.minY) return false;

    // Line does not cross the mobj's bounding box?
    if(P_BoxOnLineSide(&p->box, ld) != -1) return false;

    // One sided lines will not be linked to because a mobj can't legally cross one.
    if(!ld->L_frontside || !ld->L_backside) return false;

    GameMap_LinkMobjToLineDef(p->map, p->mo, ld);
    return false;
}

/**
 * @important Caller must ensure that the mobj is currently unlinked.
 */
void GameMap_LinkMobjToLineDefs(GameMap* map, mobj_t* mo)
{
    linelinker_data_t p;
    vec2f_t point;
    assert(map);

    // Get a new root node.
    mo->lineRoot = NP_New(&map->mobjNodes, NP_ROOT_NODE);

    // Set up a line iterator for doing the linking.
    p.map = map;
    p.mo = mo;
    V2f_Set(point, mo->pos[VX] - mo->radius, mo->pos[VY] - mo->radius);
    V2f_InitBox(p.box.arvec2, point);
    V2f_Set(point, mo->pos[VX] + mo->radius, mo->pos[VY] + mo->radius);
    V2f_AddToBox(p.box.arvec2, point);

    validCount++;
    P_AllLinesBoxIterator(&p.box, PIT_LinkToLines, (void*)&p);
}

/**
 * Links a mobj into both a block and a BSP leaf based on it's (x,y).
 * Sets mobj->bspLeaf properly. Calling with flags==0 only updates
 * the BspLeaf pointer. Can be called without unlinking first.
 */
void P_MobjLink(mobj_t* mo, byte flags)
{
    Sector* sec;

    // Link into the sector.
    mo->bspLeaf = P_BspLeafAtPointXY(mo->pos[VX], mo->pos[VY]);
    sec = mo->bspLeaf->sector;

    if(flags & DDLINK_SECTOR)
    {
        // Unlink from the current sector, if any.
        if(mo->sPrev)
            P_UnlinkMobjFromSector(mo);

        // Link the new mobj to the head of the list.
        // Prev pointers point to the pointer that points back to us.
        // (Which practically disallows traversing the list backwards.)

        if((mo->sNext = sec->mobjList))
            mo->sNext->sPrev = &mo->sNext;

        *(mo->sPrev = &sec->mobjList) = mo;
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
        ddplayer_t* player = mo->dPlayer;

        player->inVoid = true;
        if(P_IsPointXYInSector(player->mo->pos[VX],
                              player->mo->pos[VY],
                              player->mo->bspLeaf->sector) &&
           (player->mo->pos[VZ] < player->mo->bspLeaf->sector->SP_ceilvisheight + 4 &&
            player->mo->pos[VZ] >= player->mo->bspLeaf->sector->SP_floorvisheight))
            player->inVoid = false;
    }
}

/**
 * The callback function will be called once for each line that crosses
 * trough the object. This means all the lines will be two-sided.
 */
int GameMap_MobjLinesIterator(GameMap* map, mobj_t* mo,
    int (*callback) (LineDef*, void*), void* parameters)
{
    void* linkstore[MAXLINKED];
    void** end = linkstore, **it;
    nodeindex_t nix;
    linknode_t* tn;
    int result = false;
    assert(map);

    tn = map->mobjNodes.nodes;
    if(mo->lineRoot)
    {
        for(nix = tn[mo->lineRoot].next; nix != mo->lineRoot;
            nix = tn[nix].next)
            *end++ = tn[nix].ptr;

        DO_LINKS(it, end);
    }
    return result;
}

/**
 * Increment validCount before calling this routine. The callback function
 * will be called once for each sector the mobj is touching (totally or
 * partly inside). This is not a 3D check; the mobj may actually reside
 * above or under the sector.
 */
int GameMap_MobjSectorsIterator(GameMap* map, mobj_t* mo,
    int (*callback) (Sector*, void*), void* parameters)
{
    void* linkstore[MAXLINKED];
    void** end = linkstore, **it;
    nodeindex_t nix;
    linknode_t* tn;
    LineDef* ld;
    Sector* sec;
    int result = false;
    assert(map);

    tn = map->mobjNodes.nodes;

    // Always process the mobj's own sector first.
    *end++ = sec = mo->bspLeaf->sector;
    sec->validCount = validCount;

    // Any good lines around here?
    if(mo->lineRoot)
    {
        for(nix = tn[mo->lineRoot].next; nix != mo->lineRoot;
            nix = tn[nix].next)
        {
            ld = (LineDef*) tn[nix].ptr;

            // All these lines are two-sided. Try front side.
            sec = ld->L_frontsector;
            if(sec->validCount != validCount)
            {
                *end++ = sec;
                sec->validCount = validCount;
            }

            // And then the back side.
            if(ld->L_backside)
            {
                sec = ld->L_backsector;
                if(sec->validCount != validCount)
                {
                    *end++ = sec;
                    sec->validCount = validCount;
                }
            }
        }
    }

    DO_LINKS(it, end);
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

    root = map->lineLinks[GameMap_LineDefIndex(map, lineDef)], nix;
    ln = map->lineNodes.nodes;

    for(nix = ln[root].next; nix != root; nix = ln[nix].next)
        *end++ = ln[nix].ptr;

    DO_LINKS(it, end);
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

    DO_LINKS(it, end);
    return result;
}

/**
 * Looks for lines in the given block that intercept the given trace to add
 * to the intercepts list.
 * A line is crossed if its endpoints are on opposite sides of the trace.
 *
 * @return  Non-zero if current iteration should stop.
 */
int PIT_AddLineDefIntercepts(LineDef* lineDef, void* paramaters)
{
    /// @fixme Do not assume lineDef is from the current map.
    const divline_t* traceLOS = GameMap_TraceLOS(theMap);
    float distance;
    divline_t dl;
    int s1, s2;

    // Is this line crossed?
    // Avoid precision problems with two routines.
    if(traceLOS->dX >  FRACUNIT * 16 || traceLOS->dY >  FRACUNIT * 16 ||
       traceLOS->dX < -FRACUNIT * 16 || traceLOS->dY < -FRACUNIT * 16)
    {
        s1 = P_PointOnDivlineSide(lineDef->L_v1pos[VX], lineDef->L_v1pos[VY], traceLOS);
        s2 = P_PointOnDivlineSide(lineDef->L_v2pos[VX], lineDef->L_v2pos[VY], traceLOS);
    }
    else
    {
        s1 = P_PointXYOnLineDefSide(FIX2FLT(traceLOS->pos[VX]), FIX2FLT(traceLOS->pos[VY]), lineDef);
        s2 = P_PointXYOnLineDefSide(FIX2FLT(traceLOS->pos[VX] + traceLOS->dX),
                                    FIX2FLT(traceLOS->pos[VY] + traceLOS->dY), lineDef);
    }
    if(s1 == s2) return false;

    // Calculate interception point.
    P_MakeDivline(lineDef, &dl);
    distance = P_InterceptVector(traceLOS, &dl);
    // On the correct side of the trace origin?
    if(!(distance < 0))
    {
        P_AddIntercept(ICPT_LINE, distance, lineDef);
    }
    // Continue iteration.
    return false;
}

int PIT_AddMobjIntercepts(mobj_t* mobj, void* paramaters)
{
    const divline_t* traceLOS;
    vec2f_t from, to;
    float distance;
    divline_t dl;
    int s1, s2;

    if(mobj->dPlayer && (mobj->dPlayer->flags & DDPF_CAMERA))
        return false; // $democam: ssshh, keep going, we're not here...

    // Check a corner to corner crossection for hit.
    /// @fixme Do not assume mobj is from the current map.
    traceLOS = GameMap_TraceLOS(theMap);
    if((traceLOS->dX ^ traceLOS->dY) > 0)
    {
        // \ Slope
        V2f_Set(from, mobj->pos[VX] - mobj->radius,
                      mobj->pos[VY] + mobj->radius);
        V2f_Set(to,   mobj->pos[VX] + mobj->radius,
                      mobj->pos[VY] - mobj->radius);
    }
    else
    {
        // / Slope
        V2f_Set(from, mobj->pos[VX] - mobj->radius,
                      mobj->pos[VY] - mobj->radius);
        V2f_Set(to,   mobj->pos[VX] + mobj->radius,
                      mobj->pos[VY] + mobj->radius);
    }

    // Is this line crossed?
    s1 = P_PointOnDivlineSide(from[VX], from[VY], traceLOS);
    s2 = P_PointOnDivlineSide(to[VX], to[VY], traceLOS);
    if(s1 == s2) return false;

    // Calculate interception point.
    dl.pos[VX] = FLT2FIX(from[VX]);
    dl.pos[VY] = FLT2FIX(from[VY]);
    dl.dX = FLT2FIX(to[VX] - from[VX]);
    dl.dY = FLT2FIX(to[VY] - from[VY]);
    distance = P_InterceptVector(traceLOS, &dl);
    // On the correct side of the trace origin?
    if(!(distance < 0))
    {
        P_AddIntercept(ICPT_MOBJ, distance, mobj);
    }
    // Continue iteration.
    return false;
}

void P_LinkMobjInBlockmap(mobj_t* mo)
{
    /// @fixme Do not assume mobj is from the current map.
    if(!theMap) return;
    GameMap_LinkMobj(theMap, mo);
}

boolean P_UnlinkMobjFromBlockmap(mobj_t* mo)
{
    /// @fixme Do not assume mobj is from the current map.
    if(!theMap) return false;
    return GameMap_UnlinkMobj(theMap, mo);
}

void P_LinkMobjToLineDefs(mobj_t* mo)
{
    /// @fixme Do not assume mobj is from the current map.
    if(!theMap) return;
    GameMap_LinkMobjToLineDefs(theMap, mo);
}

boolean P_UnlinkMobjFromLineDefs(mobj_t* mo)
{
    /// @fixme Do not assume mobj is from the current map.
    if(!theMap) return false;
    return GameMap_UnlinkMobjFromLineDefs(theMap, mo);
}

/**
 * The callback function will be called once for each line that crosses
 * trough the object. This means all the lines will be two-sided.
 */
int P_MobjLinesIterator(mobj_t* mo, int (*callback) (LineDef*, void*), void* parameters)
{
    /// @fixme Do not assume mobj is in the current map.
    if(!theMap) return false; // Continue iteration.
    return GameMap_MobjLinesIterator(theMap, mo, callback, parameters);
}

/**
 * Increment validCount before calling this routine. The callback function
 * will be called once for each sector the mobj is touching (totally or
 * partly inside). This is not a 3D check; the mobj may actually reside
 * above or under the sector.
 */
int P_MobjSectorsIterator(mobj_t* mo, int (*callback) (Sector*, void*), void* parameters)
{
    /// @fixme Do not assume mobj is in the current map.
    if(!theMap) return false; // Continue iteration.
    return GameMap_MobjSectorsIterator(theMap, mo, callback, parameters);
}

int P_LineMobjsIterator(LineDef* lineDef, int (*callback) (mobj_t*, void*), void* parameters)
{
    /// @fixme Do not assume lineDef is in the current map.
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
int P_SectorTouchingMobjsIterator(Sector* sector, int (*callback) (mobj_t*, void*), void* parameters)
{
    /// @fixme Do not assume sector is in the current map.
    if(!theMap) return false; // Continue iteration.
    return GameMap_SectorTouchingMobjsIterator(theMap, sector, callback, parameters);
}

/// @note Part of the Doomsday public API.
int P_MobjsBoxIterator(const AABoxf* box, int (*callback) (mobj_t*, void*), void* parameters)
{
    if(!theMap) return false; // Continue iteration.
    return GameMap_MobjsBoxIterator(theMap, box, callback, parameters);
}

/// @note Part of the Doomsday public API.
int P_PolyobjsBoxIterator(const AABoxf* box, int (*callback) (struct polyobj_s*, void*), void* parameters)
{
    if(!theMap) return false; // Continue iteration.
    return GameMap_PolyobjsBoxIterator(theMap, box, callback, parameters);
}

/// @note Part of the Doomsday public API.
int P_LinesBoxIterator(const AABoxf* box, int (*callback) (LineDef*, void*), void* parameters)
{
    if(!theMap) return false; // Continue iteration.
    return GameMap_LineDefsBoxIterator(theMap, box, callback, parameters);
}

/// @note Part of the Doomsday public API.
int P_PolyobjLinesBoxIterator(const AABoxf* box, int (*callback) (LineDef*, void*), void* parameters)
{
    if(!theMap) return false; // Continue iteration.
    return GameMap_PolyobjLinesBoxIterator(theMap, box, callback, parameters);
}

/// @note Part of the Doomsday public API.
int P_BspLeafsBoxIterator(const AABoxf* box, Sector* sector,
    int (*callback) (BspLeaf*, void*), void* parameters)
{
    if(!theMap) return false; // Continue iteration.
    return GameMap_BspLeafsBoxIterator(theMap, box, sector, callback, parameters);
}

/// @note Part of the Doomsday public API.
int P_AllLinesBoxIterator(const AABoxf* box, int (*callback) (LineDef*, void*), void* parameters)
{
    if(!theMap) return false; // Continue iteration.
    return GameMap_AllLineDefsBoxIterator(theMap, box, callback, parameters);
}

/// @note Part of the Doomsday public API.
int P_PathTraverse2(float const from[2], float const to[2], int flags, traverser_t callback,
    void* paramaters)
{
    if(!theMap) return false; // Continue iteration.
    return GameMap_PathTraverse2(theMap, from, to, flags, callback, paramaters);
}

/// @note Part of the Doomsday public API.
int P_PathTraverse(float const from[2], float const to[2], int flags, traverser_t callback)
{
    if(!theMap) return false; // Continue iteration.
    return GameMap_PathTraverse(theMap, from, to, flags, callback);
}

/// @note Part of the Doomsday public API.
int P_PathXYTraverse2(float fromX, float fromY, float toX, float toY, int flags,
    traverser_t callback, void* paramaters)
{
    if(!theMap) return false; // Continue iteration.
    return GameMap_PathXYTraverse2(theMap, fromX, fromY, toX, toY, flags, callback, paramaters);
}

/// @note Part of the Doomsday public API.
int P_PathXYTraverse(float fromX, float fromY, float toX, float toY, int flags,
    traverser_t callback)
{
    if(!theMap) return false; // Continue iteration.
    return GameMap_PathXYTraverse(theMap, fromX, fromY, toX, toY, flags, callback);
}

/// @note Part of the Doomsday public API.
boolean P_CheckLineSight(const float from[3], const float to[3], float bottomSlope,
    float topSlope, int flags)
{
    if(!theMap) return false; // I guess?
    return GameMap_CheckLineSight(theMap, from, to, bottomSlope, topSlope, flags);
}
