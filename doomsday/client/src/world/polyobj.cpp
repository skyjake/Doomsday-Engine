/** @file polyobj.h  World map polyobj.
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
#include "world/polyobj.h"

#include "world/worldsystem.h" // validCount
#include "world/map.h"
#include "world/p_object.h"
#include "BspLeaf"
#include "ConvexSubspace"
#ifdef __CLIENT__
#  include "SectorCluster"
#  include "Shard"
#endif

#ifdef __CLIENT__
#  include "render/rend_main.h" // useBias
#endif

#include <de/vector1.h>
#include <QSet>
#include <QVector>

using namespace de;

// Function to be called when the polyobj collides with some map element.
static void (*collisionCallback) (mobj_t *mobj, void *line, void *polyobj);

/// Used to store the original/previous vertex coordinates.
typedef QVector<Vector2d> VertexCoords;

static void notifyGeometryChanged(Polyobj &po)
{
#ifdef __CLIENT__
    if(!ddMapSetup && useBias)
    {
        // Shadow bias must be informed when surfaces move/deform.
        foreach(HEdge *hedge, po.mesh().hedges())
        {
            // Is this on the back of a one-sided line?
            if(!hedge->hasMapElement())
                continue;

            /// @note If polyobjs are allowed to move between sector clusters
            /// then we'll need to revise the bias illumination storage specially.
            if(Shard *shard = po.bspLeaf().subspace().cluster().findShard(hedge->mapElement(), LineSide::Middle))
            {
                shard->updateBiasAfterMove();
            }
        }
    }
#else // !__CLIENT__
    DENG2_UNUSED(po);
#endif
}

/**
 * @param po    Polyobj instance.
 * @param mobj  Mobj that @a line of the polyobj is in collision with.
 * @param line  Polyobj line that @a mobj is in collision with.
 */
static void notifyCollision(Polyobj &po, mobj_t *mobj, Line *line)
{
    if(collisionCallback)
    {
        collisionCallback(mobj, line, &po);
    }
}

polyobj_s::polyobj_s(de::Vector2d const &origin_)
{
    origin[VX] = origin_.x;
    origin[VY] = origin_.y;
    _bspLeaf = 0;
    tag = 0;
    validCount = 0;
    dest[0] = dest[1] = 0;
    angle = destAngle = 0;
    angleSpeed = 0;
    _mesh = new de::Mesh;
    _lines = new Lines;
    _uniqueVertexes = new Vertexes;
    _originalPts = new VertexCoords;
    _prevPts = new VertexCoords;
    speed = 0;
    crush = false;
    seqType = 0;
    _indexInMap = MapElement::NoIndex;
}

polyobj_s::~polyobj_s()
{
    delete static_cast<Lines *>(_lines);
    delete static_cast<Vertexes *>(_uniqueVertexes);
    delete static_cast<VertexCoords *>(_originalPts);
    delete static_cast<VertexCoords *>(_prevPts);
    delete static_cast<de::Mesh *>(_mesh);
}

void Polyobj::setCollisionCallback(void (*func) (mobj_t *mobj, void *line, void *polyobj)) // static
{
    collisionCallback = func;
}

Map &Polyobj::map() const
{
    /// @todo Do not assume the CURRENT map.
    return App_WorldSystem().map();
}

Mesh &Polyobj::mesh() const
{
    return *static_cast<de::Mesh *>(_mesh);
}

bool Polyobj::isLinked()
{
    return _bspLeaf != 0;
}

void Polyobj::unlink()
{
    if(_bspLeaf)
    {
        Map &map = _bspLeaf->map();

        if(_bspLeaf->hasSubspace())
        {
            _bspLeaf->subspace().unlink(*this);
        }
        _bspLeaf = 0;

        map.unlink(*this);
    }
}

void Polyobj::link()
{
    if(!_bspLeaf)
    {
        map().link(*this);

        // Find the center point of the polyobj.
        Vector2d avg;
        foreach(Line *line, lines())
        {
            avg += line->fromOrigin();
        }
        avg /= lineCount();

        // Given the center point determine in which BSP leaf the polyobj resides.
        _bspLeaf = &map().bspLeafAt(avg);
        if(_bspLeaf->hasSubspace())
        {
            _bspLeaf->subspace().link(*this);
        }
    }
}

bool Polyobj::hasBspLeaf() const
{
    return _bspLeaf != 0;
}

BspLeaf &Polyobj::bspLeaf() const
{
    if(_bspLeaf)
    {
        return *_bspLeaf;
    }
    /// @throw Polyobj::NotLinkedError Attempted while the polyobj is not linked to the BSP.
    throw Polyobj::NotLinkedError("Polyobj::bspLeaf", "Polyobj is not presently linked in the BSP");
}

bool Polyobj::hasSector() const
{
    return hasBspLeaf() && bspLeaf().hasSubspace();
}

Sector &Polyobj::sector() const
{
    return *bspLeaf().sectorPtr();
}

Sector *Polyobj::sectorPtr() const
{
    return hasBspLeaf()? bspLeaf().sectorPtr() : 0;
}

