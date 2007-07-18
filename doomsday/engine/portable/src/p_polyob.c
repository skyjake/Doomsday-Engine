/**\file
 *\section License
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
 * p_polyob.c: Polygon Objects
 *
 * Polyobj translation and rotation.
 * Based on Hexen by Raven Software.
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_console.h"
#include "de_play.h"
#include "de_refresh.h"

// MACROS ------------------------------------------------------------------

#define MAXRADIUS   32*FRACUNIT

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void UpdateSegBBox(seg_t *seg);
static void RotatePt(int an, float *x, float *y, fixed_t startSpotX,
                     fixed_t startSpotY);
static boolean CheckMobjBlocking(seg_t *seg, polyobj_t *po);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// Called when the polyobj hits a mobj.
void    (*po_callback) (mobj_t *mobj, void *seg, void *po);

polyobj_t *polyobjs;               // list of all poly-objects on the level
uint    po_NumPolyobjs;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

/**
 * Allocate memory for polyobjs.
 */
void PO_Allocate(void)
{
    uint        i;

    polyobjs = Z_Malloc(po_NumPolyobjs * sizeof(polyobj_t), PU_LEVEL, 0);
    memset(polyobjs, 0, po_NumPolyobjs * sizeof(polyobj_t));

    // Initialize the runtime type identifiers.
    for(i = 0; i < po_NumPolyobjs; ++i)
    {
        polyobjs[i].header.type = DMU_POLYOBJ;
    }
}

/**
 * The po_callback is called when a polyobj hits a mobj.
 */
void PO_SetCallback(void (*func) (mobj_t *, void *, void *))
{
    po_callback = func;
}

polyobj_t *GetPolyobj(uint polyNum)
{
    uint        i;

    for(i = 0; i < po_NumPolyobjs; ++i)
    {
        if((uint) PO_PTR(i)->tag == polyNum)
        {
            return PO_PTR(i);   //&polyobjs[i];
        }
    }
    return NULL;
}

void UpdateSegBBox(seg_t *seg)
{
    line_t     *line = seg->linedef;
    byte        edge;

    edge = (seg->SG_v1pos[VX] < seg->SG_v2pos[VX]);
    line->bbox[BOXLEFT]  = FLT2FIX(seg->SG_vpos(edge^1)[VX]);
    line->bbox[BOXRIGHT] = FLT2FIX(seg->SG_vpos(edge)[VX]);

    edge = (seg->SG_v1pos[VY] < seg->SG_v2pos[VY]);
    line->bbox[BOXBOTTOM] = FLT2FIX(seg->SG_vpos(edge^1)[VY]);
    line->bbox[BOXTOP]    = FLT2FIX(seg->SG_vpos(edge)[VY]);

    // Update the line's slopetype
    line->dx = line->L_v2pos[VX] - line->L_v1pos[VX];
    line->dy = line->L_v2pos[VY] - line->L_v1pos[VY];
    if(!line->dx)
    {
        line->slopetype = ST_VERTICAL;
    }
    else if(!line->dy)
    {
        line->slopetype = ST_HORIZONTAL;
    }
    else
    {
        if(line->dy / line->dx > 0)
        {
            line->slopetype = ST_POSITIVE;
        }
        else
        {
            line->slopetype = ST_NEGATIVE;
        }
    }
}

/**
 * Called at the start of the level after all the structures needed for
 * refresh have been setup.
 */
void PO_SetupPolyobjs(void)
{
    uint        i, j, k, num;
    seg_t     **segList, *seg;
    line_t     *line;
    side_t     *side;

    for(i = 0; i < po_NumPolyobjs; ++i)
    {
        segList = polyobjs[i].segs;
        num = polyobjs[i].numsegs;
        for(j = 0; j < num; ++j, segList++)
        {
            seg = (*segList);

            // Mark this as a poly object seg.
            seg->flags |= SEGF_POLYOBJ;

            if(seg->linedef)
            {
                line = seg->linedef;

                for(k = 0; k < 2; ++k)
                {
                    if(line->L_side(k))
                    {
                        side = line->L_side(k);

                        side->SW_topflags |= SUF_NO_RADIO;
                        side->SW_middleflags |= SUF_NO_RADIO;
                        side->SW_bottomflags |= SUF_NO_RADIO;
                    }
                }
            }
        }
    }
}

