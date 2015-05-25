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
#include "world/polyobjdata.h"

#include <QSet>
#include <QVector>
#include <de/vector1.h>

#include "dd_main.h"
#include "world/worldsystem.h"  // validCount
#include "world/blockmap.h"
#include "world/map.h"
#include "world/p_object.h"
#include "BspLeaf"
#include "ConvexSubspace"
#ifdef __CLIENT__
#  include "SectorCluster"
#  include "Shard"
#endif

#ifdef __CLIENT__
#  include "render/rend_main.h"  // useBias
#endif

using namespace de;

// Function to be called when the polyobj collides with some map element.
static void (*collisionCallback) (mobj_t *mobj, void *line, void *polyobj);

static inline bool mobCanBlockMovement(mobj_t const &mob)
{
    return (mob.ddFlags & DDMF_SOLID) || (mob.dPlayer && !(mob.dPlayer->flags & DDPF_CAMERA));
}

static void notifyGeometryChanged(Polyobj &pob)
{
#ifdef __CLIENT__
    if(!ddMapSetup && useBias)
    {
        // Shadow bias must be informed when surfaces move/deform.
        for(HEdge *hedge : pob.mesh().hedges())
        {
            // Is this on the back of a one-sided line?
            if(!hedge->hasMapElement())
                continue;

            /// @note If polyobjs are allowed to move between sector clusters
            /// then we'll need to revise the bias illumination storage specially.
            if(Shard *shard = pob.bspLeaf().subspace().cluster().findShard(hedge->mapElement(), LineSide::Middle))
            {
                shard->updateBiasAfterMove();
            }
        }
    }
#else // !__CLIENT__
    DENG2_UNUSED(pob);
#endif
}

void Polyobj::NotifyCollision(Polyobj &pob, mobj_t *mob, Line *line)  // static
{
    if(collisionCallback)
    {
        collisionCallback(mob, line, &pob);
    }
}

bool Polyobj::blocked() const
{
    for(Line const *line : lines())
    {
        dint const localValidCount = ++::validCount;

        bool collision = false;
        map().mobjBlockmap().forAllInBox(AABoxd(line->aaBox().minX - DDMOBJ_RADIUS_MAX,
                                                line->aaBox().minY - DDMOBJ_RADIUS_MAX,
                                                line->aaBox().maxX + DDMOBJ_RADIUS_MAX,
                                                line->aaBox().maxY + DDMOBJ_RADIUS_MAX)
                            , [this, &line, &collision, &localValidCount](void *object)
        {
            auto &mob = *(mobj_t *)object;

            // Already processed?
            if(mob.validCount == localValidCount) return LoopContinue;
            mob.validCount = localValidCount;  // now processed.

            if(mobCanBlockMovement(mob))
            {
                // Out of range?
                AABoxd moAABox = Mobj_AABox(mob);
                if(moAABox.maxX <= line->aaBox().minX ||
                   moAABox.minX >= line->aaBox().maxX ||
                   moAABox.maxY <= line->aaBox().minY ||
                   moAABox.minY >= line->aaBox().maxY)
                   return LoopContinue;

                if(!line->boxOnSide(moAABox))
                {
                    // This map-object blocks our path!
                    NotifyCollision(*const_cast<Polyobj *>(this), &mob, const_cast<Line *>(line));
                    collision = true;
                }
            }

            // Process all map-objects...
            return LoopContinue;
        });

        if(collision) return true;
    }

    return false;  // All clear.
}

polyobj_s::polyobj_s(Vector2d const &origin_)
    : thinker(thinker_s::InitializeToZero)
{
    origin[0] = origin_.x;
    origin[1] = origin_.y;
    tag = 0;
    validCount = 0;
    dest[0] = dest[1] = 0;
    angle = destAngle = 0;
    angleSpeed = 0;
    speed = 0;
    crush = false;
    seqType = 0;
    _bspLeaf = 0;

    // Allocate private data.
    thinker.d = new PolyobjData;
    THINKER_DATA(thinker, PolyobjData).setThinker(&thinker);
}

polyobj_s::~polyobj_s()
{
    delete THINKER_DATA_MAYBE(thinker, PolyobjData);
}

PolyobjData &polyobj_s::data()
{
    return THINKER_DATA(thinker, PolyobjData);
}

PolyobjData const &polyobj_s::data() const
{
    return THINKER_DATA(thinker, PolyobjData);
}

void Polyobj::setCollisionCallback(void (*func) (mobj_t *mob, void *line, void *polyob))  // static
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
    return *data().mesh;
}

