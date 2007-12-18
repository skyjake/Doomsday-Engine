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
#include "de_misc.h"

// MACROS ------------------------------------------------------------------

#define MAXRADIUS   32

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void updateSegBBox(seg_t *seg);
static void RotatePt(int an, float *x, float *y, float startSpotX,
                     float startSpotY);
static boolean CheckMobjBlocking(seg_t *seg, polyobj_t *po);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// Called when the polyobj hits a mobj.
void    (*po_callback) (mobj_t *mobj, void *seg, void *po);

polyobj_t **polyobjs; // List of all poly-objects in the map.
uint    numpolyobjs;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

/**
 * The po_callback is called when a polyobj hits a mobj.
 */
void PO_SetCallback(void (*func) (mobj_t *, void *, void *))
{
    po_callback = func;
}

polyobj_t *P_GetPolyobj(uint polyNum)
{
    uint            i;

    for(i = 0; i < numpolyobjs; ++i)
    {
        polyobj_t      *po = polyobjs[i];

        if((uint) po->tag == polyNum)
        {
            return po;
        }
    }

    return NULL;
}

static void updateSegBBox(seg_t *seg)
{
    line_t         *line = seg->linedef;
    byte            edge;

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
void P_PolyobjUpdateBBox(polyobj_t *po)
{
    uint            i;
    vec2_t          point;
    vertex_t       *vtx;
    seg_t         **segPtr;

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
void PO_InitForMap(void)
{
    uint                i;

    for(i = 0; i < numpolyobjs; ++i)
    {
        polyobj_t          *po = polyobjs[i];
        seg_t            **segPtr;
        subsector_t        *ssec;
        fvertex_t           avg; // Used to find a polyobj's center, and hence subsector.

        avg.pos[VX] = 0;
        avg.pos[VY] = 0;

        segPtr = po->segs;
        while(*segPtr)
        {
            seg_t              *seg = *segPtr;
            side_t             *side = seg->linedef->L_frontside;

            side->SW_topflags |= SUF_NO_RADIO;
            side->SW_middleflags |= SUF_NO_RADIO;
            side->SW_bottomflags |= SUF_NO_RADIO;

            avg.pos[VX] += seg->SG_v1pos[VX];
            avg.pos[VY] += seg->SG_v1pos[VY];

            *segPtr++;
        }

        avg.pos[VX] /= po->numsegs;
        avg.pos[VY] /= po->numsegs;

        ssec = R_PointInSubsector(avg.pos[VX], avg.pos[VY]);
        if(ssec)
        {
            if(ssec->poly)
            {
                Con_Message("PO_InitForMap: Warning: Multiple polyobjs in a single subsector\n"
                            "  (ssec %i, sector %i). Previous polyobj overridden.\n",
                            GET_SUBSECTOR_IDX(ssec), GET_SECTOR_IDX(ssec->sector));
            }
            ssec->poly = po;
        }

        P_PolyobjUnLink(po);
        P_PolyobjLink(po);
    }
}

boolean P_PolyobjMove(uint num, float x, float y)
{
    uint            count;
    polyobj_t      *po;
    fvertex_t      *prevPts;
    seg_t         **segList;
    seg_t         **veryTempSeg;
    boolean         blocked;

    if(num & 0x80000000)
    {
        po = polyobjs[num & 0x7fffffff];
    }
    else if(!(po = P_GetPolyobj(num)))
    {
        Con_Error("P_PolyobjMove:  Invalid polyobj number: %d\n", num);
    }

    P_PolyobjUnLink(po);

    segList = po->segs;
    prevPts = po->prevPts;
    blocked = false;

    validCount++;
    for(count = 0; count < po->numsegs; ++count, segList++, prevPts++)
    {
        seg_t          *seg = *segList;

        if(seg->linedef->validCount != validCount)
        {
            seg->linedef->bbox[BOXTOP]    += y;
            seg->linedef->bbox[BOXBOTTOM] += y;
            seg->linedef->bbox[BOXLEFT]   += x;
            seg->linedef->bbox[BOXRIGHT]  += x;
            seg->linedef->validCount = validCount;
        }

        for(veryTempSeg = po->segs; veryTempSeg != segList; veryTempSeg++)
        {
            if((*veryTempSeg)->SG_v1 == seg->SG_v1)
            {
                break;
            }
        }

        if(veryTempSeg == segList)
        {
            seg->SG_v1pos[VX] += x;
            seg->SG_v1pos[VY] += y;
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
            seg_t              *seg = *segList;

            if(seg->linedef->validCount != validCount)
            {
                seg->linedef->bbox[BOXTOP]    -= y;
                seg->linedef->bbox[BOXBOTTOM] -= y;
                seg->linedef->bbox[BOXLEFT]   -= x;
                seg->linedef->bbox[BOXRIGHT]  -= x;
                seg->linedef->validCount = validCount;
            }

            for(veryTempSeg = po->segs; veryTempSeg != segList; veryTempSeg++)
            {
                if((*veryTempSeg)->SG_v1 == seg->SG_v1)
                {
                    break;
                }
            }

            if(veryTempSeg == segList)
            {
                seg->SG_v1pos[VX] -= x;
                seg->SG_v1pos[VY] -= y;
            }

            (*prevPts).pos[VX] -= x;
            (*prevPts).pos[VY] -= y;

            segList++;
            prevPts++;
        }

        P_PolyobjLink(po);
        return false;
    }

    po->startSpot.pos[VX] += x;
    po->startSpot.pos[VY] += y;
    P_PolyobjLink(po);

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

boolean P_PolyobjRotate(uint num, angle_t angle)
{
    int             an;
    uint            count;
    fvertex_t      *originalPts;
    fvertex_t      *prevPts;
    polyobj_t      *po;
    vertex_t       *vtx;
    seg_t         **segList;
    boolean         blocked;

    if(num & 0x80000000)
    {
        po = polyobjs[num & 0x7fffffff];
    }
    else if(!(po = P_GetPolyobj(num)))
    {
        Con_Error("P_PolyobjRotate: Invalid polyobj number: %d\n", num);
    }
    an = (po->angle + angle) >> ANGLETOFINESHIFT;

    P_PolyobjUnLink(po);

    segList = po->segs;
    originalPts = po->originalPts;
    prevPts = po->prevPts;

    for(count = 0; count < po->numsegs;
        ++count, segList++, originalPts++, prevPts++)
    {
        seg_t          *seg = *segList;

        vtx = seg->SG_v1;

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
        seg_t          *seg = *segList;

        if(CheckMobjBlocking(seg, po))
        {
            blocked = true;
        }

        if(seg->linedef->validCount != validCount)
        {
            updateSegBBox(seg);
            seg->linedef->validCount = validCount;
        }

        seg->angle += angle;
    }

    if(blocked)
    {
        segList = po->segs;
        prevPts = po->prevPts;
        for(count = 0; count < po->numsegs; ++count, segList++, prevPts++)
        {
            seg_t          *seg = *segList;

            vtx = seg->SG_v1;
            vtx->V_pos[VX] = prevPts->pos[VX];
            vtx->V_pos[VY] = prevPts->pos[VY];
        }

        segList = po->segs;
        validCount++;
        for(count = 0; count < po->numsegs; ++count, segList++, prevPts++)
        {
            seg_t          *seg = *segList;

            if(seg->linedef->validCount != validCount)
            {
                updateSegBBox(seg);
                seg->linedef->validCount = validCount;
            }
            seg->angle -= angle;
        }

        P_PolyobjLink(po);
        return false;
    }

    po->angle += angle;
    P_PolyobjLink(po);
    P_PolyobjChanged(po);
    return true;
}

void P_PolyobjLinkToRing(polyobj_t *po, linkpolyobj_t **link)
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

void P_PolyobjUnlinkFromRing(polyobj_t *po, linkpolyobj_t **list)
{
    linkpolyobj_t *iter = *list;

    while(iter != NULL && iter->polyobj != po)
    {
        iter = iter->next;
    }

    if(iter != NULL)
    {
        iter->polyobj = NULL;
    }
}

void P_PolyobjUnLink(polyobj_t *po)
{
    P_BlockmapUnlinkPolyobj(BlockMap, po);
}

void P_PolyobjLink(polyobj_t *po)
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
 *                      else @c NULL.
 */
polyobj_t* PO_GetPolyobjForDegen(void *degenMobj)
{
    uint        i;
    polyobj_t  *po;

    for(i = 0; i < numpolyobjs; ++i)
    {
        po = polyobjs[i];

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
boolean P_PolyobjLinesIterator(polyobj_t *po, boolean (*func) (line_t *, void *),
                               void *data)
{
    uint            i;
    seg_t         **segList;

    segList = po->segs;
    for(i = 0; i < po->numsegs; ++i, segList++)
    {
        seg_t          *seg = *segList;
        line_t         *line = seg->linedef;

        if(line->validCount == validCount)
            continue;

        line->validCount = validCount;

        if(!func(line, data))
            return false;
    }

    return true;
}
