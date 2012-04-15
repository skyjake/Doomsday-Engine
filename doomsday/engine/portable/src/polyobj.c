/**
 * @file polyobj.c
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
static boolean checkMobjBlocking(LineDef* line, Polyobj* po);

void Polyobj_UpdateAABox(Polyobj* po)
{
    LineDef** lineIter;
    LineDef* line;
    assert(po);

    lineIter = po->lines;
    if(!*lineIter) return; // Very odd...

    line = *lineIter;
    V2f_InitBox(po->aaBox.arvec2, line->L_v1pos);
    lineIter++;

    while(*lineIter)
    {
        line = *lineIter;
        V2f_AddToBox(po->aaBox.arvec2, line->L_v1pos);
        lineIter++;
    }
}

void Polyobj_UpdateSurfaceTangents(Polyobj* po)
{
    LineDef** lineIter;
    assert(po);

    for(lineIter = po->lines; *lineIter; lineIter++)
    {
        LineDef* line = *lineIter;

        SideDef_UpdateSurfaceTangents(line->L_frontside);
        if(line->L_backside)
        {
            SideDef_UpdateSurfaceTangents(line->L_backside);
        }
    }
}

void Polyobj_UpdateSideDefOrigins(Polyobj* po)
{
    LineDef** lineIter;
    assert(po);

    for(lineIter = po->lines; *lineIter; lineIter++)
    {
        LineDef* line = *lineIter;

        SideDef_UpdateOrigin(line->L_frontside);
        if(line->L_backside)
        {
            SideDef_UpdateOrigin(line->L_backside);
        }
    }
}

static boolean mobjIsBlockingPolyobj(Polyobj* po)
{
    LineDef** lineIter;
    if(!po) return false;

    for(lineIter = po->lines; *lineIter; lineIter++)
    {
        LineDef* line = *lineIter;
        if(checkMobjBlocking(line, po))
        {
            return true;
        }
    }
    // All clear.
    return false;
}

boolean Polyobj_Move(Polyobj* po, float delta[2])
{
    povertex_t* prevPts;
    LineDef** lineIter;
    uint i;
    assert(po);

    P_PolyobjUnlink(po);

    lineIter = po->lines;
    prevPts = po->prevPts;
    for(i = 0; i < po->lineCount; ++i, lineIter++, prevPts++)
    {
        LineDef* line = *lineIter;
        LineDef** veryTempLine;

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
        LineDef* line = *lineIter;
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
            LineDef* line = *lineIter;
            LineDef** veryTempLine;

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
            LineDef* line = *lineIter;
            LineDef_UpdateAABox(line);
        }
        po->pos[VX] -= delta[VX];
        po->pos[VY] -= delta[VY];
        Polyobj_UpdateAABox(po);

        P_PolyobjLink(po);
        return false;
    }

    Polyobj_UpdateSideDefOrigins(po);

    // Various parties may be interested in this change; signal it.
    P_PolyobjChanged(po);

    return true;
}

boolean Polyobj_MoveXY(Polyobj* po, float x, float y)
{
    float delta[2];
    delta[VX] = x;
    delta[VY] = y;
    return Polyobj_Move(po, delta);
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

boolean Polyobj_Rotate(Polyobj* po, angle_t angle)
{
    povertex_t* originalPts, *prevPts;
    uint i, fineAngle;
    LineDef** lineIter;
    assert(po);

    P_PolyobjUnlink(po);

    lineIter = po->lines;
    originalPts = po->originalPts;
    prevPts = po->prevPts;

    fineAngle = (po->angle + angle) >> ANGLETOFINESHIFT;
    for(i = 0; i < po->lineCount; ++i, lineIter++, originalPts++, prevPts++)
    {
        LineDef* line = *lineIter;
        Vertex* vtx = line->L_v1;

        prevPts->pos[VX] = vtx->pos[VX];
        prevPts->pos[VY] = vtx->pos[VY];
        vtx->pos[VX] = originalPts->pos[VX];
        vtx->pos[VY] = originalPts->pos[VY];

        rotatePoint2d(vtx->pos, po->pos, fineAngle);
    }

    lineIter = po->lines;
    for(i = 0; i < po->lineCount; ++i, lineIter++)
    {
        LineDef* line = *lineIter;

        LineDef_UpdateAABox(line);
        LineDef_UpdateSlope(line);
        line->angle += ANGLE_TO_BANG(angle);

        // HEdge angle must be kept in sync.
        line->L_frontside->hedgeLeft->angle = BANG_TO_ANGLE(line->angle);
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
            LineDef* line = *lineIter;
            Vertex* vtx = line->L_v1;
            vtx->pos[VX] = prevPts->pos[VX];
            vtx->pos[VY] = prevPts->pos[VY];
        }

        lineIter = po->lines;
        for(i = 0; i < po->lineCount; ++i, lineIter++)
        {
            LineDef* line = *lineIter;

            LineDef_UpdateAABox(line);
            LineDef_UpdateSlope(line);
            line->angle -= ANGLE_TO_BANG(angle);

            // HEdge angle must be kept in sync.
            line->L_frontside->hedgeLeft->angle = BANG_TO_ANGLE(line->angle);
        }
        Polyobj_UpdateAABox(po);
        po->angle -= angle;

        P_PolyobjLink(po);
        return false;
    }

    Polyobj_UpdateSideDefOrigins(po);
    Polyobj_UpdateSurfaceTangents(po);

    // Various parties may be interested in this change; signal it.
    P_PolyobjChanged(po);
    return true;
}

typedef struct ptrmobjblockingparams_s {
    boolean isBlocked;
    LineDef* line;
    Polyobj* polyobj;
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
                P_PolyobjCallback(mo, params->line, params->polyobj);

                params->isBlocked = true;
            }
        }
    }

    return false; // Continue iteration.
}

static boolean checkMobjBlocking(LineDef* line, Polyobj* po)
{
    ptrmobjblockingparams_t params;
    AABoxf aaBox;

    params.isBlocked = false;
    params.line = line;
    params.polyobj = po;

    aaBox.minX = line->aaBox.minX - DDMOBJ_RADIUS_MAX;
    aaBox.minY = line->aaBox.minY - DDMOBJ_RADIUS_MAX;
    aaBox.maxX = line->aaBox.maxX + DDMOBJ_RADIUS_MAX;
    aaBox.maxY = line->aaBox.maxY + DDMOBJ_RADIUS_MAX;

    validCount++;
    P_MobjsBoxIterator(&aaBox, PTR_checkMobjBlocking, &params);

    return params.isBlocked;
}

int Polyobj_LineIterator(Polyobj* po, int (*callback) (struct linedef_s*, void*),
    void* paramaters)
{
    int result = false; // Continue iteration.
    if(callback)
    {
        LineDef** lineIter;
        for(lineIter = po->lines; *lineIter; lineIter++)
        {
            LineDef* line = *lineIter;

            if(line->validCount == validCount) continue;
            line->validCount = validCount;

            result = callback(line, paramaters);
            if(result) break;
        }
    }
    return result;
}