bool Polyobj::isLinked()
{
    return hasBspLeaf();
}

void Polyobj::unlink()
{
    if(_bspLeaf)
    {
        if(_bspLeaf->hasSubspace())
        {
            _bspLeaf->subspace().unlink(*this);
        }
        _bspLeaf = nullptr;

        map().unlink(*this);
    }
}

void Polyobj::link()
{
    if(!_bspLeaf)
    {
        map().link(*this);

        // Find the center point of the polyobj.
        Vector2d avg;
        for(Line *line : lines())
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
    return _bspLeaf != nullptr;
}

BspLeaf &Polyobj::bspLeaf() const
{
    if(hasBspLeaf()) return *_bspLeaf;
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
    return hasBspLeaf()? bspLeaf().sectorPtr() : nullptr;
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
    return data().lines;
}

Polyobj::Vertexes const &Polyobj::uniqueVertexes() const
{
    return data().uniqueVertexes;
}

void Polyobj::buildUniqueVertexes()
{
    QSet<Vertex *> vertexSet;
    for(Line *line : lines())
    {
        vertexSet.insert(&line->from());
        vertexSet.insert(&line->to());
    }

    Vertexes &uniqueVertexes = data().uniqueVertexes;
    uniqueVertexes = vertexSet.toList();

    // Resize the coordinate vectors as they are implicitly linked to the unique vertexes.
    data().originalPts.resize(uniqueVertexes.count());
    data().prevPts.resize(uniqueVertexes.count());
}

void Polyobj::updateOriginalVertexCoords()
{
    PolyobjData::VertexCoords::iterator origCoordsIt = data().originalPts.begin();
    for(Vertex *vertex : uniqueVertexes())
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
    for(Line *line : lines())
    {
        line->front().updateSurfaceNormals();
        line->back().updateSurfaceNormals();
    }
}

bool Polyobj::move(Vector2d const &delta)
{
    LOG_AS("Polyobj::move");
    //LOG_DEBUG("Applying delta %s to [%p]") << delta.asText() << this;

    unlink();
    {
        PolyobjData::VertexCoords::iterator prevCoordsIt = data().prevPts.begin();
        for(Vertex *vertex : uniqueVertexes())
        {
            // Remember the previous coords in case we need to undo.
            (*prevCoordsIt) = vertex->origin();

            // Apply translation.
            vertex->setOrigin(vertex->origin() + delta);

            prevCoordsIt++;
        }

        for(Line *line : lines())
        {
            line->updateAABox();
        }

        Vector2d newOrigin = Vector2d(origin) + delta;
        V2d_Set(origin, newOrigin.x, newOrigin.y);

        updateAABox();
    }
    link();

    // With translation applied now determine if we collided with anything.
    if(blocked())
    {
        //LOG_DEBUG("Blocked by some map object? Undoing...");

        unlink();
        {
            PolyobjData::VertexCoords::const_iterator prevCoordsIt = data().prevPts.constBegin();
            for(Vertex *vertex : uniqueVertexes())
            {
                vertex->setOrigin(*prevCoordsIt);
                prevCoordsIt++;
            }

            for(Line *line : lines())
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
static void rotatePoint2d(Vector2d &point, Vector2d const &about, duint fineAngle)
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
        duint fineAngle = (angle + delta) >> ANGLETOFINESHIFT;

        PolyobjData::VertexCoords::const_iterator origCoordsIt = data().originalPts.constBegin();
        PolyobjData::VertexCoords::iterator prevCoordsIt = data().prevPts.begin();
        for(Vertex *vertex : uniqueVertexes())
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

        for(Line *line : lines())
        {
            line->updateAABox();
            line->updateSlopeType();
        }
        updateAABox();
        angle += delta;
    }
    link();

    // With rotation applied now determine if we collided with anything.
    if(blocked())
    {
        //LOG_DEBUG("Blocked by some map object? Undoing...");

        unlink();
        {
            PolyobjData::VertexCoords::const_iterator prevCoordsIt = data().prevPts.constBegin();
            for(Vertex *vertex : uniqueVertexes())
            {
                vertex->setOrigin(*prevCoordsIt);
                prevCoordsIt++;
            }

            for(Line *line : lines())
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

void Polyobj::setTag(dint newTag)
{
    tag = newTag;
}

void Polyobj::setSequenceType(dint newType)
{
    seqType = newType;
}

dint Polyobj::indexInMap() const
{
    return data().indexInMap;
}

void Polyobj::setIndexInMap(dint newIndex)
{
    data().indexInMap = newIndex;
}