SoundEmitter &Polyobj::soundEmitter()
{
    return *reinterpret_cast<SoundEmitter *>(this);
}

SoundEmitter const &Polyobj::soundEmitter() const
{
    return const_cast<SoundEmitter const &>(const_cast<Polyobj &>(*this).soundEmitter());
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
        vertexSet.insert(&line->from());
        vertexSet.insert(&line->to());
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
        (*origCoordsIt) = vertex->origin() - Vector2d(origin);
        origCoordsIt++;
    }
}

void Polyobj::updateAABox()
{
    aaBox.clear();

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
    Polyobj *polyobj;
    Line *line; // Line of the polyobj which suffered collision.
    bool isBlocked;
};

static inline bool mobjCanBlockMovement(mobj_t *mo)
{
    DENG2_ASSERT(mo != 0);
    return (mo->ddFlags & DDMF_SOLID) || (mo->dPlayer && !(mo->dPlayer->flags & DDPF_CAMERA));
}

static int PTR_CheckMobjBlocking(mobj_t *mo, void *context)
{
    ptrmobjblockingparams_t *parms = (ptrmobjblockingparams_t *) context;

    if(!mobjCanBlockMovement(mo))
        return false;

    // Out of range?
    AABoxd moAABox = Mobj_AABox(*mo);
    if(moAABox.maxX <= parms->line->aaBox().minX ||
       moAABox.minX >= parms->line->aaBox().maxX ||
       moAABox.maxY <= parms->line->aaBox().minY ||
       moAABox.minY >= parms->line->aaBox().maxY)
        return false;

    if(parms->line->boxOnSide(moAABox))
        return false;

    // This mobj blocks our path!
    notifyCollision(*parms->polyobj, mo, parms->line);
    parms->isBlocked = true;

    // Process all blocking mobjs...
    return false;
}

static bool checkMobjBlocking(Polyobj &po, Line &line)
{
    ptrmobjblockingparams_t parms;
    parms.polyobj   = &po;
    parms.line      = &line;
    parms.isBlocked = false;

    AABoxd interceptRange(line.aaBox().minX - DDMOBJ_RADIUS_MAX,
                          line.aaBox().minY - DDMOBJ_RADIUS_MAX,
                          line.aaBox().maxX + DDMOBJ_RADIUS_MAX,
                          line.aaBox().maxY + DDMOBJ_RADIUS_MAX);

    validCount++;
    Mobj_BoxIterator(&interceptRange, PTR_CheckMobjBlocking, &parms);

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

    unlink();
    {
        VertexCoords::iterator prevCoordsIt = static_cast<VertexCoords *>(_prevPts)->begin();
        foreach(Vertex *vertex, uniqueVertexes())
        {
            // Remember the previous coords in case we need to undo.
            (*prevCoordsIt) = vertex->origin();

            // Apply translation.
            vertex->setOrigin(vertex->origin() + delta);

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
    link();

    // With translation applied now determine if we collided with anything.
    if(mobjIsBlockingPolyobj(*this))
    {
        //LOG_DEBUG("Blocked by mobj, undoing...");

        unlink();
        {
            VertexCoords::const_iterator prevCoordsIt = static_cast<VertexCoords *>(_prevPts)->constBegin();
            foreach(Vertex *vertex, uniqueVertexes())
            {
                vertex->setOrigin(*prevCoordsIt);
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
        link();

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
static void rotatePoint2d(Vector2d &point, Vector2d const &about, uint fineAngle)
{
    coord_t const c = FIX2DBL(fineCosine[fineAngle]);
    coord_t const s = FIX2DBL(finesine[fineAngle]);

    Vector2d orig = point;

    point.x = orig.x * c - orig.y * s + about.x;
    point.y = orig.y * c + orig.x * s + about.y;
}

bool Polyobj::rotate(angle_t delta)
{
    LOG_AS("Polyobj::rotate");
    //LOG_DEBUG("Applying delta %u (%f) to [%p]")
    //    << delta << (delta / float( ANGLE_MAX ) * 360) << this;

    unlink();
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
            vertex->setOrigin(newCoords);

            origCoordsIt++;
            prevCoordsIt++;
        }

        foreach(Line *line, lines())
        {
            line->updateAABox();
            line->updateSlopeType();
        }
        updateAABox();
        angle += delta;
    }
    link();

    // With rotation applied now determine if we collided with anything.
    if(mobjIsBlockingPolyobj(*this))
    {
        //LOG_DEBUG("Blocked by mobj, undoing...");

        unlink();
        {
            VertexCoords::const_iterator prevCoordsIt = static_cast<VertexCoords *>(_prevPts)->constBegin();
            foreach(Vertex *vertex, uniqueVertexes())
            {
                vertex->setOrigin(*prevCoordsIt);
                prevCoordsIt++;
            }

            foreach(Line *line, lines())
            {
                line->updateAABox();
                line->updateSlopeType();
            }
            updateAABox();
            angle -= delta;
        }
        link();

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

int Polyobj::indexInMap() const
{
    return _indexInMap;
}

void Polyobj::setIndexInMap(int newIndex)
{
    _indexInMap = newIndex;
}