boolean PO_MovePolyobj(uint num, int x, int y)
{
    uint        count;
    seg_t     **segList;
    seg_t     **veryTempSeg;
    polyobj_t  *po;
    ddvertex_t *prevPts;
    boolean     blocked;
    float       fx = FIX2FLT(x), fy = FIX2FLT(y);

    if(num & 0x80000000)
    {
        po = PO_PTR(num & 0x7fffffff);
    }
    else if(!(po = GetPolyobj(num)))
    {
        Con_Error("PO_MovePolyobj:  Invalid polyobj number: %d\n", num);
    }

    PO_UnLinkPolyobj(po);

    segList = po->segs;
    prevPts = po->prevPts;
    blocked = false;

    validcount++;
    for(count = po->numsegs; count; count--, segList++, prevPts++)
    {
        if((*segList)->linedef->validcount != validcount)
        {
            (*segList)->linedef->bbox[BOXTOP]    += y;
            (*segList)->linedef->bbox[BOXBOTTOM] += y;
            (*segList)->linedef->bbox[BOXLEFT]   += x;
            (*segList)->linedef->bbox[BOXRIGHT]  += x;
            (*segList)->linedef->validcount = validcount;
        }
        for(veryTempSeg = po->segs; veryTempSeg != segList; veryTempSeg++)
        {
            if((*veryTempSeg)->SG_v1 == (*segList)->SG_v1)
            {
                break;
            }
        }
        if(veryTempSeg == segList)
        {
            (*segList)->SG_v1pos[VX] += fx;
            (*segList)->SG_v1pos[VY] += fy;
        }
        (*prevPts).pos[VX] += x;      // previous points are unique for each seg
        (*prevPts).pos[VY] += y;
    }
    segList = po->segs;
    for(count = po->numsegs; count; count--, segList++)
    {
        if(CheckMobjBlocking(*segList, po))
        {
            blocked = true;
        }
    }
    if(blocked)
    {
        count = po->numsegs;
        segList = po->segs;
        prevPts = po->prevPts;
        validcount++;
        while(count--)
        {
            if((*segList)->linedef->validcount != validcount)
            {
                (*segList)->linedef->bbox[BOXTOP]    -= y;
                (*segList)->linedef->bbox[BOXBOTTOM] -= y;
                (*segList)->linedef->bbox[BOXLEFT]   -= x;
                (*segList)->linedef->bbox[BOXRIGHT]  -= x;
                (*segList)->linedef->validcount = validcount;
            }
            for(veryTempSeg = po->segs; veryTempSeg != segList; veryTempSeg++)
            {
                if((*veryTempSeg)->SG_v1 == (*segList)->SG_v1)
                {
                    break;
                }
            }
            if(veryTempSeg == segList)
            {
                (*segList)->SG_v1pos[VX] -= fx;
                (*segList)->SG_v1pos[VY] -= fy;
            }
            (*prevPts).pos[VX] -= x;
            (*prevPts).pos[VY] -= y;

            segList++;
            prevPts++;
        }
        PO_LinkPolyobj(po);
        return false;
    }
    po->startSpot.pos[VX] += x;
    po->startSpot.pos[VY] += y;
    PO_LinkPolyobj(po);

    // A change has occured.
    P_PolyobjChanged(po);

    return true;
}

static void RotatePt(int an, float *x, float *y, fixed_t startSpotX,
                     fixed_t startSpotY)
{
    fixed_t trx, try;
    fixed_t gxt, gyt;

    trx = FLT2FIX(*x);
    try = FLT2FIX(*y);

    gxt = FixedMul(trx, finecosine[an]);
    gyt = FixedMul(try, finesine[an]);
    *x = FIX2FLT((gxt - gyt) + startSpotX);

    gxt = FixedMul(trx, finesine[an]);
    gyt = FixedMul(try, finecosine[an]);
    *y = FIX2FLT((gyt + gxt) + startSpotY);
}

