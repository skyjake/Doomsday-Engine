/**\file
 *\section Copyright and License Summary
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2006 Jaakko Keränen <skyjake@dengine.net>
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

/*
 * p_maputil.c: Map Utility Routines
 */

// HEADER FILES ------------------------------------------------------------

#include <math.h>

#include "de_base.h"
#include "de_play.h"
#include "de_refresh.h"
#include "de_misc.h"

// MACROS ------------------------------------------------------------------

#define ORDER(x,y,a,b)  ( (x)<(y)? ((a)=(x),(b)=(y)) : ((b)=(x),(a)=(y)) )

// Linkstore is list of pointers gathered when iterating stuff.
// This is pretty much the only way to avoid *all* potential problems
// caused by callback routines behaving badly (moving or destroying
// things). The idea is to get a snapshot of all the objects being
// iterated before any callbacks are called. The hardcoded limit is
// a drag, but I'd like to see you iterating 2048 things/lines in one block.

#define MAXLINKED           2048
#define DO_LINKS(it, end)   for(it = linkstore; it < end; it++) \
                                if(!func(*it, data)) return false;

// TYPES -------------------------------------------------------------------

typedef struct {
    mobj_t *thing;
    fixed_t box[4];
} linelinker_data_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

float opentop, openbottom, openrange;
float lowfloor;

divline_t trace;
boolean earlyout;
int     ptflags;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

float P_AccurateDistance(fixed_t dx, fixed_t dy)
{
    float   fx = FIX2FLT(dx), fy = FIX2FLT(dy);

    return (float) sqrt(fx * fx + fy * fy);
}

float P_AccurateDistancef(float dx, float dy)
{
    return (float) sqrt(dx * dx + dy * dy);
}

/*
 * Gives an estimation of distance (not exact).
 */
fixed_t P_ApproxDistance(fixed_t dx, fixed_t dy)
{
    dx = abs(dx);
    dy = abs(dy);
    return dx + dy - ((dx < dy ? dx : dy) >> 1);
}

/*
 * Gives an estimation of 3D distance (not exact).
 * The Z axis aspect ratio is corrected.
 */
fixed_t P_ApproxDistance3(fixed_t dx, fixed_t dy, fixed_t dz)
{
    return P_ApproxDistance(P_ApproxDistance(dx, dy),
                            FixedMul(dz, 1.2f * FRACUNIT));
}

/*
 * Returns a two-component float unit vector parallel to the line.
 */
void P_LineUnitVector(line_t *line, float *unitvec)
{
    float   len = M_ApproxDistancef(line->dx, line->dy);

    if(len)
    {
        unitvec[VX] = line->dx / len;
        unitvec[VY] = line->dy / len;
    }
    else
    {
        unitvec[VX] = unitvec[VY] = 0;
    }
}

/*
 * Either end or fixpoint must be specified. The distance is measured
 * (approximately) in 3D. Start must always be specified.
 */
float P_MobjPointDistancef(mobj_t *start, mobj_t *end, float *fixpoint)
{
    if(!start)
        return 0;
    if(end)
    {
        // Start -> end.
        return
            FIX2FLT(P_ApproxDistance
                    (end->pos[VZ] - start->pos[VZ],
                     P_ApproxDistance(end->pos[VX] - start->pos[VX],
                                      end->pos[VY] - start->pos[VY])));
    }
    if(fixpoint)
    {
        float   sp[3];

        sp[0] = FIX2FLT(start->pos[VX]);
        sp[1] = FIX2FLT(start->pos[VY]),
        sp[2] = FIX2FLT(start->pos[VZ]);

        return M_ApproxDistancef(fixpoint[VZ] - sp[VZ],
                                 M_ApproxDistancef(fixpoint[VX] - sp[VX],
                                                   fixpoint[VY] - sp[VY]));
    }
    return 0;
}

/*
 * Determines on which side of dline the point is. Returns true if the
 * point is on the line or on the right side.
 */
