/** @file polyobj.cpp World Map Polyobj.
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

#include <QSet>
#include <QVector>

#include <de/vector1.h>

#include "de_base.h"

#include "map/p_maptypes.h"
#include "map/p_polyobjs.h"
#include "render/r_main.h" // validCount
#include "HEdge"

#include "map/polyobj.h"

using namespace de;

/// Used to store the original/previous vertex coordinates.
typedef QVector<Vector2d> VertexCoords;

static void notifyGeometryChanged(Polyobj &po)
{
#ifdef __CLIENT__
    // Shadow bias must be informed when surfaces move/deform.
    foreach(Line *line, po.lines())
    {
        HEdge *hedge = line->front().leftHEdge();
        if(!hedge) continue;

        for(int i = 0; i < 3; ++i)
        {
            SB_SurfaceMoved(&hedge->biasSurfaceForGeometryGroup(i));
        }
    }
#else // !__CLIENT__
    DENG2_UNUSED(po);
#endif
}

polyobj_s::polyobj_s(de::Vector2d const &origin_)
{
    origin[VX] = origin_.x;
    origin[VY] = origin_.y;
    bspLeaf = 0;
    idx = 0;
    tag = 0;
    validCount = 0;
    dest[0] = dest[1] = 0;
    angle = destAngle = 0;
    angleSpeed = 0;
    _lines = new Lines;
    _uniqueVertexes = new Vertexes;
    _originalPts = new VertexCoords;
    _prevPts = new VertexCoords;
    speed = 0;
    crush = false;
    seqType = 0;
    _origIndex = 0;
}

polyobj_s::~polyobj_s()
{
    foreach(Line *line, lines())
    {
        delete line->front().leftHEdge();
    }

    delete static_cast<Lines *>(_lines);
    delete static_cast<Vertexes *>(_uniqueVertexes);
    delete static_cast<VertexCoords *>(_originalPts);
    delete static_cast<VertexCoords *>(_prevPts);
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
    QSet<Vertex *> vertexSet;
    foreach(Line *line, lines())
    {
        vertexSet.insert(&line->v1());
        vertexSet.insert(&line->v2());
    }

    Vertexes &uniqueVertexes = *static_cast<Vertexes *>(_uniqueVertexes);
    uniqueVertexes = vertexSet.toList();

    // Resize the coordinate vectors as they are implicitly linked to the unique vertexes.
    static_cast<VertexCoords *>(_originalPts)->resize(uniqueVertexes.count());
    static_cast<VertexCoords *>(_prevPts)->resize(uniqueVertexes.count());
}

void Polyobj::updateOriginalVertexCoords()
{
    VertexCoords::iterator origCoordsIt = static_cast<VertexCoords *>(_originalPts)->begin();
    foreach(Vertex *vertex, uniqueVertexes())
    {
        // The original coordinates are relative to the polyobj origin.
        (*origCoordsIt) = Vector2d(vertex->origin()) - Vector2d(origin);
        origCoordsIt++;
    }
}

void Polyobj::updateAABox()
{
    V2d_Set(aaBox.min, DDMAXFLOAT, DDMAXFLOAT);
    V2d_Set(aaBox.max, DDMINFLOAT, DDMINFLOAT);

    if(!lineCount()) return;

    QListIterator<Line *> lineIt(lines());

    Line *line = lineIt.next();
    V2d_CopyBox(aaBox.arvec2, line->aaBox().arvec2);

    while(lineIt.hasNext())
    {
        line = lineIt.next();
        V2d_UniteBox(aaBox.arvec2, line->aaBox().arvec2);
    }
}

void Polyobj::updateSurfaceTangents()
{
    foreach(Line *line, lines())
    {
        line->front().updateSurfaceNormals();
        line->back().updateSurfaceNormals();
    }
}

struct ptrmobjblockingparams_t
{
    bool isBlocked;
    Line *line;
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

    if(!mobjCanBlockMovement(mo))
        return false;

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

static bool checkMobjBlocking(Polyobj &po, Line &line)
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
    foreach(Line *line, po.lines())
    {
        if(checkMobjBlocking(po, *line))
            return true;
    }
    return false; // All clear.
}

bool Polyobj::move(Vector2d const &delta)
{
    LOG_AS("Polyobj::move");
    //LOG_DEBUG("Applying delta %s to [%p]") << delta.asText() << this;

    P_PolyobjUnlink(this);
    {
        VertexCoords::iterator prevCoordsIt = static_cast<VertexCoords *>(_prevPts)->begin();
        foreach(Vertex *vertex, uniqueVertexes())
        {
            // Remember the previous coords in case we need to undo.
            (*prevCoordsIt) = vertex->origin();

            // Apply translation.
            Vector2d newVertexOrigin = Vector2d(vertex->origin()) + delta;
            V2d_Set(vertex->_origin, newVertexOrigin.x, newVertexOrigin.y);

            prevCoordsIt++;
        }

        foreach(Line *line, lines())
        {
            line->updateAABox();
        }

        Vector2d newOrigin = Vector2d(origin) + delta;
        V2d_Set(origin, newOrigin.x, newOrigin.y);

        updateAABox();
    }
    P_PolyobjLink(this);

    // With translation applied now determine if we collided with anything.
    if(mobjIsBlockingPolyobj(*this))
    {
        //LOG_DEBUG("Blocked by mobj, undoing...");

        P_PolyobjUnlink(this);
        {
            VertexCoords::const_iterator prevCoordsIt = static_cast<VertexCoords *>(_prevPts)->constBegin();
            foreach(Vertex *vertex, uniqueVertexes())
            {
                V2d_Set(vertex->_origin, prevCoordsIt->x, prevCoordsIt->y);
                prevCoordsIt++;
            }

            foreach(Line *line, lines())
            {
                line->updateAABox();
            }

            Vector2d newOrigin = Vector2d(origin) - delta;
            V2d_Set(origin, newOrigin.x, newOrigin.y);

            updateAABox();
        }
        P_PolyobjLink(this);

        return false;
    }

    // Various parties may be interested in this change; signal it.
    notifyGeometryChanged(*this);

    return true;
}

/**
 * @param point      Point to be rotated (in-place).
 * @param about      Origin to rotate @a point relative to.
 * @param fineAngle  Angle to rotate (theta).
 */
