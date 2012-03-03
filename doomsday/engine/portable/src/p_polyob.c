/**
 * @file p_polyob.c
 * Polyobj implementation. @ingroup map
 *
 * @authors Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2012 Daniel Swanson <danij@dengine.net>
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
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#include "de_base.h"
#include "de_console.h"
#include "de_play.h"
#include "de_refresh.h"
#include "de_misc.h"

static void rotatePoint(int an, float* x, float* y, float startSpotX, float startSpotY);
static boolean checkMobjBlocking(linedef_t* line, polyobj_t* po);

// Called when the polyobj hits a mobj.
static void (*po_callback) (mobj_t* mobj, void* line, void* polyobj);

polyobj_t** polyObjs; // List of all poly-objects in the map.
uint numPolyObjs;

void P_SetPolyobjCallback(void (*func) (struct mobj_s*, void*, void*))
{
    po_callback = func;
}

polyobj_t* P_PolyobjByID(uint id)
{
    if(id < numPolyObjs)
        return polyObjs[id];
    return NULL;
}

polyobj_t* P_PolyobjByTag(int tag)
{
    uint i;
    for(i = 0; i < numPolyObjs; ++i)
    {
        polyobj_t* po = polyObjs[i];
        if(po->tag == tag)
        {
            return po;
        }
    }
    return NULL;
}

polyobj_t* P_PolyobjByOrigin(void* ddMobjBase)
{
    uint i;
    for(i = 0; i < numPolyObjs; ++i)
    {
        polyobj_t* po = polyObjs[i];
        if(po == ddMobjBase)
        {
            return po;
        }
    }
    return NULL;
}

void Polyobj_UpdateAABox(polyobj_t* po)
{
    linedef_t** lineIter;
    linedef_t* line;
    assert(po);

    lineIter = po->lines;
    if(!*lineIter) return; // Very odd...

    line = *lineIter;
    V2_InitBox(po->aaBox.arvec2, line->L_v1pos);
    lineIter++;

    while(*lineIter)
    {
        line = *lineIter;
        V2_AddToBox(po->aaBox.arvec2, line->L_v1pos);
        lineIter++;
    }
}

void Polyobj_UpdateSurfaceTangents(polyobj_t* po)
{
    linedef_t** lineIter;
    assert(po);

    for(lineIter = po->lines; *lineIter; lineIter++)
    {
        linedef_t* line = *lineIter;

        SideDef_UpdateSurfaceTangents(line->L_frontside);
        if(line->L_backside)
        {
            SideDef_UpdateSurfaceTangents(line->L_backside);
        }
    }
}

void P_MapInitPolyobj(polyobj_t* po)
{
    linedef_t** lineIter;
    subsector_t* ssec;
    vec2_t avg; // < Used to find a polyobj's center, and hence subsector.

    if(!po) return;

    V2_Set(avg, 0, 0);
    for(lineIter = po->lines; *lineIter; lineIter++)
    {
        linedef_t* line = *lineIter;
        sidedef_t* front = line->L_frontside;

        front->SW_topinflags |= SUIF_NO_RADIO;
        front->SW_middleinflags |= SUIF_NO_RADIO;
        front->SW_bottominflags |= SUIF_NO_RADIO;

        if(line->L_backside)
        {
            sidedef_t* back = line->L_backside;

            back->SW_topinflags |= SUIF_NO_RADIO;
            back->SW_middleinflags |= SUIF_NO_RADIO;
            back->SW_bottominflags |= SUIF_NO_RADIO;
        }

        V2_Sum(avg, avg, line->L_v1pos);
    }
    V2_Scale(avg, 1.f / po->lineCount);

    ssec = R_PointInSubsector(avg[VX], avg[VY]);
    if(ssec)
    {
        if(ssec->polyObj)
        {
            Con_Message("Warning: P_MapInitPolyobj: Multiple polyobjs in a single subsector\n"
                        "  (subsector %ld, sector %ld). Previous polyobj overridden.\n",
                        (long)GET_SUBSECTOR_IDX(ssec), (long)GET_SECTOR_IDX(ssec->sector));
        }
        ssec->polyObj = po;
        po->subsector = ssec;
    }

    Polyobj_UpdateAABox(po);
    Polyobj_UpdateSurfaceTangents(po);

    P_PolyobjUnlink(po);
    P_PolyobjLink(po);
}

void P_MapInitPolyobjs(void)
{
    uint i;
    for(i = 0; i < numPolyObjs; ++i)
    {
        P_MapInitPolyobj(polyObjs[i]);
    }
}

static boolean mobjIsBlockingPolyobj(polyobj_t* po)
{
    linedef_t** lineIter;
    if(!po) return false;

    for(lineIter = po->lines; *lineIter; lineIter++)
    {
        linedef_t* line = *lineIter;
        if(checkMobjBlocking(line, po))
        {
            return true;
        }
    }
    // All clear.
    return false;
}

boolean P_PolyobjMove(polyobj_t* po, float delta[2])
{
    fvertex_t* prevPts;
    linedef_t** lineIter;
    uint i;

    if(!po) return false;

    P_PolyobjUnlink(po);

    lineIter = po->lines;
    prevPts = po->prevPts;
    for(i = 0; i < po->lineCount; ++i, lineIter++, prevPts++)
    {
        linedef_t* line = *lineIter;
        linedef_t** veryTempLine;

        for(veryTempLine = po->lines; veryTempLine != lineIter; veryTempLine++)
        {
            if((*veryTempLine)->L_v1 == line->L_v1)
            {
                break;
            }
        }

        if(veryTempLine == lineIter)
        {
            line->L_v1pos[VX] += delta[VX];
            line->L_v1pos[VY] += delta[VY];
        }

        (*prevPts).pos[VX] += delta[VX]; // Previous points are unique for each hedge.
        (*prevPts).pos[VY] += delta[VY];
    }

    lineIter = po->lines;
    for(i = 0; i < po->lineCount; ++i, lineIter++)
    {
        linedef_t* line = *lineIter;
        LineDef_UpdateAABox(line);
    }
    po->pos[VX] += delta[VX];
    po->pos[VY] += delta[VY];
    Polyobj_UpdateAABox(po);

    // With translation applied now determine if we collided with anything.
    P_PolyobjLink(po);
    if(mobjIsBlockingPolyobj(po))
    {
        // Something is blocking our path. We must undo...
        P_PolyobjUnlink(po);

        i = 0;
        lineIter = po->lines;
        prevPts = po->prevPts;
        for(i = 0; i < po->lineCount; ++i, lineIter++, prevPts++)
        {
            linedef_t* line = *lineIter;
            linedef_t** veryTempLine;

            for(veryTempLine = po->lines; veryTempLine != lineIter; veryTempLine++)
            {
                if((*veryTempLine)->L_v1 == line->L_v1)
                {
                    break;
                }
            }

            if(veryTempLine == lineIter)
            {
                line->L_v1pos[VX] -= delta[VX];
                line->L_v1pos[VY] -= delta[VY];
            }

            (*prevPts).pos[VX] -= delta[VX];
            (*prevPts).pos[VY] -= delta[VY];
        }

        lineIter = po->lines;
        for(i = 0; i < po->lineCount; ++i, lineIter++)
        {
            linedef_t* line = *lineIter;
            LineDef_UpdateAABox(line);
        }
        po->pos[VX] -= delta[VX];
        po->pos[VY] -= delta[VY];
        Polyobj_UpdateAABox(po);

        P_PolyobjLink(po);
        return false;
    }

    // Various parties may be interested in this change; signal it.
    P_PolyobjChanged(po);

    return true;
}

boolean P_PolyobjMoveXY(polyobj_t* po, float x, float y)
{
    float delta[2];
    delta[VX] = x;
    delta[VY] = y;
    return P_PolyobjMove(po, delta);
}

static void rotatePoint2d(float point[2], const float origin[2], uint fineAngle)
{
    float orig[2], rotated[2];

    orig[VX] = point[VX];
    orig[VY] = point[VY];

    rotated[VX] = orig[VX] * FIX2FLT(fineCosine[fineAngle]);
    rotated[VY] = orig[VY] * FIX2FLT(finesine[fineAngle]);
    point[VX] = rotated[VX] - rotated[VY] + origin[VX];

    rotated[VX] = orig[VX] * FIX2FLT(finesine[fineAngle]);
    rotated[VY] = orig[VY] * FIX2FLT(fineCosine[fineAngle]);
    point[VY] = rotated[VY] + rotated[VX] + origin[VY];
}

boolean P_PolyobjRotate(polyobj_t* po, angle_t angle)
{
    fvertex_t* originalPts, *prevPts;
    uint i, fineAngle;
    linedef_t** lineIter;

    if(!po) return false;

    P_PolyobjUnlink(po);

    lineIter = po->lines;
    originalPts = po->originalPts;
    prevPts = po->prevPts;

    fineAngle = (po->angle + angle) >> ANGLETOFINESHIFT;
    for(i = 0; i < po->lineCount; ++i, lineIter++, originalPts++, prevPts++)
    {
        linedef_t* line = *lineIter;
        vertex_t* vtx = line->L_v1;

        prevPts->pos[VX] = vtx->V_pos[VX];
        prevPts->pos[VY] = vtx->V_pos[VY];
        vtx->V_pos[VX] = originalPts->pos[VX];
        vtx->V_pos[VY] = originalPts->pos[VY];

        rotatePoint2d(vtx->V_pos, po->pos, fineAngle);
    }

    lineIter = po->lines;
    for(i = 0; i < po->lineCount; ++i, lineIter++)
    {
        linedef_t* line = *lineIter;

        LineDef_UpdateAABox(line);
        LineDef_UpdateSlope(line);
        line->angle += ANGLE_TO_BANG(angle);

        // HEdge angle must be kept in sync.
        line->L_frontside->hedges[0]->angle = BANG_TO_ANGLE(line->angle);
    }
    Polyobj_UpdateAABox(po);
    po->angle += angle;

    // With rotation applied now determine if we collided with anything.
    P_PolyobjLink(po);
    if(mobjIsBlockingPolyobj(po))
    {
        // Something is blocking our path. We must undo...
        P_PolyobjUnlink(po);

        lineIter = po->lines;
        prevPts = po->prevPts;
        for(i = 0; i < po->lineCount; ++i, lineIter++, prevPts++)
        {
            linedef_t* line = *lineIter;
            vertex_t* vtx = line->L_v1;
            vtx->V_pos[VX] = prevPts->pos[VX];
            vtx->V_pos[VY] = prevPts->pos[VY];
        }

        lineIter = po->lines;
        for(i = 0; i < po->lineCount; ++i, lineIter++)
        {
            linedef_t* line = *lineIter;

            LineDef_UpdateAABox(line);
            LineDef_UpdateSlope(line);
            line->angle -= ANGLE_TO_BANG(angle);

            // HEdge angle must be kept in sync.
            line->L_frontside->hedges[0]->angle = BANG_TO_ANGLE(line->angle);
        }
        Polyobj_UpdateAABox(po);
        po->angle -= angle;

        P_PolyobjLink(po);
        return false;
    }

    Polyobj_UpdateSurfaceTangents(po);

    // Various parties may be interested in this change; signal it.
    P_PolyobjChanged(po);
    return true;
}

void P_PolyobjUnlink(polyobj_t* po)
{
    gamemap_t* map = P_GetCurrentMap();
    Map_UnlinkPolyobjInBlockmap(map, po);
}

void P_PolyobjLink(polyobj_t* po)
{
    gamemap_t* map = P_GetCurrentMap();
    Map_LinkPolyobjInBlockmap(map, po);
}

typedef struct ptrmobjblockingparams_s {
    boolean isBlocked;
    linedef_t* line;
    polyobj_t* polyobj;
} ptrmobjblockingparams_t;

int PTR_checkMobjBlocking(mobj_t* mo, void* data)
{
    if((mo->ddFlags & DDMF_SOLID) ||
       (mo->dPlayer && !(mo->dPlayer->flags & DDPF_CAMERA)))
    {
        ptrmobjblockingparams_t* params = data;
        AABoxf moBox;

        moBox.minX = mo->pos[VX] - mo->radius;
        moBox.minY = mo->pos[VY] - mo->radius;
        moBox.maxX = mo->pos[VX] + mo->radius;
        moBox.maxY = mo->pos[VY] + mo->radius;

        if(!(moBox.maxX <= params->line->aaBox.minX ||
             moBox.minX >= params->line->aaBox.maxX ||
             moBox.maxY <= params->line->aaBox.minY ||
             moBox.minY >= params->line->aaBox.maxY))
        {
            if(P_BoxOnLineSide(&moBox, params->line) == -1)
            {
                if(po_callback)
                    po_callback(mo, params->line, params->polyobj);

                params->isBlocked = true;
            }
        }
    }

    return false; // Continue iteration.
}

static boolean checkMobjBlocking(linedef_t* line, polyobj_t* po)
{
    gamemap_t* map = P_GetCurrentMap();
    ptrmobjblockingparams_t params;
    GridmapBlock blockCoords;
    AABoxf aaBox;

    params.isBlocked = false;
    params.line = line;
    params.polyobj = po;

    aaBox.minX = line->aaBox.minX - DDMOBJ_RADIUS_MAX;
    aaBox.minY = line->aaBox.minY - DDMOBJ_RADIUS_MAX;
    aaBox.maxX = line->aaBox.maxX + DDMOBJ_RADIUS_MAX;
    aaBox.maxY = line->aaBox.maxY + DDMOBJ_RADIUS_MAX;

    validCount++;
    Blockmap_CellBlockCoords(map->mobjBlockmap, &blockCoords, &aaBox);
    Map_IterateCellBlockMobjs(map, &blockCoords, PTR_checkMobjBlocking, &params);

    return params.isBlocked;
}

int Polyobj_LineIterator(polyobj_t* po, int (*callback) (struct linedef_s*, void*),
    void* paramaters)
{
    int result = false; // Continue iteration.
    if(callback)
    {
        linedef_t** lineIter;
        for(lineIter = po->lines; *lineIter; lineIter++)
        {
            linedef_t* line = *lineIter;

            if(line->validCount == validCount) continue;
            line->validCount = validCount;

            result = callback(line, paramaters);
            if(result) break;
        }
    }
    return result;
}