#ifdef MSVC
#  pragma optimize("g", off)
#endif
int P_FloatPointOnLineSide(fvertex_t *pnt, fdivline_t *dline)
{
    /*
       (AY-CY)(BX-AX)-(AX-CX)(BY-AY)
       s = -----------------------------
       L**2

       If s<0      C is left of AB (you can just check the numerator)
       If s>0      C is right of AB
       If s=0      C is on AB
     */
    // We'll return false if the point c is on the left side.
    return ((dline->pos[VY] - pnt->pos[VY]) * dline->dx -
            (dline->pos[VX] - pnt->pos[VX]) * dline->dy >= 0);
}

/*
 * Lines start, end and fdiv must intersect.
 */
float P_FloatInterceptVertex(fvertex_t *start, fvertex_t *end,
                             fdivline_t *fdiv, fvertex_t *inter)
{
    float   ax = start->pos[VX], ay = start->pos[VY], bx = end->pos[VX], by = end->pos[VY];
    float   cx = fdiv->pos[VX], cy = fdiv->pos[VY], dx = cx + fdiv->dx, dy = cy + fdiv->dy;

    /*
       (YA-YC)(XD-XC)-(XA-XC)(YD-YC)
       r = -----------------------------  (eqn 1)
       (XB-XA)(YD-YC)-(YB-YA)(XD-XC)
     */

    float   r =
        ((ay - cy) * (dx - cx) -
         (ax - cx) * (dy - cy)) / ((bx - ax) * (dy - cy) - (by - ay) * (dx -
                                                                        cx));
    /*
       XI=XA+r(XB-XA)
       YI=YA+r(YB-YA)
     */
    inter->pos[VX] = ax + r * (bx - ax);
    inter->pos[VY] = ay + r * (by - ay);
    return r;
}
#ifdef MSVC
#  pragma optimize("", on)
#endif

/**
 * (0,1) = top left; (2,3) = bottom right
 */
void P_SectorBoundingBox(sector_t *sec, float *bbox)
{
    uint        i;
    vertex_t   *vtx;

    if(!sec->linecount)
        return;

    vtx = sec->Lines[0]->L_v1;
    bbox[BLEFT] = bbox[BRIGHT]  = vtx->pos[VX];
    bbox[BTOP]  = bbox[BBOTTOM] = vtx->pos[VY];

    for(i = 1; i < sec->linecount; ++i)
    {
        vtx = sec->Lines[i]->L_v1;

        if(vtx->pos[VX] < bbox[BLEFT])
            bbox[BLEFT]   = vtx->pos[VX];
        if(vtx->pos[VX] > bbox[BRIGHT])
            bbox[BRIGHT]  = vtx->pos[VX];
        if(vtx->pos[VY] < bbox[BTOP])
            bbox[BTOP]    = vtx->pos[VY];
        if(vtx->pos[VY] > bbox[BBOTTOM])
            bbox[BBOTTOM] = vtx->pos[VY];
    }
}

/*
 * Returns 0 or 1
 */
int P_PointOnLineSide(fixed_t x, fixed_t y, line_t *line)
{
    float       fx = FIX2FLT(x), fy = FIX2FLT(y);
    vertex_t   *vtx1 = line->L_v1;

    return  !line->dx ? fx <= vtx1->pos[VX] ? line->dy > 0 : line->dy <
        0 : !line->dy ? fy <= vtx1->pos[VY] ? line->dx < 0 : line->dx >
        0 : (fy - vtx1->pos[VY]) * line->dx >= line->dy * (fx - vtx1->pos[VX]);
}

/*
 * Considers the line to be infinite
 * Reformatted
 * Returns side 0 or 1, -1 if box crosses the line
 */