boolean PO_RotatePolyobj(uint num, angle_t angle)
{
    uint        count;
    seg_t     **segList;
    ddvertex_t *originalPts;
    ddvertex_t *prevPts;
    int         an;
    polyobj_t  *po;
    boolean     blocked;
    vertex_t   *vtx;

    if(num & 0x80000000)
    {
        po = PO_PTR(num & 0x7fffffff);
    }
    else if(!(po = GetPolyobj(num)))
    {
        Con_Error("PO_RotatePolyobj: Invalid polyobj number: %d\n", num);
    }
    an = (po->angle + angle) >> ANGLETOFINESHIFT;

    PO_UnLinkPolyobj(po);

    segList = po->segs;
    originalPts = po->originalPts;
    prevPts = po->prevPts;

    for(count = po->numsegs; count;
        count--, segList++, originalPts++, prevPts++)
    {
        vtx = (*segList)->SG_v1;

        prevPts->pos[VX] = FLT2FIX(vtx->V_pos[VX]);
        prevPts->pos[VY] = FLT2FIX(vtx->V_pos[VY]);
        vtx->V_pos[VX] = FIX2FLT(originalPts->pos[VX]);
        vtx->V_pos[VY] = FIX2FLT(originalPts->pos[VY]);

        RotatePt(an, &vtx->V_pos[VX], &vtx->V_pos[VY],
                 po->startSpot.pos[VX], po->startSpot.pos[VY]);
    }
    segList = po->segs;
    blocked = false;
    validcount++;
    for(count = po->numsegs; count; count--, segList++)
    {
        if(CheckMobjBlocking(*segList, po))
        {
            blocked = true;
        }
        if((*segList)->linedef->validcount != validcount)
        {
            UpdateSegBBox(*segList);
            (*segList)->linedef->validcount = validcount;
        }
        (*segList)->angle += angle;
    }
    if(blocked)
    {
        segList = po->segs;
        prevPts = po->prevPts;
        for(count = po->numsegs; count; count--, segList++, prevPts++)
        {
            vtx = (*segList)->SG_v1;

            vtx->V_pos[VX] = FIX2FLT(prevPts->pos[VX]);
            vtx->V_pos[VY] = FIX2FLT(prevPts->pos[VY]);
        }
        segList = po->segs;
        validcount++;
        for(count = po->numsegs; count; count--, segList++, prevPts++)
        {
            if((*segList)->linedef->validcount != validcount)
            {
                UpdateSegBBox(*segList);
                (*segList)->linedef->validcount = validcount;
            }
            (*segList)->angle -= angle;
        }
        PO_LinkPolyobj(po);
        return false;
    }
    po->angle += angle;
    PO_LinkPolyobj(po);
    P_PolyobjChanged(po);
    return true;
}

void PO_UnLinkPolyobj(polyobj_t * po)
{
    polyblock_t *link;
    int     i, j;
    int     index;

    // remove the polyobj from each blockmap section
    for(j = po->bbox[BOXBOTTOM]; j <= po->bbox[BOXTOP]; ++j)
    {
        index = j * bmapwidth;
        for(i = po->bbox[BOXLEFT]; i <= po->bbox[BOXRIGHT]; ++i)
        {
            if(i >= 0 && i < bmapwidth && j >= 0 && j < bmapheight)
            {
                link = polyblockmap[index + i];
                while(link != NULL && link->polyobj != po)
                {
                    link = link->next;
                }
                if(link == NULL)
                {               // polyobj not located in the link cell
                    continue;
                }
                link->polyobj = NULL;
            }
        }
    }
}

void PO_LinkPolyobj(polyobj_t * po)
{
    int         leftX, rightX;
    int         topY, bottomY;
    seg_t     **tempSeg;
    polyblock_t **link;
    polyblock_t *tempLink;
    uint        s;
    int         i, j;
    vertex_t   *vtx;

    // calculate the polyobj bbox
    tempSeg = po->segs;
    rightX = leftX = FLT2FIX((*tempSeg)->SG_v1pos[VX]);
    topY = bottomY = FLT2FIX((*tempSeg)->SG_v1pos[VY]);

    for(s = 0; s < po->numsegs; ++s, tempSeg++)
    {
        vtx = (*tempSeg)->SG_v1;

        if(vtx->V_pos[VX] > rightX)
        {
            rightX = FLT2FIX(vtx->V_pos[VX]);
        }
        if(vtx->V_pos[VX] < leftX)
        {
            leftX = FLT2FIX(vtx->V_pos[VX]);
        }
        if(vtx->V_pos[VY] > topY)
        {
            topY = FLT2FIX(vtx->V_pos[VY]);
        }
        if(vtx->V_pos[VY] < bottomY)
        {
            bottomY = FLT2FIX(vtx->V_pos[VY]);
        }
    }
    po->bbox[BOXRIGHT]  = (rightX  - bmaporgx) >> MAPBLOCKSHIFT;
    po->bbox[BOXLEFT]   = (leftX   - bmaporgx) >> MAPBLOCKSHIFT;
    po->bbox[BOXTOP]    = (topY    - bmaporgy) >> MAPBLOCKSHIFT;
    po->bbox[BOXBOTTOM] = (bottomY - bmaporgy) >> MAPBLOCKSHIFT;
    // add the polyobj to each blockmap section
    for(j = po->bbox[BOXBOTTOM] * bmapwidth; j <= po->bbox[BOXTOP] * bmapwidth;
        j += bmapwidth)
    {
        for(i = po->bbox[BOXLEFT]; i <= po->bbox[BOXRIGHT]; ++i)
        {
            if(i >= 0 && i < bmapwidth && j >= 0 && j < bmapheight * bmapwidth)
            {
                link = &polyblockmap[j + i];
                if(!(*link))
                {               // Create a new link at the current block cell
                    *link = Z_Malloc(sizeof(polyblock_t), PU_LEVEL, 0);
                    (*link)->next = NULL;
                    (*link)->prev = NULL;
                    (*link)->polyobj = po;
                    continue;
                }
                else
                {
                    tempLink = *link;
                    while(tempLink->next != NULL && tempLink->polyobj != NULL)
                    {
                        tempLink = tempLink->next;
                    }
                }
                if(tempLink->polyobj == NULL)
                {
                    tempLink->polyobj = po;
                    continue;
                }
                else
                {
                    tempLink->next =
                        Z_Malloc(sizeof(polyblock_t), PU_LEVEL, 0);
                    tempLink->next->next = NULL;
                    tempLink->next->prev = tempLink;
                    tempLink->next->polyobj = po;
                }
            }
            // else, don't link the polyobj, since it's off the map
        }
    }
}

