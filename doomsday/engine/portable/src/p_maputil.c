/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2009 Daniel Swanson <danij@dengine.net>
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
#include "de_play.h"
#include "de_refresh.h"
#include "de_misc.h"
#include "p_bmap.h"

// MACROS ------------------------------------------------------------------

#define ORDER(x,y,a,b)  ( (x)<(y)? ((a)=(x),(b)=(y)) : ((b)=(x),(a)=(y)) )

// Linkstore is list of pointers gathered when iterating stuff.
// This is pretty much the only way to avoid *all* potential problems
// caused by callback routines behaving badly (moving or destroying
// mobjs). The idea is to get a snapshot of all the objects being
// iterated before any callbacks are called. The hardcoded limit is
// a drag, but I'd like to see you iterating 2048 mobjs/lines in one block.

#define MAXLINKED           2048
#define DO_LINKS(it, end)   for(it = linkstore; it < end; it++) \
                                if(!func(*it, data)) return false;

// TYPES -------------------------------------------------------------------

typedef struct {
    mobj_t*         mo;
    vec2_t          box[2];
} linelinker_data_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

float opentop, openbottom, openrange;
float lowfloor;

divline_t traceLOS;
boolean earlyout;
int ptflags;

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
 * Returns a two-component float unit vector parallel to the line.
 */