int P_BoxOnLineSide(fixed_t *tmbox, line_t *ld)
{
    int         p;

    switch(ld->slopetype)
    {

    default:                    // shut up compiler warnings -- killough
    case ST_HORIZONTAL:
    {
        fixed_t ly = FLT2FIX(ld->L_v1->pos[VY]);
        return (tmbox[BOXBOTTOM] > ly) ==
                (p = tmbox[BOXTOP] > ly) ? p ^ (ld->dx < 0) : -1;
    }
    case ST_VERTICAL:
    {
        fixed_t lx = FLT2FIX(ld->L_v1->pos[VX]);
        return (tmbox[BOXLEFT] < lx) ==
                (p = tmbox[BOXRIGHT] < lx) ? p ^ (ld->dy < 0) : -1;
    }
    case ST_POSITIVE:
        return P_PointOnLineSide(tmbox[BOXRIGHT], tmbox[BOXBOTTOM], ld) ==
                (p = P_PointOnLineSide(tmbox[BOXLEFT], tmbox[BOXTOP], ld)) ? p : -1;

    case ST_NEGATIVE:
        return (P_PointOnLineSide(tmbox[BOXLEFT], tmbox[BOXBOTTOM], ld)) ==
                (p = P_PointOnLineSide(tmbox[BOXRIGHT], tmbox[BOXTOP], ld)) ? p : -1;
    }
}

/*
 * Returns 0 or 1
 */
int P_PointOnDivlineSide(fixed_t x, fixed_t y, divline_t *line)
{
    return  !line->dx ? x <= line->pos[VX] ? line->dy > 0 : line->dy <
        0 : !line->dy ? y <= line->pos[VY] ? line->dx < 0 : line->dx >
        0 : (line->dy ^ line->dx ^ (x -= line->pos[VX]) ^ (y -= line->pos[VY])) <
        0 ? (line->dy ^ x) < 0 : FixedMul(y >> 8, line->dx >> 8) >=
        FixedMul(line->dy >> 8, x >> 8);
}

void P_MakeDivline(line_t *li, divline_t *dl)
{
    vertex_t    *vtx = li->L_v1;

    dl->pos[VX] = FLT2FIX(vtx->pos[VX]);
    dl->pos[VY] = FLT2FIX(vtx->pos[VY]);
    dl->dx = FLT2FIX(li->dx);
    dl->dy = FLT2FIX(li->dy);
}

/*
 * Returns the fractional intercept point along the first divline
 *
 * This is only called by the addthings and addlines traversers
 */
fixed_t P_InterceptVector(divline_t *v2, divline_t *v1)
{
    fixed_t den =
        FixedMul(v1->dy >> 8, v2->dx) - FixedMul(v1->dx >> 8, v2->dy);
    return den ?
        FixedDiv((FixedMul((v1->pos[VX] - v2->pos[VX]) >> 8, v1->dy) +
                  FixedMul((v2->pos[VY] - v1->pos[VY]) >> 8, v1->dx)), den) : 0;
}

/*
 * Sets opentop and openbottom to the window through a two sided line
 * OPTIMIZE: keep this precalculated
 */
