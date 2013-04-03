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

static bool checkMobjBlocking(Polyobj &po, LineDef &line);

void Polyobj::updateAABox()
{
    LineDef **lineIter = lines;
    if(!*lineIter) return; // Very odd...

    LineDef *line = *lineIter;
    V2d_InitBox(aaBox.arvec2, line->v1Origin());
    lineIter++;

    while(*lineIter)
    {
        line = *lineIter;
        V2d_AddToBox(aaBox.arvec2, line->v1Origin());
        lineIter++;
    }
}

void Polyobj::updateSurfaceTangents()
{
    for(LineDef **lineIter = lines; *lineIter; lineIter++)
    {
        LineDef *line = *lineIter;

        line->frontSideDef().updateSurfaceTangents();
        if(line->hasBackSideDef())
        {
            line->backSideDef().updateSurfaceTangents();
        }
    }
}

static bool mobjIsBlockingPolyobj(Polyobj &po)
{
    for(LineDef **lineIter = po.lines; *lineIter; lineIter++)
    {
        if(checkMobjBlocking(po, **lineIter))
            return true;
    }
    return false; // All clear.
}

bool Polyobj::move(const_pvec2d_t delta)
{
    P_PolyobjUnlink(this);

    LineDef **lineIter = lines;
    povertex_t *prevPtIt = prevPts;
    uint i;
    for(i = 0; i < lineCount; ++i, lineIter++, prevPtIt++)
    {
        LineDef *line = *lineIter;
        LineDef **veryTempLine;

        for(veryTempLine = lines; veryTempLine != lineIter; veryTempLine++)
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

        (*prevPtIt).origin[VX] += delta[VX]; // Previous points are unique for each hedge.
        (*prevPtIt).origin[VY] += delta[VY];
    }

    lineIter = lines;
    for(i = 0; i < lineCount; ++i, lineIter++)
    {
        LineDef *line = *lineIter;
        line->updateAABox();
    }
    origin[VX] += delta[VX];
    origin[VY] += delta[VY];
    updateAABox();

    // With translation applied now determine if we collided with anything.
    P_PolyobjLink(this);
    if(mobjIsBlockingPolyobj(*this))
    {
        // Something is blocking our path. We must undo...
        P_PolyobjUnlink(this);

        i = 0;
        lineIter = lines;
        prevPtIt = prevPts;
        for(i = 0; i < lineCount; ++i, lineIter++, prevPtIt++)
        {
            LineDef *line = *lineIter;
            LineDef **veryTempLine;

            for(veryTempLine = lines; veryTempLine != lineIter; veryTempLine++)
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

            (*prevPtIt).origin[VX] -= delta[VX];
            (*prevPtIt).origin[VY] -= delta[VY];
        }

        lineIter = lines;
        for(i = 0; i < lineCount; ++i, lineIter++)
        {
            LineDef *line = *lineIter;
            line->updateAABox();
        }
        origin[VX] -= delta[VX];
        origin[VY] -= delta[VY];
        updateAABox();

        P_PolyobjLink(this);
        return false;
    }

    // Various parties may be interested in this change; signal it.
    P_PolyobjChanged(this);

    return true;
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

bool Polyobj::rotate(angle_t angle)
{
    P_PolyobjUnlink(this);

    uint fineAngle = (angle + angle) >> ANGLETOFINESHIFT;

    LineDef **lineIter       = lines;
    povertex_t *originalPtIt = originalPts;
    povertex_t *prevPtIt     = prevPts;
    uint i = 0;
    for(i = 0; i < lineCount; ++i, lineIter++, originalPtIt++, prevPtIt++)
    {
        LineDef *line = *lineIter;
        Vertex &vtx = line->v1();

        V2d_Copy(prevPtIt->origin, vtx.origin());
        V2d_Copy(vtx._origin, originalPtIt->origin);

        rotatePoint2d(vtx._origin, origin, fineAngle);
    }

    lineIter = lines;
    for(i = 0; i < lineCount; ++i, lineIter++)
    {
        LineDef *line = *lineIter;

        line->updateAABox();
        line->updateSlopeType();
        line->_angle += ANGLE_TO_BANG(angle);

        // HEdge angle must be kept in sync.
        line->front().leftHEdge()._angle = BANG_TO_ANGLE(line->_angle);
    }
    updateAABox();
    angle += angle;

    // With rotation applied now determine if we collided with anything.
    P_PolyobjLink(this);
    if(mobjIsBlockingPolyobj(*this))
    {
        // Something is blocking our path. We must undo...
        P_PolyobjUnlink(this);

        lineIter = lines;
        prevPtIt = prevPts;
        for(i = 0; i < lineCount; ++i, lineIter++, prevPtIt++)
        {
            Vertex &vtx = (*lineIter)->v1();
            V2d_Copy(vtx._origin, prevPtIt->origin);
        }

        lineIter = lines;
        for(i = 0; i < lineCount; ++i, lineIter++)
        {
            LineDef *line = *lineIter;

            line->updateAABox();
            line->updateSlopeType();
            line->_angle -= ANGLE_TO_BANG(angle);

            // HEdge angle must be kept in sync.
            line->front().leftHEdge()._angle = BANG_TO_ANGLE(line->_angle);
        }
        updateAABox();
        angle -= angle;

        P_PolyobjLink(this);
        return false;
    }

    updateSurfaceTangents();

    // Various parties may be interested in this change; signal it.
    P_PolyobjChanged(this);
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

        if(!(moBox.maxX <= params->line->aaBox().minX ||
             moBox.minX >= params->line->aaBox().maxX ||
             moBox.maxY <= params->line->aaBox().minY ||
             moBox.minY >= params->line->aaBox().maxY))
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

static bool checkMobjBlocking(Polyobj &po, LineDef &line)
{
    ptrmobjblockingparams_t params;
    params.isBlocked = false;
    params.line      = &line;
    params.polyobj   = &po;

    AABoxd aaBox(line.aaBox().minX - DDMOBJ_RADIUS_MAX,
                 line.aaBox().minY - DDMOBJ_RADIUS_MAX,
                 line.aaBox().maxX + DDMOBJ_RADIUS_MAX,
                 line.aaBox().maxY + DDMOBJ_RADIUS_MAX);

    validCount++;
    P_MobjsBoxIterator(&aaBox, PTR_checkMobjBlocking, &params);

    return params.isBlocked;
}

int Polyobj::lineIterator(int (*callback) (LineDef *, void *), void *parameters)
{
    int result = false; // Continue iteration.
    if(callback)
    {
        for(LineDef **lineIter = lines; *lineIter; lineIter++)
        {
            LineDef *line = *lineIter;

            if(line->validCount() == validCount) continue;
            line->_validCount = validCount;

            result = callback(line, parameters);
            if(result) break;
        }
    }
    return result;
}