void P_LineUnitVector(linedef_t* line, float* unitvec)
{
    float               len = M_ApproxDistancef(line->dX, line->dY);

    if(len)
    {
        unitvec[VX] = line->dX / len;
        unitvec[VY] = line->dY / len;
    }
    else
    {
        unitvec[VX] = unitvec[VY] = 0;
    }
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

/**
 * Lines start, end and fdiv must intersect.
 */
#ifdef _MSC_VER
#  pragma optimize("g", off)
#endif
float P_FloatInterceptVertex(fvertex_t* start, fvertex_t* end,
                             fdivline_t* fdiv, fvertex_t* inter)
{
    float               ax = start->pos[VX], ay = start->pos[VY];
    float               bx = end->pos[VX], by = end->pos[VY];
    float               cx = fdiv->pos[VX], cy = fdiv->pos[VY];
    float               dx = cx + fdiv->dX, dy = cy + fdiv->dY;

    /*
           (YA-YC)(XD-XC)-(XA-XC)(YD-YC)
       r = -----------------------------  (eqn 1)
           (XB-XA)(YD-YC)-(YB-YA)(XD-XC)
     */

    float               r =
        ((ay - cy) * (dx - cx) - (ax - cx) * (dy - cy)) /
        ((bx - ax) * (dy - cy) - (by - ay) * (dx - cx));
    /*
       XI = XA+r(XB-XA)
       YI = YA+r(YB-YA)
     */
    inter->pos[VX] = ax + r * (bx - ax);
    inter->pos[VY] = ay + r * (by - ay);
    return r;
}

/**
 * @return              Non-zero if the point is on the right side of the
 *                      specified line.
 */
int P_PointOnLineSide(float x, float y, float lX, float lY, float lDX,
                      float lDY)
{
    /*
       (AY-CY)(BX-AX)-(AX-CX)(BY-AY)
       s = -----------------------------
       L**2

       If s<0      C is left of AB (you can just check the numerator)
       If s>0      C is right of AB
       If s=0      C is on AB
     */
    return ((lY - y) * lDX - (lX - x) * lDY >= 0);
}
#ifdef _MSC_VER
#  pragma optimize("", on)
#endif

/**
 * Determines on which side of dline the point is. Returns true if the
 * point is on the line or on the right side.
 */
int P_PointOnDivLineSidef(fvertex_t* pnt, fdivline_t* dline)
{
    return !P_PointOnLineSide(pnt->pos[VX], pnt->pos[VY], dline->pos[VX],
                              dline->pos[VY], dline->dX, dline->dY);
}

/**
 * @return              Non-zero if the point is on the right side of the
 *                      specified line.
 */
int P_PointOnLinedefSide(float x, float y, const linedef_t* line)
{
    return !P_PointOnLineSide(x, y, line->L_v1pos[VX], line->L_v1pos[VY],
                              line->dX, line->dY);
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

/**
 * Check the spatial relationship between the given box and a partitioning
 * line.
 *
 * @param bbox          Ptr to the box being tested.
 * @param lineSX        X coordinate of the start of the line.
 * @param lineSY        Y coordinate of the end of the line.
 * @param lineDX        X delta of the line (slope).
 * @param lineDY        Y delta of the line (slope).
 * @param linePerp      Perpendicular d of the line.
 * @param lineLength    Length of the line.
 * @param epsilon       Points within this distance will be considered equal.
 *
 * @return              @c <0= bbox is wholly on the left side.
 *                      @c  0= line intersects bbox.
 *                      @c >0= bbox wholly on the right side.
 */
int P_BoxOnLineSide3(const int bbox[4], double lineSX, double lineSY,
                     double lineDX, double lineDY, double linePerp,
                     double lineLength, double epsilon)
{
#define IFFY_LEN        4.0

    int                 p1, p2;
    double              x1 = (double)bbox[BOXLEFT]   - IFFY_LEN * 1.5;
    double              y1 = (double)bbox[BOXBOTTOM] - IFFY_LEN * 1.5;
    double              x2 = (double)bbox[BOXRIGHT]  + IFFY_LEN * 1.5;
    double              y2 = (double)bbox[BOXTOP]    + IFFY_LEN * 1.5;

    if(lineDX == 0)
    {   // Horizontal.
        p1 = (x1 > lineSX? +1 : -1);
        p2 = (x2 > lineSX? +1 : -1);

        if(lineDY < 0)
        {
            p1 = -p1;
            p2 = -p2;
        }
    }
    else if(lineDY == 0)
    {   // Vertical.
        p1 = (y1 < lineSY? +1 : -1);
        p2 = (y2 < lineSY? +1 : -1);

        if(lineDX < 0)
        {
            p1 = -p1;
            p2 = -p2;
        }
    }
    else if(lineDX * lineDY > 0)
    {   // Positive slope.
        p1 = P_PointOnLinedefSide2(x1, y2, lineDX, lineDY, linePerp, lineLength, epsilon);
        p2 = P_PointOnLinedefSide2(x2, y1, lineDX, lineDY, linePerp, lineLength, epsilon);
    }
    else
    {   // Negative slope.
        p1 = P_PointOnLinedefSide2(x1, y1, lineDX, lineDY, linePerp, lineLength, epsilon);
        p2 = P_PointOnLinedefSide2(x2, y2, lineDX, lineDY, linePerp, lineLength, epsilon);
    }

    if(p1 == p2)
        return p1;

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
                     const linedef_t* ld)
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
        a = P_PointOnLinedefSide(xl, yh, ld);
        b = P_PointOnLinedefSide(xh, yl, ld);
        break;

    case ST_NEGATIVE:
        a = P_PointOnLinedefSide(xh, yh, ld);
        b = P_PointOnLinedefSide(xl, yl, ld);
        break;
    }

    if(a == b)
        return a;

    return -1;
}