static boolean CheckMobjBlocking(seg_t *seg, polyobj_t * po)
{
    mobj_t     *mobj, *root;
    int         i, j;
    int         left, right, top, bottom;
    int         tmbbox[4];
    line_t     *ld;
    boolean     blocked;

    ld = seg->linedef;

    top    = (ld->bbox[BOXTOP]    - bmaporgy + MAXRADIUS) >> MAPBLOCKSHIFT;
    bottom = (ld->bbox[BOXBOTTOM] - bmaporgy - MAXRADIUS) >> MAPBLOCKSHIFT;
    left   = (ld->bbox[BOXLEFT]   - bmaporgx - MAXRADIUS) >> MAPBLOCKSHIFT;
    right  = (ld->bbox[BOXRIGHT]  - bmaporgx + MAXRADIUS) >> MAPBLOCKSHIFT;

    blocked = false;

    bottom = bottom < 0 ? 0 : bottom;
    bottom = bottom >= bmapheight ? bmapheight - 1 : bottom;
    top    = top    < 0 ? 0 : top;
    top    = top    >= bmapheight ? bmapheight - 1 : top;
    left   = left   < 0 ? 0 : left;
    left   = left   >= bmapwidth ?  bmapwidth  - 1 : left;
    right  = right  < 0 ? 0 : right;
    right  = right  >= bmapwidth ?  bmapwidth  - 1 : right;

    for(j = bottom * bmapwidth; j <= top * bmapwidth; j += bmapwidth)
    {
        for(i = left; i <= right; ++i)
        {
            root = (mobj_t *) &blockrings[j + i];
            for(mobj = root->bnext; mobj != root; mobj = mobj->bnext)
            {
                if((mobj->ddflags & DDMF_SOLID) ||
                   (mobj->dplayer && !(mobj->dplayer->flags & DDPF_CAMERA)))
                {
                    tmbbox[BOXTOP]    = mobj->pos[VY] + mobj->radius;
                    tmbbox[BOXBOTTOM] = mobj->pos[VY] - mobj->radius;
                    tmbbox[BOXLEFT]   = mobj->pos[VX] - mobj->radius;
                    tmbbox[BOXRIGHT]  = mobj->pos[VX] + mobj->radius;

                    if(tmbbox[BOXRIGHT]  <= ld->bbox[BOXLEFT] ||
                       tmbbox[BOXLEFT]   >= ld->bbox[BOXRIGHT] ||
                       tmbbox[BOXTOP]    <= ld->bbox[BOXBOTTOM] ||
                       tmbbox[BOXBOTTOM] >= ld->bbox[BOXTOP])
                    {
                        continue;
                    }
                    if(P_BoxOnLineSide(tmbbox, ld) != -1)
                    {
                        continue;
                    }
                    if(po_callback)
                        po_callback(mobj, seg, po);
                    blocked = true;
                }
            }
        }
    }
    return blocked;
}

/**
 * @returns             Ptr to the polyobj that owns the degenmobj,
 *                      else <code>NULL</code>.
 */
polyobj_t* PO_GetForDegen(void *degenMobj)
{
    uint        i;
    polyobj_t  *po;

    for(i = 0; i < po_NumPolyobjs; ++i)
    {
        po = PO_PTR(i);

        if(&po->startSpot == degenMobj)
        {
            return po;
        }
    }

    return NULL;
}
