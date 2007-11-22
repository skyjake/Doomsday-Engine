/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2007 Jaakko Keränen <jaakko.keranen@iki.fi>
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

/**
 * p_polyob.c: Polygon Objects
 *
 * Polyobj translation and rotation.
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_console.h"
#include "de_play.h"
#include "de_refresh.h"

// MACROS ------------------------------------------------------------------

#define MAXRADIUS   32

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void UpdateSegBBox(seg_t *seg);
static void RotatePt(int an, float *x, float *y, float startSpotX,
                     float startSpotY);
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
    line->bbox[BOXLEFT]  = seg->SG_vpos(edge^1)[VX];
    line->bbox[BOXRIGHT] = seg->SG_vpos(edge)[VX];

    edge = (seg->SG_v1pos[VY] < seg->SG_v2pos[VY]);
    line->bbox[BOXBOTTOM] = seg->SG_vpos(edge^1)[VY];
    line->bbox[BOXTOP]    = seg->SG_vpos(edge)[VY];

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
 * Update the polyobj bounding box.
 */
void PO_UpdateBBox(polyobj_t *po)
{
    seg_t     **segPtr;
    uint        i;
    vec2_t      point;
    vertex_t   *vtx;

    segPtr = po->segs;
    V2_Set(point, (*segPtr)->SG_v1pos[VX], (*segPtr)->SG_v1pos[VY]);
    V2_InitBox(po->box, point);

    for(i = 0; i < po->numsegs; ++i, segPtr++)
    {
        vtx = (*segPtr)->SG_v1;

        V2_Set(point, vtx->V_pos[VX], vtx->V_pos[VY]);
        V2_AddToBox(po->box, point);
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
    polyobj_t  *po;

    for(i = 0; i < po_NumPolyobjs; ++i)
    {
        po = &polyobjs[i];
        segList = po->segs;
        num = po->numsegs;

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

        PO_UnLinkPolyobj(po);
        PO_LinkPolyobj(po);
    }
}

boolean PO_MovePolyobj(uint num, float x, float y)
{
    uint        count;
    seg_t     **segList;
    seg_t     **veryTempSeg;
    polyobj_t  *po;
    fvertex_t  *prevPts;
    boolean     blocked;

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

    validCount++;
    for(count = 0; count < po->numsegs; ++count, segList++, prevPts++)
    {
        if((*segList)->linedef->validCount != validCount)
        {
            (*segList)->linedef->bbox[BOXTOP]    += y;
            (*segList)->linedef->bbox[BOXBOTTOM] += y;
            (*segList)->linedef->bbox[BOXLEFT]   += x;
            (*segList)->linedef->bbox[BOXRIGHT]  += x;
            (*segList)->linedef->validCount = validCount;
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
            (*segList)->SG_v1pos[VX] += x;
            (*segList)->SG_v1pos[VY] += y;
        }
        (*prevPts).pos[VX] += x;      // previous points are unique for each seg
        (*prevPts).pos[VY] += y;
    }
    segList = po->segs;
    for(count = 0; count < po->numsegs; ++count, segList++)
    {
        if(CheckMobjBlocking(*segList, po))
        {
            blocked = true;
        }
    }
    if(blocked)
    {
        count = 0;
        segList = po->segs;
        prevPts = po->prevPts;
        validCount++;
        while(count++ < po->numsegs)
        {
            if((*segList)->linedef->validCount != validCount)
            {
                (*segList)->linedef->bbox[BOXTOP]    -= y;
                (*segList)->linedef->bbox[BOXBOTTOM] -= y;
                (*segList)->linedef->bbox[BOXLEFT]   -= x;
                (*segList)->linedef->bbox[BOXRIGHT]  -= x;
                (*segList)->linedef->validCount = validCount;
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
                (*segList)->SG_v1pos[VX] -= x;
                (*segList)->SG_v1pos[VY] -= y;
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

static void RotatePt(int an, float *x, float *y, float startSpotX, float startSpotY)
{
    float trx, try;
    float gxt, gyt;

    trx = *x;
    try = *y;

    gxt = trx * FIX2FLT(fineCosine[an]);
    gyt = try * FIX2FLT(finesine[an]);
    *x = gxt - gyt + startSpotX;

    gxt = trx * FIX2FLT(finesine[an]);
    gyt = try * FIX2FLT(fineCosine[an]);
    *y = gyt + gxt + startSpotY;
}

boolean PO_RotatePolyobj(uint num, angle_t angle)
{
    uint        count;
    seg_t     **segList;
    fvertex_t  *originalPts;
    fvertex_t  *prevPts;
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

    for(count = 0; count < po->numsegs;
        ++count, segList++, originalPts++, prevPts++)
    {
        vtx = (*segList)->SG_v1;

        prevPts->pos[VX] = vtx->V_pos[VX];
        prevPts->pos[VY] = vtx->V_pos[VY];
        vtx->V_pos[VX] = originalPts->pos[VX];
        vtx->V_pos[VY] = originalPts->pos[VY];

        RotatePt(an, &vtx->V_pos[VX], &vtx->V_pos[VY],
                 po->startSpot.pos[VX], po->startSpot.pos[VY]);
    }
    segList = po->segs;
    blocked = false;
    validCount++;
    for(count = 0; count < po->numsegs; ++count, segList++)
    {
        if(CheckMobjBlocking(*segList, po))
        {
            blocked = true;
        }
        if((*segList)->linedef->validCount != validCount)
        {
            UpdateSegBBox(*segList);
            (*segList)->linedef->validCount = validCount;
        }
        (*segList)->angle += angle;
    }
    if(blocked)
    {
        segList = po->segs;
        prevPts = po->prevPts;
        for(count = 0; count < po->numsegs; ++count, segList++, prevPts++)
        {
            vtx = (*segList)->SG_v1;

            vtx->V_pos[VX] = prevPts->pos[VX];
            vtx->V_pos[VY] = prevPts->pos[VY];
        }
        segList = po->segs;
        validCount++;
        for(count = 0; count < po->numsegs; ++count, segList++, prevPts++)
        {
            if((*segList)->linedef->validCount != validCount)
            {
                UpdateSegBBox(*segList);
                (*segList)->linedef->validCount = validCount;
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

void PO_LinkPolyobjToRing(polyobj_t *po, linkpolyobj_t **link)
{
    linkpolyobj_t *tempLink;

    if(!(*link))
    {   // Create a new link at the current block cell.
        *link = Z_Malloc(sizeof(linkpolyobj_t), PU_LEVEL, 0);
        (*link)->next = NULL;
        (*link)->prev = NULL;
        (*link)->polyobj = po;
        return;
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
        return;
    }
    else
    {
        tempLink->next =
            Z_Malloc(sizeof(linkpolyobj_t), PU_LEVEL, 0);
        tempLink->next->next = NULL;
        tempLink->next->prev = tempLink;
        tempLink->next->polyobj = po;
    }
}

void PO_UnlinkPolyobjFromRing(polyobj_t *po, linkpolyobj_t **link)
{
    while((*link) != NULL && (*link)->polyobj != po)
    {
        (*link) = (*link)->next;
    }

    if((*link) != NULL)
    {
        (*link)->polyobj = NULL;
    }
}

void PO_UnLinkPolyobj(polyobj_t *po)
{
    P_BlockmapUnlinkPolyobj(BlockMap, po);
}

void PO_LinkPolyobj(polyobj_t *po)
{
    P_BlockmapLinkPolyobj(BlockMap, po);
}

typedef struct ptrmobjblockingparams_s {
    boolean         blocked;
    line_t         *line;
    seg_t          *seg;
    polyobj_t      *po;
} ptrmobjblockingparams_t;

boolean PTR_CheckMobjBlocking(mobj_t *mo, void *data)
{
    if((mo->ddflags & DDMF_SOLID) ||
       (mo->dplayer && !(mo->dplayer->flags & DDPF_CAMERA)))
    {
        float       tmbbox[4];
        ptrmobjblockingparams_t *params = data;

        tmbbox[BOXTOP]    = mo->pos[VY] + mo->radius;
        tmbbox[BOXBOTTOM] = mo->pos[VY] - mo->radius;
        tmbbox[BOXLEFT]   = mo->pos[VX] - mo->radius;
        tmbbox[BOXRIGHT]  = mo->pos[VX] + mo->radius;

        if(!(tmbbox[BOXRIGHT]  <= params->line->bbox[BOXLEFT] ||
             tmbbox[BOXLEFT]   >= params->line->bbox[BOXRIGHT] ||
             tmbbox[BOXTOP]    <= params->line->bbox[BOXBOTTOM] ||
             tmbbox[BOXBOTTOM] >= params->line->bbox[BOXTOP]))
        {
            if(P_BoxOnLineSide(tmbbox, params->line) == -1)
            {
                if(po_callback)
                    po_callback(mo, params->seg, params->po);

                params->blocked = true;
            }
        }
    }

    return true; // Continue iteration.
}

static boolean CheckMobjBlocking(seg_t *seg, polyobj_t *po)
{
    uint        blockBox[4];
    vec2_t      bbox[2];
    line_t     *ld;
    ptrmobjblockingparams_t params;

    params.blocked = false;
    params.line = ld = seg->linedef;
    params.seg = seg;
    params.po = po;

    bbox[0][VX] = ld->bbox[BOXLEFT]   - MAXRADIUS;
    bbox[0][VY] = ld->bbox[BOXBOTTOM] - MAXRADIUS;
    bbox[1][VX] = ld->bbox[BOXRIGHT]  + MAXRADIUS;
    bbox[1][VY] = ld->bbox[BOXTOP]    + MAXRADIUS;

    P_BoxToBlockmapBlocks(BlockMap, blockBox, bbox);
    P_BlockBoxMobjsIterator(BlockMap, blockBox,
                            PTR_CheckMobjBlocking, &params);

    return params.blocked;
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

/**
 * Iterate the linedefs of the polyobj calling func for each.
 * Iteration will stop if func returns false.
 *
 * @param po            The polyobj whose lines are to be iterated.
 * @param func          Call back function to call for each line of this po.
 * @return              @c true, if all callbacks are successfull.
 */
boolean PO_PolyobjLineIterator(polyobj_t *po, boolean (*func) (line_t *, void *),
                               void *data)
{
    uint        i;
    seg_t     **segList;

    segList = po->segs;
    for(i = 0; i < po->numsegs; ++i, segList++)
    {
        line_t     *line = (*segList)->linedef;

        if(line->validCount == validCount)
            continue;

        line->validCount = validCount;

        if(!func(line, data))
            return false;
    }

    return true;
}