int P_BoxOnLineSide(const float* box, const linedef_t* ld)
{
    return P_BoxOnLineSide2(box[BOXLEFT], box[BOXRIGHT],
                            box[BOXBOTTOM], box[BOXTOP], ld);
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

void P_MakeDivline(linedef_t* li, divline_t* dl)
{
    vertex_t*           vtx = li->L_v1;

    dl->pos[VX] = FLT2FIX(vtx->V_pos[VX]);
    dl->pos[VY] = FLT2FIX(vtx->V_pos[VY]);
    dl->dX = FLT2FIX(li->dX);
    dl->dY = FLT2FIX(li->dY);
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

/**
 * Sets opentop and openbottom to the window through a two sided line.
 * \fixme $nplanes.
 */
void P_LineOpening(linedef_t* linedef)
{
    sector_t*           front, *back;

    if(!linedef->L_backside)
    {   // Single sided line.
        openrange = 0;
        return;
    }

    front = linedef->L_frontsector;
    back = linedef->L_backsector;

    if(front->SP_ceilheight < back->SP_ceilheight)
        opentop = front->SP_ceilheight;
    else
        opentop = back->SP_ceilheight;

    if(front->SP_floorheight > back->SP_floorheight)
    {
        openbottom = front->SP_floorheight;
        lowfloor = back->SP_floorheight;
    }
    else
    {
        openbottom = back->SP_floorheight;
        lowfloor = front->SP_floorheight;
    }

    openrange = opentop - openbottom;
}

/**
 * Two links to update:
 * 1) The link to us from the previous node (sprev, always set) will
 *    be modified to point to the node following us.
 * 2) If there is a node following us, set its sprev pointer to point
 *    to the pointer that points back to it (our sprev, just modified).
 */
boolean P_UnlinkFromSector(mobj_t* mo)
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
 * Unlinks the mobj from all the lines it's been linked to. Can be called
 * without checking that the list does indeed contain lines.
 */
boolean P_UnlinkFromLines(mobj_t* mo)
{
    linknode_t*         tn = mobjNodes->nodes;
    nodeindex_t         nix;

    // Try unlinking from lines.
    if(!mo->lineRoot)
        return false; // A zero index means it's not linked.

    // Unlink from each line.
    for(nix = tn[mo->lineRoot].next; nix != mo->lineRoot;
        nix = tn[nix].next)
    {
        // Data is the linenode index that corresponds this mobj.
        NP_Unlink(lineNodes, tn[nix].data);
        // We don't need these nodes any more, mark them as unused.
        // Dismissing is a macro.
        NP_Dismiss(lineNodes, tn[nix].data);
        NP_Dismiss(mobjNodes, nix);
    }

    // The mobj no longer has a line ring.
    NP_Dismiss(mobjNodes, mo->lineRoot);
    mo->lineRoot = 0;

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
    int                 links = 0;

    if(P_UnlinkFromSector(mo))
        links |= DDLINK_SECTOR;
    if(P_BlockmapUnlinkMobj(BlockMap, mo))
        links |= DDLINK_BLOCKMAP;
    if(!P_UnlinkFromLines(mo))
        links |= DDLINK_NOLINE;

    return links;
}

/**
 * The given line might cross the mobj. If necessary, link the mobj into
 * the line's mobj link ring.
 */
boolean PIT_LinkToLines(linedef_t* ld, void* parm)
{
    linelinker_data_t*  data = parm;
    nodeindex_t         nix;

    if(data->box[1][VX] <= ld->bBox[BOXLEFT] ||
       data->box[0][VX] >= ld->bBox[BOXRIGHT] ||
       data->box[1][VY] <= ld->bBox[BOXBOTTOM] ||
       data->box[0][VY] >= ld->bBox[BOXTOP])
        // Bounding boxes do not overlap.
        return true;

    if(P_BoxOnLineSide2(data->box[0][VX], data->box[1][VX],
                        data->box[0][VY], data->box[1][VY], ld) != -1)
        // Line does not cross the mobj's bounding box.
        return true;

    // One sided lines will not be linked to because a mobj
    // can't legally cross one.
    if(!ld->L_frontside || !ld->L_backside)
        return true;

    // No redundant nodes will be creates since this routine is
    // called only once for each line.

    // Add a node to the mobj's ring.
    NP_Link(mobjNodes, nix = NP_New(mobjNodes, ld), data->mo->lineRoot);

    // Add a node to the line's ring. Also store the linenode's index
    // into the mobjring's node, so unlinking is easy.
    NP_Link(lineNodes, mobjNodes->nodes[nix].data =
            NP_New(lineNodes, data->mo), linelinks[GET_LINE_IDX(ld)]);

    return true;
}

/**
 * \pre The mobj must be currently unlinked.
 */
void P_LinkToLines(mobj_t* mo)
{
    linelinker_data_t   data;
    vec2_t              point;

    // Get a new root node.
    mo->lineRoot = NP_New(mobjNodes, NP_ROOT_NODE);

    // Set up a line iterator for doing the linking.
    data.mo = mo;
    V2_Set(point, mo->pos[VX] - mo->radius, mo->pos[VY] - mo->radius);
    V2_InitBox(data.box, point);
    V2_Set(point, mo->pos[VX] + mo->radius, mo->pos[VY] + mo->radius);
    V2_AddToBox(data.box, point);

    validCount++;
    P_AllLinesBoxIteratorv(data.box, PIT_LinkToLines, &data);
}

void P_MobjLinkToRing(mobj_t* mo, linkmobj_t** link)
{
    linkmobj_t*      tempLink;

    if(!(*link))
    {   // Create a new link at the current block cell.
        *link = Z_Malloc(sizeof(linkmobj_t), PU_MAP, 0);
        (*link)->next = NULL;
        (*link)->prev = NULL;
        (*link)->mobj = mo;
        return;
    }
    else
    {
        tempLink = *link;
        while(tempLink->next != NULL && tempLink->mobj != NULL)
        {
            tempLink = tempLink->next;
        }
    }

    if(tempLink->mobj == NULL)
    {
        tempLink->mobj = mo;
        return;
    }
    else
    {
        tempLink->next =
            Z_Malloc(sizeof(linkmobj_t), PU_MAP, 0);
        tempLink->next->next = NULL;
        tempLink->next->prev = tempLink;
        tempLink->next->mobj = mo;
    }
}

/**
 * Unlink the given mobj from the specified block ring (if indeed linked).
 *
 * @param mo            Ptr to the mobj to unlink.
 * @return              @c true, iff the mobj was linked to the ring and was
 *                      successfully unlinked.
 */
boolean P_MobjUnlinkFromRing(mobj_t* mo, linkmobj_t** list)
{
    linkmobj_t*         iter = *list;

    while(iter != NULL && iter->mobj != mo)
    {
        iter = iter->next;
    }

    if(iter != NULL)
    {
        iter->mobj = NULL;
        return true; // Mobj was unlinked.
    }

    return false; // Mobj was not linked.
}

/**
 * Links a mobj into both a block and a subsector based on it's (x,y).
 * Sets mobj->subsector properly. Calling with flags==0 only updates
 * the subsector pointer. Can be called without unlinking first.
 */
void P_MobjLink(mobj_t* mo, byte flags)
{
    sector_t*           sec;

    // Link into the sector.
    mo->subsector = R_PointInSubsector(mo->pos[VX], mo->pos[VY]);
    sec = mo->subsector->sector;

    if(flags & DDLINK_SECTOR)
    {
        // Unlink from the current sector, if any.
        if(mo->sPrev)
            P_UnlinkFromSector(mo);

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
        P_BlockmapUnlinkMobj(BlockMap, mo);
        P_BlockmapLinkMobj(BlockMap, mo);
    }

    // Link into lines.
    if(!(flags & DDLINK_NOLINE))
    {
        // Unlink from any existing lines.
        P_UnlinkFromLines(mo);

        // Link to all contacted lines.
        P_LinkToLines(mo);
    }

    // If this is a player - perform addtional tests to see if they have
    // entered or exited the void.
    if(mo->dPlayer)
    {
        ddplayer_t*         player = mo->dPlayer;

        player->inVoid = true;
        if(R_IsPointInSector2(player->mo->pos[VX],
                              player->mo->pos[VY],
                              player->mo->subsector->sector))
            player->inVoid = false;
    }
}

/**
 * The callback function will be called once for each line that crosses
 * trough the object. This means all the lines will be two-sided.
 */
boolean P_MobjLinesIterator(mobj_t* mo,
                            boolean (*func) (linedef_t*, void*),
                            void* data)
{
    void*               linkstore[MAXLINKED];
    void**              end = linkstore, **it;
    nodeindex_t         nix;
    linknode_t*         tn = mobjNodes->nodes;

    if(!mo->lineRoot)
        return true; // No lines to process.

    for(nix = tn[mo->lineRoot].next; nix != mo->lineRoot;
        nix = tn[nix].next)
        *end++ = tn[nix].ptr;

    DO_LINKS(it, end);
    return true;
}

/**
 * Increment validCount before calling this routine. The callback function
 * will be called once for each sector the mobj is touching (totally or
 * partly inside). This is not a 3D check; the mobj may actually reside
 * above or under the sector.
 */
boolean P_MobjSectorsIterator(mobj_t* mo,
                              boolean (*func) (sector_t*, void*),
                              void* data)
{
    void*               linkstore[MAXLINKED];
    void**              end = linkstore, **it;
    nodeindex_t         nix;
    linknode_t*         tn = mobjNodes->nodes;
    linedef_t*          ld;
    sector_t*           sec;

    // Always process the mobj's own sector first.
    *end++ = sec = mo->subsector->sector;
    sec->validCount = validCount;

    // Any good lines around here?
    if(mo->lineRoot)
    {
        for(nix = tn[mo->lineRoot].next; nix != mo->lineRoot;
            nix = tn[nix].next)
        {
            ld = (linedef_t *) tn[nix].ptr;

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
    return true;
}

boolean P_LineMobjsIterator(linedef_t* line,
                            boolean (*func) (mobj_t*, void*),
                            void* data)
{
    void*               linkstore[MAXLINKED];
    void**              end = linkstore, **it;
    nodeindex_t         root = linelinks[GET_LINE_IDX(line)], nix;
    linknode_t*         ln = lineNodes->nodes;

    for(nix = ln[root].next; nix != root; nix = ln[nix].next)
        *end++ = ln[nix].ptr;

    DO_LINKS(it, end);
    return true;
}

/**
 * Increment validCount before using this. 'func' is called for each mobj
 * that is (even partly) inside the sector. This is not a 3D test, the
 * mobjs may actually be above or under the sector.
 *
 * (Lovely name; actually this is a combination of SectorMobjs and
 * a bunch of LineMobjs iterations.)
 */
boolean P_SectorTouchingMobjsIterator(sector_t* sector,
                                      boolean (*func) (mobj_t*, void*),
                                      void* data)
{
    uint                i;
    void*               linkstore[MAXLINKED];
    void**              end = linkstore, **it;
    mobj_t*             mo;
    linedef_t*          li;
    nodeindex_t         root, nix;
    linknode_t*         ln = lineNodes->nodes;

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
        root = linelinks[GET_LINE_IDX(li)];
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
    return true;
}

boolean P_MobjsBoxIterator(const float box[4],
                           boolean (*func) (mobj_t*, void*),
                           void* data)
{
    vec2_t              bounds[2];

    bounds[0][VX] = box[BOXLEFT];
    bounds[0][VY] = box[BOXBOTTOM];
    bounds[1][VX] = box[BOXRIGHT];
    bounds[1][VY] = box[BOXTOP];

    return P_MobjsBoxIteratorv(bounds, func, data);
}

boolean P_MobjsBoxIteratorv(const arvec2_t box,
                            boolean (*func) (mobj_t*, void*),
                            void* data)
{
    uint                blockBox[4];

    P_BoxToBlockmapBlocks(BlockMap, blockBox, box);

    return P_BlockBoxMobjsIterator(BlockMap, blockBox, func, data);
}

boolean P_PolyobjsBoxIterator(const float box[4],
                              boolean (*func) (struct polyobj_s*, void*),
                              void* data)
{
    vec2_t              bounds[2];

    bounds[0][VX] = box[BOXLEFT];
    bounds[0][VY] = box[BOXBOTTOM];
    bounds[1][VX] = box[BOXRIGHT];
    bounds[1][VY] = box[BOXTOP];

    return P_PolyobjsBoxIteratorv(bounds, func, data);
}

/**
 * The validCount flags are used to avoid checking polys that are marked in
 * multiple mapblocks, so increment validCount before the first call, then
 * make one or more calls to it.
 */
boolean P_PolyobjsBoxIteratorv(const arvec2_t box,
                               boolean (*func) (struct polyobj_s*, void*),
                               void* data)
{
    uint                blockBox[4];

    // Blockcoords to check.
    P_BoxToBlockmapBlocks(BlockMap, blockBox, box);

    return P_BlockBoxPolyobjsIterator(BlockMap, blockBox, func, data);
}

boolean P_LinesBoxIterator(const float box[4],
                           boolean (*func) (linedef_t*, void*),
                           void* data)
{
    vec2_t              bounds[2];

    bounds[0][VX] = box[BOXLEFT];
    bounds[0][VY] = box[BOXBOTTOM];
    bounds[1][VX] = box[BOXRIGHT];
    bounds[1][VY] = box[BOXTOP];

    return P_LinesBoxIteratorv(bounds, func, data);
}

boolean P_LinesBoxIteratorv(const arvec2_t box,
                            boolean (*func) (linedef_t*, void*),
                            void* data)
{
    uint                blockBox[4];

    P_BoxToBlockmapBlocks(BlockMap, blockBox, box);

    return P_BlockBoxLinesIterator(BlockMap, blockBox, func, data);
}

/**
 * @return              @c false, if the iterator func returns @c false.
 */
boolean P_SubsectorsBoxIterator(const float box[4], sector_t* sector,
                                boolean (*func) (subsector_t*, void*),
                                void* parm)
{
    vec2_t              bounds[2];

    bounds[0][VX] = box[BOXLEFT];
    bounds[0][VY] = box[BOXBOTTOM];
    bounds[1][VX] = box[BOXRIGHT];
    bounds[1][VY] = box[BOXTOP];

    return P_SubsectorsBoxIteratorv(bounds, sector, func, parm);
}

/**
 * Same as the fixed-point version of this routine, but the bounding box
 * is specified using an vec2_t array (see m_vector.c).
 */
boolean P_SubsectorsBoxIteratorv(const arvec2_t box, sector_t* sector,
                                 boolean (*func) (subsector_t*, void*),
                                 void* data)
{
    static int          localValidCount = 0;
    uint                blockBox[4];

    // This is only used here.
    localValidCount++;

    // Blockcoords to check.
    P_BoxToBlockmapBlocks(SSecBlockMap, blockBox, box);

    return P_BlockBoxSubsectorsIterator(SSecBlockMap, blockBox, sector,
                                        box, localValidCount, func, data);
}

boolean P_PolyobjLinesBoxIterator(const float box[4],
                                  boolean (*func) (linedef_t*, void*),
                                  void* data)
{
    vec2_t              bounds[2];

    bounds[0][VX] = box[BOXLEFT];
    bounds[0][VY] = box[BOXBOTTOM];
    bounds[1][VX] = box[BOXRIGHT];
    bounds[1][VY] = box[BOXTOP];

    return P_PolyobjLinesBoxIteratorv(bounds, func, data);
}

boolean P_PolyobjLinesBoxIteratorv(const arvec2_t box,
                                   boolean (*func) (linedef_t*, void*),
                                   void* data)
{
    uint                blockBox[4];

    P_BoxToBlockmapBlocks(BlockMap, blockBox, box);

    return P_BlockBoxPolyobjLinesIterator(BlockMap, blockBox, func, data);
}


/**
 * The validCount flags are used to avoid checking lines that are marked
 * in multiple mapblocks, so increment validCount before the first call
 * to P_BlockmapLinesIterator, then make one or more calls to it.
 */
boolean P_AllLinesBoxIterator(const float box[4],
                              boolean (*func) (linedef_t*, void*),
                              void* data)
{
    vec2_t              bounds[2];

    bounds[0][VX] = box[BOXLEFT];
    bounds[0][VY] = box[BOXBOTTOM];
    bounds[1][VX] = box[BOXRIGHT];
    bounds[1][VY] = box[BOXTOP];

    return P_AllLinesBoxIteratorv(bounds, func, data);
}

/**
 * The validCount flags are used to avoid checking lines that are marked
 * in multiple mapblocks, so increment validCount before the first call
 * to P_BlockmapLinesIterator, then make one or more calls to it.
 */
boolean P_AllLinesBoxIteratorv(const arvec2_t box,
                               boolean (*func) (linedef_t*, void*),
                               void* data)
{
    if(numPolyObjs > 0)
    {
        if(!P_PolyobjLinesBoxIteratorv(box, func, data))
            return false;
    }

    return P_LinesBoxIteratorv(box, func, data);
}

/**
 * Looks for lines in the given block that intercept the given trace to add
 * to the intercepts list
 * A line is crossed if its endpoints are on opposite sides of the trace.
 *
 * @return              @c true if earlyout and a solid line hit.
 */
boolean PIT_AddLineIntercepts(linedef_t* ld, void* data)
{
    int                 s[2];
    float               frac;
    divline_t           dl;

    // Avoid precision problems with two routines.
    if(traceLOS.dX > FRACUNIT * 16 || traceLOS.dY > FRACUNIT * 16 ||
       traceLOS.dX < -FRACUNIT * 16 || traceLOS.dY < -FRACUNIT * 16)
    {
        s[0] = P_PointOnDivlineSide(ld->L_v1pos[VX],
                                    ld->L_v1pos[VY], &traceLOS);
        s[1] = P_PointOnDivlineSide(ld->L_v2pos[VX],
                                    ld->L_v2pos[VY], &traceLOS);
    }
    else
    {
        s[0] = P_PointOnLinedefSide(FIX2FLT(traceLOS.pos[VX]),
                                 FIX2FLT(traceLOS.pos[VY]), ld);
        s[1] = P_PointOnLinedefSide(FIX2FLT(traceLOS.pos[VX] + traceLOS.dX),
                                 FIX2FLT(traceLOS.pos[VY] + traceLOS.dY), ld);
    }

    if(s[0] == s[1])
        return true; // Line isn't crossed.

    // Hit the line.
    P_MakeDivline(ld, &dl);
    frac = P_InterceptVector(&traceLOS, &dl);
    if(frac < 0)
        return true; // Behind source.

    // Try to early out the check.
    if(earlyout && frac < 1 && !ld->L_backside)
        return false; // Stop iteration.

    P_AddIntercept(frac, true, ld);

    return true; // Continue iteration.
}

boolean PIT_AddMobjIntercepts(mobj_t* mo, void* data)
{
    float               x1, y1, x2, y2;
    int                 s[2];
    divline_t           dl;
    float               frac;

    if(mo->dPlayer && (mo->dPlayer->flags & DDPF_CAMERA))
        return true; // $democam: ssshh, keep going, we're not here...

    // Check a corner to corner crossection for hit.
    if((traceLOS.dX ^ traceLOS.dY) > 0)
    {
        x1 = mo->pos[VX] - mo->radius;
        y1 = mo->pos[VY] + mo->radius;

        x2 = mo->pos[VX] + mo->radius;
        y2 = mo->pos[VY] - mo->radius;
    }
    else
    {
        x1 = mo->pos[VX] - mo->radius;
        y1 = mo->pos[VY] - mo->radius;

        x2 = mo->pos[VX] + mo->radius;
        y2 = mo->pos[VY] + mo->radius;
    }

    s[0] = P_PointOnDivlineSide(x1, y1, &traceLOS);
    s[1] = P_PointOnDivlineSide(x2, y2, &traceLOS);
    if(s[0] == s[1])
        return true; // Line isn't crossed.

    dl.pos[VX] = FLT2FIX(x1);
    dl.pos[VY] = FLT2FIX(y1);
    dl.dX = FLT2FIX(x2 - x1);
    dl.dY = FLT2FIX(y2 - y1);

    frac = P_InterceptVector(&traceLOS, &dl);
    if(frac < 0)
        return true; // Behind source.

    P_AddIntercept(frac, false, mo);

    return true; // Keep going.
}

/**
 * Traces a line from x1,y1 to x2,y2, calling the traverser function for each
 * Returns true if the traverser function returns true for all lines
 */
boolean P_PathTraverse(float x1, float y1, float x2, float y2,
                       int flags, boolean (*trav) (intercept_t*))
{
    float               origin[2], dest[2];
    uint                originBlock[2], destBlock[2];
    gamemap_t*          map = P_GetCurrentMap();
    vec2_t              min, max;

    V2_Set(origin, x1, y1);
    V2_Set(dest, x2, y2);

    P_GetBlockmapBounds(map->blockMap, min, max);

    if(!(origin[VX] >= min[VX] && origin[VX] <= max[VX] &&
         origin[VY] >= min[VY] && origin[VY] <= max[VY]))
    {   // Origin is outside the blockmap (really? very unusual...)
        return false;
    }

    // Check the easy case of a path that lies completely outside the bmap.
    if((origin[VX] < min[VX] && dest[VX] < min[VX]) ||
       (origin[VX] > max[VX] && dest[VX] > max[VX]) ||
       (origin[VY] < min[VY] && dest[VY] < min[VY]) ||
       (origin[VY] > max[VY] && dest[VY] > max[VY]))
    {   // Nothing intercepts outside the blockmap!
        return false;
    }

    if((FLT2FIX(origin[VX] - min[VX]) & (MAPBLOCKSIZE - 1)) == 0)
        origin[VX] += 1; // Don't side exactly on a line.
    if((FLT2FIX(origin[VY] - min[VY]) & (MAPBLOCKSIZE - 1)) == 0)
        origin[VY] += 1; // Don't side exactly on a line.

    traceLOS.pos[VX] = FLT2FIX(origin[VX]);
    traceLOS.pos[VY] = FLT2FIX(origin[VY]);
    traceLOS.dX = FLT2FIX(dest[VX] - origin[VX]);
    traceLOS.dY = FLT2FIX(dest[VY] - origin[VY]);

    /**
     * It is possible that one or both points are outside the blockmap.
     * Clip path so that dest is within the AABB of the blockmap (note we
     * would have already abandoned if origin lay outside. Also, to avoid
     * potential rounding errors which might occur when determining the
     * blocks later, we will shrink the bbox slightly first.
     */

    if(!(dest[VX] >= min[VX] && dest[VX] <= max[VX] &&
         dest[VY] >= min[VY] && dest[VY] <= max[VY]))
    {   // Dest is outside the blockmap.
        float               ab;
        vec2_t              bbox[4], point;

        V2_Set(bbox[0], min[VX] + 1, min[VY] + 1);
        V2_Set(bbox[1], min[VX] + 1, max[VY] - 1);
        V2_Set(bbox[2], max[VX] - 1, max[VY] - 1);
        V2_Set(bbox[3], max[VX] - 1, min[VY] + 1);

        ab = V2_Intercept(origin, dest, bbox[0], bbox[1], point);
        if(ab >= 0 && ab <= 1)
            V2_Copy(dest, point);

        ab = V2_Intercept(origin, dest, bbox[1], bbox[2], point);
        if(ab >= 0 && ab <= 1)
            V2_Copy(dest, point);

        ab = V2_Intercept(origin, dest, bbox[2], bbox[3], point);
        if(ab >= 0 && ab <= 1)
            V2_Copy(dest, point);

        ab = V2_Intercept(origin, dest, bbox[3], bbox[0], point);
        if(ab >= 0 && ab <= 1)
            V2_Copy(dest, point);
    }

    if(!(P_ToBlockmapBlockIdx(map->blockMap, originBlock, origin) &&
         P_ToBlockmapBlockIdx(map->blockMap, destBlock, dest)))
    {   // Shouldn't reach here due to the clipping above.
        return false;
    }

    earlyout = flags & PT_EARLYOUT;

    validCount++;
    P_ClearIntercepts();

    V2_Subtract(origin, origin, min);
    V2_Subtract(dest, dest, min);

    if(!P_BlockPathTraverse(BlockMap, originBlock, destBlock, origin, dest,
                            flags, trav))
        return false; // Early out.

    // Go through the sorted list.
    return P_TraverseIntercepts(trav, 1.0f);
}