void P_LineOpening(line_t *linedef)
{
    sector_t *front, *back;

    if(!linedef->L_backside)
    {                           // single sided line
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

/*
 * The index is not checked.
 */
mobj_t *P_GetBlockRootIdx(int index)
{
    return (mobj_t *) (blockrings + index);
}

/*
 * Returns a pointer to the root linkmobj_t of the given mobj. If such
 * a block does not exist, NULL is returned. This routine is exported
 * for use in Games.
 */
mobj_t *P_GetBlockRoot(int blockx, int blocky)
{
    // We must be in the block map range.
    if(blockx < 0 || blocky < 0 || blockx >= bmapwidth || blocky >= bmapheight)
    {
        return NULL;
    }
    return (mobj_t *) (blockrings + (blocky * bmapwidth + blockx));
}

/*
 * Same as P_GetBlockRoot, but takes world coordinates as parameters.
 */
mobj_t *P_GetBlockRootXY(int x, int y)
{
    return P_GetBlockRoot((x - bmaporgx) >> MAPBLOCKSHIFT,
                          (y - bmaporgy) >> MAPBLOCKSHIFT);
}

/*
 * Only call if it is certain the thing is linked to a sector!
 * Two links to update:
 * 1) The link to us from the previous node (sprev, always set) will
 *    be modified to point to the node following us.
 * 2) If there is a node following us, set its sprev pointer to point
 *    to the pointer that points back to it (our sprev, just modified).
 */
void P_UnlinkFromSector(mobj_t *thing)
{
    if((*thing->sprev = thing->snext))
        thing->snext->sprev = thing->sprev;

    // Not linked any more.
    thing->snext = NULL;
    thing->sprev = NULL;
}

/*
 * Only call if it is certain that the thing is linked to a block!
 */
void P_UnlinkFromBlock(mobj_t *thing)
{
    (thing->bnext->bprev = thing->bprev)->bnext = thing->bnext;
    // Not linked any more.
    thing->bnext = thing->bprev = NULL;
}

/*
 * Unlinks the thing from all the lines it's been linked to. Can be called
 * without checking that the list does indeed contain lines.
 */
void P_UnlinkFromLines(mobj_t *thing)
{
    linknode_t *tn = thingnodes.nodes;
    nodeindex_t nix;

    // Try unlinking from lines.
    if(!thing->lineroot)
        return;                 // A zero index means it's not linked.

    // Unlink from each line.
    for(nix = tn[thing->lineroot].next; nix != thing->lineroot;
        nix = tn[nix].next)
    {
        // Data is the linenode index that corresponds this thing.
        NP_Unlink(&linenodes, tn[nix].data);
        // We don't need these nodes any more, mark them as unused.
        // Dismissing is a macro.
        NP_Dismiss(linenodes, tn[nix].data);
        NP_Dismiss(thingnodes, nix);
    }

    // The thing no longer has a line ring.
    NP_Dismiss(thingnodes, thing->lineroot);
    thing->lineroot = 0;
}

/*
 * Unlinks a thing from everything it has been linked to.
 */
void P_UnlinkThing(mobj_t *thing)
{
    if(IS_SECTOR_LINKED(thing))
        P_UnlinkFromSector(thing);
    if(IS_BLOCK_LINKED(thing))
        P_UnlinkFromBlock(thing);
    P_UnlinkFromLines(thing);
}

/*
 * The given line might cross the thing. If necessary, link the mobj
 * into the line's ring.
 */
boolean PIT_LinkToLines(line_t *ld, void *parm)
{
    int         p;
    linelinker_data_t *data = parm;
    fixed_t     bbox[4];
    nodeindex_t nix;

    // Setup the bounding box for the line.
    p = (ld->L_v1->pos[VX] < ld->L_v2->pos[VX]);
    bbox[BOXLEFT]   = FLT2FIX(ld->L_v(p^1)->pos[VX]);
    bbox[BOXRIGHT]  = FLT2FIX(ld->L_v(p)->pos[VX]);

    p = (ld->L_v1->pos[VY] < ld->L_v2->pos[VY]);
    bbox[BOXBOTTOM] = FLT2FIX(ld->L_v(p^1)->pos[VY]);
    bbox[BOXTOP]    = FLT2FIX(ld->L_v(p)->pos[VY]);

    if(data->box[BOXRIGHT]  <= bbox[BOXLEFT] ||
       data->box[BOXLEFT]   >= bbox[BOXRIGHT] ||
       data->box[BOXTOP]    <= bbox[BOXBOTTOM] ||
       data->box[BOXBOTTOM] >= bbox[BOXTOP])
        // Bounding boxes do not overlap.
        return true;

    if(P_BoxOnLineSide(data->box, ld) != -1)
        // Line does not cross the thing's bounding box.
        return true;

    // One sided lines will not be linked to because a mobj
    // can't legally cross one.
    if(!(ld->L_frontside || ld->L_backside))
        return true;

    // No redundant nodes will be creates since this routine is
    // called only once for each line.

    // Add a node to the thing's ring.
    NP_Link(&thingnodes, nix = NP_New(&thingnodes, ld), data->thing->lineroot);

    // Add a node to the line's ring. Also store the linenode's index
    // into the thingring's node, so unlinking is easy.
    NP_Link(&linenodes, thingnodes.nodes[nix].data =
            NP_New(&linenodes, data->thing), linelinks[GET_LINE_IDX(ld)]);

    return true;
}

/*
 * The thing must be currently unlinked.
 */
void P_LinkToLines(mobj_t *thing)
{
    int     xl, yl, xh, yh, bx, by;
    linelinker_data_t data;

    // Get a new root node.
    thing->lineroot = NP_New(&thingnodes, NP_ROOT_NODE);

    // Set up a line iterator for doing the linking.
    data.thing = thing;
    data.box[BOXTOP]    = thing->pos[VY] + thing->radius;
    data.box[BOXBOTTOM] = thing->pos[VY] - thing->radius;
    data.box[BOXRIGHT]  = thing->pos[VX] + thing->radius;
    data.box[BOXLEFT]   = thing->pos[VX] - thing->radius;

    xl = (data.box[BOXLEFT]   - bmaporgx) >> MAPBLOCKSHIFT;
    xh = (data.box[BOXRIGHT]  - bmaporgx) >> MAPBLOCKSHIFT;
    yl = (data.box[BOXBOTTOM] - bmaporgy) >> MAPBLOCKSHIFT;
    yh = (data.box[BOXTOP]    - bmaporgy) >> MAPBLOCKSHIFT;

    validcount++;
    for(bx = xl; bx <= xh; ++bx)
        for(by = yl; by <= yh; ++by)
            P_BlockLinesIterator(bx, by, PIT_LinkToLines, &data);
}

/*
 * Links a thing into both a block and a subsector based on it's (x,y).
 * Sets thing->subsector properly. Calling with flags==0 only updates
 * the subsector pointer. Can be called without unlinking first.
 */
void P_LinkThing(mobj_t *thing, byte flags)
{
    sector_t *sec;
    mobj_t *root;

    // Link into the sector.
    sec = (thing->subsector = R_PointInSubsector(thing->pos[VX], thing->pos[VY]))->sector;
    if(flags & DDLINK_SECTOR)
    {
        // Unlink from the current sector, if any.
        if(thing->sprev)
            P_UnlinkFromSector(thing);

        // Link the new thing to the head of the list.
        // Prev pointers point to the pointer that points back to us.
        // (Which practically disallows traversing the list backwards.)

        if((thing->snext = sec->thinglist))
            thing->snext->sprev = &thing->snext;

        *(thing->sprev = &sec->thinglist) = thing;
    }

    // Link into blockmap.
    if(flags & DDLINK_BLOCKMAP)
    {
        // Unlink from the old block, if any.
        if(thing->bnext)
            P_UnlinkFromBlock(thing);

        // Link into the block we're currently in.
        root = P_GetBlockRootXY(thing->pos[VX], thing->pos[VY]);
        if(root)
        {
            // Only link if we're inside the blockmap.
            thing->bprev = root;
            thing->bnext = root->bnext;
            thing->bnext->bprev = root->bnext = thing;
        }
    }

    // Link into lines.
    if(!(flags & DDLINK_NOLINE))
    {
        // Unlink from any existing lines.
        P_UnlinkFromLines(thing);

        // Link to all contacted lines.
        P_LinkToLines(thing);
    }

    // If this is a player - perform addtional tests to see if they have
    // entered or exited the void.
    if(thing->dplayer)
    {
        ddplayer_t* player = thing->dplayer;

        player->invoid = true;
        if(R_IsPointInSector2(player->mo->pos[VX], player->mo->pos[VY],
                              player->mo->subsector->sector))
            player->invoid = false;
    }
}

/*
 * 'func' can do whatever it pleases to the mobjs.
 */
boolean P_BlockThingsIterator(int x, int y, boolean (*func) (mobj_t *, void *),
                              void *data)
{
    mobj_t *mobj, *root = P_GetBlockRoot(x, y);
    void   *linkstore[MAXLINKED], **end = linkstore, **it;

    if(!root)
        return true;            // Not inside the blockmap.
    // Gather all the things in the block into the list.
    for(mobj = root->bnext; mobj != root; mobj = mobj->bnext)
        *end++ = mobj;
    DO_LINKS(it, end);
    return true;
}

/*
 * The callback function will be called once for each line that crosses
 * trough the object. This means all the lines will be two-sided.
 */
boolean P_ThingLinesIterator(mobj_t *thing, boolean (*func) (line_t *, void *),
                             void *data)
{
    nodeindex_t nix;
    linknode_t *tn = thingnodes.nodes;
    void   *linkstore[MAXLINKED], **end = linkstore, **it;

    if(!thing->lineroot)
        return true;            // No lines to process.
    for(nix = tn[thing->lineroot].next; nix != thing->lineroot;
        nix = tn[nix].next)
        *end++ = tn[nix].ptr;

    DO_LINKS(it, end);
    return true;
}

/*
 * Increment validcount before calling this routine. The callback function
 * will be called once for each sector the thing is touching
 * (totally or partly inside). This is not a 3D check; the thing may
 * actually reside above or under the sector.
 */
boolean P_ThingSectorsIterator(mobj_t *thing,
                               boolean (*func) (sector_t *, void *),
                               void *data)
{
    void   *linkstore[MAXLINKED], **end = linkstore, **it;
    nodeindex_t nix;
    linknode_t *tn = thingnodes.nodes;
    line_t *ld;
    sector_t *sec;

    // Always process the thing's own sector first.
    *end++ = sec = thing->subsector->sector;
    sec->validcount = validcount;

    // Any good lines around here?
    if(thing->lineroot)
    {
        for(nix = tn[thing->lineroot].next; nix != thing->lineroot;
            nix = tn[nix].next)
        {
            ld = (line_t *) tn[nix].ptr;
            // All these lines are two-sided. Try front side.
            sec = ld->L_frontsector;
            if(sec->validcount != validcount)
            {
                *end++ = sec;
                sec->validcount = validcount;
            }
            // And then the back side.
            if(ld->L_backside)
            {
                sec = ld->L_backsector;
                if(sec->validcount != validcount)
                {
                    *end++ = sec;
                    sec->validcount = validcount;
                }
            }
        }
    }
    DO_LINKS(it, end);
    return true;
}

boolean P_LineThingsIterator(line_t *line, boolean (*func) (mobj_t *, void *),
                             void *data)
{
    void   *linkstore[MAXLINKED], **end = linkstore, **it;
    nodeindex_t root = linelinks[GET_LINE_IDX(line)], nix;
    linknode_t *ln = linenodes.nodes;

    for(nix = ln[root].next; nix != root; nix = ln[nix].next)
        *end++ = ln[nix].ptr;
    DO_LINKS(it, end);
    return true;
}

/**
 * Increment validcount before using this. 'func' is called for each mobj
 * that is (even partly) inside the sector. This is not a 3D test, the
 * mobjs may actually be above or under the sector.
 *
 * (Lovely name; actually this is a combination of SectorThings and
 * a bunch of LineThings iterations.)
 */
boolean P_SectorTouchingThingsIterator(sector_t *sector,
                                       boolean (*func) (mobj_t *, void *),
                                       void *data)
{
    void   *linkstore[MAXLINKED], **end = linkstore, **it;
    mobj_t *mo;
    line_t *li;
    nodeindex_t root, nix;
    linknode_t *ln = linenodes.nodes;
    uint    i;

    // First process the things that obviously are in the sector.
    for(mo = sector->thinglist; mo; mo = mo->snext)
    {
        if(mo->validcount == validcount)
            continue;
        mo->validcount = validcount;
        *end++ = mo;
    }
    // Then check the sector's lines.
    for(i = 0; i < sector->linecount; ++i)
    {
        li = sector->Lines[i];
        // Iterate all mobjs on the line.
        root = linelinks[GET_LINE_IDX(li)];
        for(nix = ln[root].next; nix != root; nix = ln[nix].next)
        {
            mo = (mobj_t *) ln[nix].ptr;
            if(mo->validcount == validcount)
                continue;
            mo->validcount = validcount;
            *end++ = mo;
        }
    }
    DO_LINKS(it, end);
    return true;
}

/*
 * Looks for lines in the given block that intercept the given trace
 * to add to the intercepts list
 * A line is crossed if its endpoints are on opposite sides of the trace
 * Returns true if earlyout and a solid line hit
 */
boolean PIT_AddLineIntercepts(line_t *ld, void *data)
{
    int         s[2];
    fixed_t     frac;
    divline_t   dl;

    // avoid precision problems with two routines
    if(trace.dx > FRACUNIT * 16 || trace.dy > FRACUNIT * 16 ||
       trace.dx < -FRACUNIT * 16 || trace.dy < -FRACUNIT * 16)
    {
        s[0] = P_PointOnDivlineSide(FLT2FIX(ld->L_v1->pos[VX]),
                                    FLT2FIX(ld->L_v1->pos[VY]), &trace);
        s[1] = P_PointOnDivlineSide(FLT2FIX(ld->L_v2->pos[VX]),
                                    FLT2FIX(ld->L_v2->pos[VY]), &trace);
    }
    else
    {
        s[0] = P_PointOnLineSide(trace.pos[VX], trace.pos[VY], ld);
        s[1] = P_PointOnLineSide(trace.pos[VX] + trace.dx, trace.pos[VY] + trace.dy, ld);
    }
    if(s[0] == s[1])
        return true;            // line isn't crossed

    //
    // hit the line
    //
    P_MakeDivline(ld, &dl);
    frac = P_InterceptVector(&trace, &dl);
    if(frac < 0)
        return true;            // behind source

    // try to early out the check
    if(earlyout && frac < FRACUNIT && !ld->L_backside)
        return false;           // stop checking

    P_AddIntercept(frac, true, ld);

    return true;                // continue
}

boolean PIT_AddThingIntercepts(mobj_t *thing, void *data)
{
    fixed_t     x1, y1, x2, y2;
    int         s[2];
    boolean     tracepositive;
    divline_t   dl;
    fixed_t     frac;

    if(thing->dplayer && thing->dplayer->flags & DDPF_CAMERA)
        return true;            // $democam: ssshh, keep going, we're not here...

    tracepositive = (trace.dx ^ trace.dy) > 0;

    // check a corner to corner crossection for hit
    if(tracepositive)
    {
        x1 = thing->pos[VX] - thing->radius;
        y1 = thing->pos[VY] + thing->radius;

        x2 = thing->pos[VX] + thing->radius;
        y2 = thing->pos[VY] - thing->radius;
    }
    else
    {
        x1 = thing->pos[VX] - thing->radius;
        y1 = thing->pos[VY] - thing->radius;

        x2 = thing->pos[VX] + thing->radius;
        y2 = thing->pos[VY] + thing->radius;
    }

    s[0] = P_PointOnDivlineSide(x1, y1, &trace);
    s[1] = P_PointOnDivlineSide(x2, y2, &trace);
    if(s[0] == s[1])
        return true;            // line isn't crossed

    dl.pos[VX] = x1;
    dl.pos[VY] = y1;
    dl.dx = x2 - x1;
    dl.dy = y2 - y1;

    frac = P_InterceptVector(&trace, &dl);
    if(frac < 0)
        return true;            // behind source

    P_AddIntercept(frac, false, thing);

    return true;                // keep going
}

/*
 * Traces a line from x1,y1 to x2,y2, calling the traverser function for each
 * Returns true if the traverser function returns true for all lines
 */
boolean P_PathTraverse(fixed_t x1, fixed_t y1, fixed_t x2, fixed_t y2,
                       int flags, boolean (*trav) (intercept_t *))
{
    fixed_t     t1[2], t2[2];
    fixed_t     xstep, ystep;
    fixed_t     partial;
    fixed_t     xintercept, yintercept;
    int         mapx, mapy, mapxstep, mapystep;
    uint        count;

    earlyout = flags & PT_EARLYOUT;

    validcount++;
    //intercept_p = intercepts;
    P_ClearIntercepts();

    if(((x1 - bmaporgx) & (MAPBLOCKSIZE - 1)) == 0)
        x1 += FRACUNIT;         // don't side exactly on a line
    if(((y1 - bmaporgy) & (MAPBLOCKSIZE - 1)) == 0)
        y1 += FRACUNIT;         // don't side exactly on a line
    trace.pos[VX] = x1;
    trace.pos[VY] = y1;
    trace.dx = x2 - x1;
    trace.dy = y2 - y1;

    x1 -= bmaporgx;
    y1 -= bmaporgy;
    t1[VX] = x1 >> MAPBLOCKSHIFT;
    t1[VY] = y1 >> MAPBLOCKSHIFT;

    x2 -= bmaporgx;
    y2 -= bmaporgy;
    t2[VX] = x2 >> MAPBLOCKSHIFT;
    t2[VY] = y2 >> MAPBLOCKSHIFT;

    if(t2[VX] > t1[VX])
    {
        mapxstep = 1;
        partial = FRACUNIT - ((x1 >> MAPBTOFRAC) & (FRACUNIT - 1));
        ystep = FixedDiv(y2 - y1, abs(x2 - x1));
    }
    else if(t2[VX] < t1[VX])
    {
        mapxstep = -1;
        partial = (x1 >> MAPBTOFRAC) & (FRACUNIT - 1);
        ystep = FixedDiv(y2 - y1, abs(x2 - x1));
    }
    else
    {
        mapxstep = 0;
        partial = FRACUNIT;
        ystep = 256 * FRACUNIT;
    }
    yintercept = (y1 >> MAPBTOFRAC) + FixedMul(partial, ystep);

    if(t2[VY] > t1[VY])
    {
        mapystep = 1;
        partial = FRACUNIT - ((y1 >> MAPBTOFRAC) & (FRACUNIT - 1));
        xstep = FixedDiv(x2 - x1, abs(y2 - y1));
    }
    else if(t2[VY] < t1[VY])
    {
        mapystep = -1;
        partial = (y1 >> MAPBTOFRAC) & (FRACUNIT - 1);
        xstep = FixedDiv(x2 - x1, abs(y2 - y1));
    }
    else
    {
        mapystep = 0;
        partial = FRACUNIT;
        xstep = 256 * FRACUNIT;
    }
    xintercept = (x1 >> MAPBTOFRAC) + FixedMul(partial, xstep);

    //
    // step through map blocks
    // Count is present to prevent a round off error from skipping the break
    mapx = t1[VX];
    mapy = t1[VY];

    for(count = 0; count < 64; ++count)
    {
        if(flags & PT_ADDLINES)
        {
            if(!P_BlockLinesIterator(mapx, mapy, PIT_AddLineIntercepts, 0))
                return false;   // early out
        }
        if(flags & PT_ADDTHINGS)
        {
            if(!P_BlockThingsIterator(mapx, mapy, PIT_AddThingIntercepts, 0))
                return false;   // early out
        }

        if(mapx == t2[VX] && mapy == t2[VY])
            break;

        if((yintercept >> FRACBITS) == mapy)
        {
            yintercept += ystep;
            mapx += mapxstep;
        }
        else if((xintercept >> FRACBITS) == mapx)
        {
            xintercept += xstep;
            mapy += mapystep;
        }

    }

    //
    // go through the sorted list
    //
    return P_TraverseIntercepts(trav, FRACUNIT);
}

void P_PointToBlock(fixed_t x, fixed_t y, int *bx, int *by)
{
    *bx = (x - bmaporgx) >> MAPBLOCKSHIFT;
    *by = (y - bmaporgy) >> MAPBLOCKSHIFT;
}
