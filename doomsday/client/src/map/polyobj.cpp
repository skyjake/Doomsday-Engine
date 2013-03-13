/** @file polyobj.cpp Polyobj implementation. 
 * @ingroup map
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
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#include "de_base.h"
#include "de_console.h"
#include "de_play.h"
#include "de_misc.h"

#include "render/r_main.h" // validCount

static void rotatePoint(int an, coord_t *x, coord_t *y, coord_t startSpotX, coord_t startSpotY);
static boolean checkMobjBlocking(LineDef *line, Polyobj *po);

void Polyobj_UpdateAABox(Polyobj *po)
{
    DENG2_ASSERT(po);

    LineDef **lineIter = po->lines;
    if(!*lineIter) return; // Very odd...

    LineDef *line = *lineIter;
    V2d_InitBox(po->aaBox.arvec2, line->v1Origin());
    lineIter++;

    while(*lineIter)
    {
        line = *lineIter;
        V2d_AddToBox(po->aaBox.arvec2, line->v1Origin());
        lineIter++;
    }
}

void Polyobj_UpdateSurfaceTangents(Polyobj *po)
{
    DENG2_ASSERT(po);

    for(LineDef **lineIter = po->lines; *lineIter; lineIter++)
    {
        LineDef *line = *lineIter;

        line->L_frontsidedef->updateSurfaceTangents();
        if(line->L_backsidedef)
        {
            line->L_backsidedef->updateSurfaceTangents();
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

boolean Polyobj_Move(Polyobj *po, coord_t delta[2])
{
    DENG2_ASSERT(po);

    P_PolyobjUnlink(po);

    LineDef **lineIter = po->lines;
    povertex_t *prevPts = po->prevPts;
    uint i;
    for(i = 0; i < po->lineCount; ++i, lineIter++, prevPts++)
    {
        LineDef *line = *lineIter;
        LineDef **veryTempLine;

        for(veryTempLine = po->lines; veryTempLine != lineIter; veryTempLine++)
        {
            if(&(*veryTempLine)->v1() == &line->v1())
            {
                break;
            }
        }

        if(veryTempLine == lineIter)
        {
            line->v1()._origin[VX] += delta[VX];
            line->v1()._origin[VY] += delta[VY];
        }

        (*prevPts).origin[VX] += delta[VX]; // Previous points are unique for each hedge.
        (*prevPts).origin[VY] += delta[VY];
    }

    lineIter = po->lines;
    for(i = 0; i < po->lineCount; ++i, lineIter++)
    {
        LineDef *line = *lineIter;
        line->updateAABox();
    }
    po->origin[VX] += delta[VX];
    po->origin[VY] += delta[VY];
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
            LineDef *line = *lineIter;
            LineDef **veryTempLine;

            for(veryTempLine = po->lines; veryTempLine != lineIter; veryTempLine++)
            {
                if(&(*veryTempLine)->v1() == &line->v1())
                {
                    break;
                }
            }

            if(veryTempLine == lineIter)
            {
                line->v1()._origin[VX] -= delta[VX];
                line->v1()._origin[VY] -= delta[VY];
            }

            (*prevPts).origin[VX] -= delta[VX];
            (*prevPts).origin[VY] -= delta[VY];
        }

        lineIter = po->lines;
        for(i = 0; i < po->lineCount; ++i, lineIter++)
        {
            LineDef *line = *lineIter;
            line->updateAABox();
        }
        po->origin[VX] -= delta[VX];
        po->origin[VY] -= delta[VY];
        Polyobj_UpdateAABox(po);

        P_PolyobjLink(po);
        return false;
    }

    // Various parties may be interested in this change; signal it.
    P_PolyobjChanged(po);

    return true;
}

boolean Polyobj_MoveXY(Polyobj *po, coord_t x, coord_t y)
{
    coord_t delta[2] = { x, y };
    return Polyobj_Move(po, delta);
}

static void rotatePoint2d(coord_t point[2], coord_t const origin[2], uint fineAngle)
{
    coord_t orig[2], rotated[2];

    orig[VX] = point[VX];
    orig[VY] = point[VY];

    rotated[VX] = orig[VX] * FIX2FLT(fineCosine[fineAngle]);
    rotated[VY] = orig[VY] * FIX2FLT(finesine[fineAngle]);
    point[VX] = rotated[VX] - rotated[VY] + origin[VX];

    rotated[VX] = orig[VX] * FIX2FLT(finesine[fineAngle]);
    rotated[VY] = orig[VY] * FIX2FLT(fineCosine[fineAngle]);
    point[VY] = rotated[VY] + rotated[VX] + origin[VY];
}

boolean Polyobj_Rotate(Polyobj *po, angle_t angle)
{
    DENG2_ASSERT(po);

    P_PolyobjUnlink(po);

    uint fineAngle = (po->angle + angle) >> ANGLETOFINESHIFT;

    LineDef **lineIter      = po->lines;
    povertex_t *originalPts = po->originalPts;
    povertex_t *prevPts     = po->prevPts;
    uint i = 0;
    for(i = 0; i < po->lineCount; ++i, lineIter++, originalPts++, prevPts++)
    {
        LineDef *line = *lineIter;
        Vertex &vtx = line->v1();

        V2d_Copy(prevPts->origin, vtx.origin());
        V2d_Copy(vtx._origin, originalPts->origin);

        rotatePoint2d(vtx._origin, po->origin, fineAngle);
    }

    lineIter = po->lines;
    for(i = 0; i < po->lineCount; ++i, lineIter++)
    {
        LineDef *line = *lineIter;

        line->updateAABox();
        line->updateSlope();
        line->angle += ANGLE_TO_BANG(angle);

        // HEdge angle must be kept in sync.
        line->front().hedgeLeft->angle = BANG_TO_ANGLE(line->angle);
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
            Vertex &vtx = (*lineIter)->v1();
            V2d_Copy(vtx._origin, prevPts->origin);
        }

        lineIter = po->lines;
        for(i = 0; i < po->lineCount; ++i, lineIter++)
        {
            LineDef *line = *lineIter;

            line->updateAABox();
            line->updateSlope();
            line->angle -= ANGLE_TO_BANG(angle);

            // HEdge angle must be kept in sync.
            line->front().hedgeLeft->angle = BANG_TO_ANGLE(line->angle);
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

typedef struct ptrmobjblockingparams_s {
    boolean isBlocked;
    LineDef *line;
    Polyobj *polyobj;
} ptrmobjblockingparams_t;

int PTR_checkMobjBlocking(mobj_t *mo, void *data)
{
    if((mo->ddFlags & DDMF_SOLID) ||
       (mo->dPlayer && !(mo->dPlayer->flags & DDPF_CAMERA)))
    {
        ptrmobjblockingparams_t *params = (ptrmobjblockingparams_t *) data;
        AABoxd moBox;

        moBox.minX = mo->origin[VX] - mo->radius;
        moBox.minY = mo->origin[VY] - mo->radius;
        moBox.maxX = mo->origin[VX] + mo->radius;
        moBox.maxY = mo->origin[VY] + mo->radius;

        if(!(moBox.maxX <= params->line->aaBox.minX ||
             moBox.minX >= params->line->aaBox.maxX ||
             moBox.maxY <= params->line->aaBox.minY ||
             moBox.minY >= params->line->aaBox.maxY))
        {
            if(!params->line->boxOnSide(moBox))
            {
                P_PolyobjCallback(mo, params->line, params->polyobj);

                params->isBlocked = true;
            }
        }
    }

    return false; // Continue iteration.
}

static boolean checkMobjBlocking(LineDef *line, Polyobj *po)
{
    ptrmobjblockingparams_t params;
    params.isBlocked = false;
    params.line      = line;
    params.polyobj   = po;

    AABoxd aaBox(line->aaBox.minX - DDMOBJ_RADIUS_MAX,
                 line->aaBox.minY - DDMOBJ_RADIUS_MAX,
                 line->aaBox.maxX + DDMOBJ_RADIUS_MAX,
                 line->aaBox.maxY + DDMOBJ_RADIUS_MAX);

    validCount++;
    P_MobjsBoxIterator(&aaBox, PTR_checkMobjBlocking, &params);

    return params.isBlocked;
}

int Polyobj_LineIterator(Polyobj *po, int (*callback) (LineDef *, void *),
    void *parameters)
{
    int result = false; // Continue iteration.
    if(callback)
    {
        for(LineDef **lineIter = po->lines; *lineIter; lineIter++)
        {
            LineDef *line = *lineIter;

            if(line->validCount == validCount) continue;
            line->validCount = validCount;

            result = callback(line, parameters);
            if(result) break;
        }
    }
    return result;
}
