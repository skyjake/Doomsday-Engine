/** @file polyobj.cpp Moveable Polygonal Map-Object (Polyobj).
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

#include <de/Vector>

#include "de_base.h"
#include "de_console.h"
#include "de_play.h"
#include "de_misc.h"
#include "render/r_main.h" // validCount

#include "map/polyobj.h"

using namespace de;

static void notifyGeometryChanged(Polyobj &po)
{
#ifdef __CLIENT__
    // Shadow bias must be informed when surfaces move/deform.
    foreach(LineDef *line, po.lines())
    {
        HEdge &hedge = line->front().leftHEdge();
        for(int i = 0; i < 3; ++i)
        {
            SB_SurfaceMoved(&hedge.biasSurfaceForGeometryGroup(i));
        }
    }
#else // !__CLIENT__
    DENG2_UNUSED(po);
#endif
}

Polyobj::Lines const &Polyobj::lines() const
{
    return *static_cast<Lines *>(_lines);
}

Polyobj::Vertexes const &Polyobj::uniqueVertexes() const
{
    return *static_cast<Vertexes *>(_uniqueVertexes);
}

void Polyobj::buildUniqueVertexes()
{
    Vertexes &uniqueVertexes = *static_cast<Vertexes *>(_uniqueVertexes);

    uniqueVertexes.clear();

    foreach(LineDef *line, lines())
    {
        uniqueVertexes.insert(&line->v1());
        uniqueVertexes.insert(&line->v2());
    }

    // Resize the point arrays as they are implicitly linked to the unique vertexes.
    originalPts = (povertex_t *) Z_Realloc(originalPts, uniqueVertexes.count() * sizeof(povertex_t), PU_MAP);
    prevPts     = (povertex_t *) Z_Realloc(prevPts,     uniqueVertexes.count() * sizeof(povertex_t), PU_MAP);
}

void Polyobj::updateAABox()
{
    V2d_Set(aaBox.min, DDMAXFLOAT, DDMAXFLOAT);
    V2d_Set(aaBox.max, DDMINFLOAT, DDMINFLOAT);

    if(!lineCount()) return;

    QListIterator<LineDef *> lineIt(lines());

    LineDef *line = lineIt.next();
    V2d_CopyBox(aaBox.arvec2, line->aaBox().arvec2);

    while(lineIt.hasNext())
    {
        line = lineIt.next();
        V2d_UniteBox(aaBox.arvec2, line->aaBox().arvec2);
    }
}

void Polyobj::updateSurfaceTangents()
{
    foreach(LineDef *line, lines())
    {
        line->front().updateSurfaceNormals();
        line->back().updateSurfaceNormals();
    }
}

struct ptrmobjblockingparams_t
{
    bool isBlocked;
    LineDef *line;
    Polyobj *polyobj;
};

static inline bool mobjCanBlockMovement(mobj_t *mo)
{
    DENG_ASSERT(mo != 0);
    return (mo->ddFlags & DDMF_SOLID) || (mo->dPlayer && !(mo->dPlayer->flags & DDPF_CAMERA));
}

static int PTR_CheckMobjBlocking(mobj_t *mo, void *context)
{
    ptrmobjblockingparams_t *parms = (ptrmobjblockingparams_t *) context;

    if(!mobjCanBlockMovement(mo)) return false;

    // Out of range?
    AABoxd moBox(mo->origin[VX] - mo->radius,
                 mo->origin[VY] - mo->radius,
                 mo->origin[VX] + mo->radius,
                 mo->origin[VY] + mo->radius);

    if(moBox.maxX <= parms->line->aaBox().minX ||
       moBox.minX >= parms->line->aaBox().maxX ||
       moBox.maxY <= parms->line->aaBox().minY ||
       moBox.minY >= parms->line->aaBox().maxY)
        return false;

    if(parms->line->boxOnSide(moBox))
        return false;

    // This mobj blocks our path!
    P_PolyobjCallback(mo, parms->line, parms->polyobj);
    parms->isBlocked = true;

    // Process all blocking mobjs...
    return false;
}

static bool checkMobjBlocking(Polyobj &po, LineDef &line)
{
    ptrmobjblockingparams_t parms;
    parms.isBlocked = false;
    parms.line      = &line;
    parms.polyobj   = &po;

    AABoxd interceptRange(line.aaBox().minX - DDMOBJ_RADIUS_MAX,
                          line.aaBox().minY - DDMOBJ_RADIUS_MAX,
                          line.aaBox().maxX + DDMOBJ_RADIUS_MAX,
                          line.aaBox().maxY + DDMOBJ_RADIUS_MAX);

    validCount++;
    P_MobjsBoxIterator(&interceptRange, PTR_CheckMobjBlocking, &parms);

    return parms.isBlocked;
}

static bool mobjIsBlockingPolyobj(Polyobj &po)
{
    foreach(LineDef *line, po.lines())
    {
        if(checkMobjBlocking(po, *line))
            return true;
    }
    return false; // All clear.
}

bool Polyobj::move(const_pvec2d_t delta)
{
    LOG_AS("Polyobj::move");
    //LOG_DEBUG("Applying delta %s to [%p]")
    //    << Vector2d(delta[VX], delta[VY]).asText() << this;

    P_PolyobjUnlink(this);
    {
        povertex_t *prevPtIt = prevPts;
        foreach(Vertex *vertex, uniqueVertexes())
        {
            V2d_Copy(prevPtIt->origin, vertex->origin());
            V2d_Sum(vertex->_origin, vertex->_origin, delta);
            prevPtIt++;
        }

        foreach(LineDef *line, lines())
        {
            line->updateAABox();
        }

        V2d_Sum(origin, origin, delta);
        updateAABox();
    }
    P_PolyobjLink(this);

    // With translation applied now determine if we collided with anything.
    if(mobjIsBlockingPolyobj(*this))
    {
        //LOG_DEBUG("Blocked by mobj, undoing...");

        P_PolyobjUnlink(this);
        {
            povertex_t *prevPtIt = prevPts;
            foreach(Vertex *vertex, uniqueVertexes())
            {
                V2d_Copy(vertex->_origin, prevPtIt->origin);
                prevPtIt++;
            }

            foreach(LineDef *line, lines())
            {
                line->updateAABox();
            }

            V2d_Subtract(origin, origin, delta);
            updateAABox();
        }
        P_PolyobjLink(this);

        return false;
    }

    // Various parties may be interested in this change; signal it.
    notifyGeometryChanged(*this);

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

bool Polyobj::rotate(angle_t delta)
{
    LOG_AS("Polyobj::rotate");
    //LOG_DEBUG("Applying delta %u (%f) to [%p]")
    //    << delta << (delta / float( ANGLE_MAX ) * 360) << this;

    P_PolyobjUnlink(this);
    {
        uint fineAngle = (angle + delta) >> ANGLETOFINESHIFT;

        povertex_t *originalPtIt = originalPts;
        povertex_t *prevPtIt     = prevPts;
        foreach(Vertex *vertex, uniqueVertexes())
        {
            V2d_Copy(prevPtIt->origin, vertex->origin());
            V2d_Copy(vertex->_origin, originalPtIt->origin);

            rotatePoint2d(vertex->_origin, origin, fineAngle);

            originalPtIt++;
            prevPtIt++;
        }

        foreach(LineDef *line, lines())
        {
            line->updateAABox();
            line->updateSlopeType();
            line->_angle += ANGLE_TO_BANG(delta);

            // HEdge angle must be kept in sync.
            line->front().leftHEdge()._angle = BANG_TO_ANGLE(line->_angle);
        }
        updateAABox();
        angle += delta;
    }
    P_PolyobjLink(this);

    // With rotation applied now determine if we collided with anything.
    if(mobjIsBlockingPolyobj(*this))
    {
        //LOG_DEBUG("Blocked by mobj, undoing...");

        P_PolyobjUnlink(this);
        {
            povertex_t *prevPtIt = prevPts;
            foreach(Vertex *vertex, uniqueVertexes())
            {
                V2d_Copy(vertex->_origin, prevPtIt->origin);
                prevPtIt++;
            }

            foreach(LineDef *line, lines())
            {
                line->updateAABox();
                line->updateSlopeType();
                line->_angle -= ANGLE_TO_BANG(delta);

                // HEdge angle must be kept in sync.
                line->front().leftHEdge()._angle = BANG_TO_ANGLE(line->_angle);
            }
            updateAABox();
            angle -= delta;
        }
        P_PolyobjLink(this);

        return false;
    }

    updateSurfaceTangents();

    // Various parties may be interested in this change; signal it.
    notifyGeometryChanged(*this);

    return true;
}