static void rotatePoint2d(Vector2d &point, Vector2d const &origin, uint fineAngle)
{
    coord_t const c = FIX2DBL(fineCosine[fineAngle]);
    coord_t const s = FIX2DBL(finesine[fineAngle]);

    Vector2d orig = point;

    point.x = orig.x * c - orig.y * s + origin.x;
    point.y = orig.y * c + orig.x * s + origin.y;
}

bool Polyobj::rotate(angle_t delta)
{
    LOG_AS("Polyobj::rotate");
    //LOG_DEBUG("Applying delta %u (%f) to [%p]")
    //    << delta << (delta / float( ANGLE_MAX ) * 360) << this;

    P_PolyobjUnlink(this);
    {
        uint fineAngle = (angle + delta) >> ANGLETOFINESHIFT;

        VertexCoords::const_iterator origCoordsIt = static_cast<VertexCoords *>(_originalPts)->constBegin();
        VertexCoords::iterator prevCoordsIt = static_cast<VertexCoords *>(_prevPts)->begin();
        foreach(Vertex *vertex, uniqueVertexes())
        {
            // Remember the previous coords in case we need to undo.
            (*prevCoordsIt) = vertex->origin();

            // Apply rotation relative to the "original" coords.
            Vector2d newCoords = (*origCoordsIt);
            rotatePoint2d(newCoords, origin, fineAngle);

            V2d_Set(vertex->_origin, newCoords.x, newCoords.y);

            origCoordsIt++;
            prevCoordsIt++;
        }

        foreach(Line *line, lines())
        {
            line->updateAABox();
            line->updateSlopeType();

            // HEdge angle must be kept in sync.
            line->front().leftHEdge()->_angle = BANG_TO_ANGLE(line->angle());
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
            VertexCoords::const_iterator prevCoordsIt = static_cast<VertexCoords *>(_prevPts)->constBegin();
            foreach(Vertex *vertex, uniqueVertexes())
            {
                V2d_Set(vertex->_origin, prevCoordsIt->x, prevCoordsIt->y);
                prevCoordsIt++;
            }

            foreach(Line *line, lines())
            {
                line->updateAABox();
                line->updateSlopeType();

                // HEdge angle must be kept in sync.
                line->front().leftHEdge()->_angle = BANG_TO_ANGLE(line->angle());
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

void Polyobj::setTag(int newTag)
{
    tag = newTag;
}

void Polyobj::setSequenceType(int newType)
{
    seqType = newType;
}

uint Polyobj::origIndex() const
{
    return _origIndex;
}

void Polyobj::setOrigIndex(uint newIndex)
{
    _origIndex = newIndex;
}
