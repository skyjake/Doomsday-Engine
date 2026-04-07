/** @file polyobj.h  World map polyobj.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2016 Daniel Swanson <danij@dengine.net>
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

#include "doomsday/world/polyobj.h"
#include "doomsday/world/polyobjdata.h"
#include "doomsday/world/blockmap.h"
#include "doomsday/world/map.h"
#include "doomsday/world/mobj.h"
#include "doomsday/world/bspleaf.h"
#include "doomsday/world/world.h"
#include "doomsday/world/convexsubspace.h"
#include "doomsday/world/factory.h"
#include "doomsday/tab_tables.h"

#include <de/legacy/vector1.h>
#include <de/set.h>
#include <de/list.h>

using namespace de;

// Function to be called when the polyobj collides with some map element.
static void (*collisionCallback) (mobj_t *mob, void *line, void *pob);

static inline bool mobCanBlockMovement(const mobj_t &mob)
{
    return (mob.ddFlags & DDMF_SOLID) || (mob.dPlayer && !(mob.dPlayer->flags & DDPF_CAMERA));
}

void Polyobj::NotifyCollision(Polyobj &pob, mobj_t *mob, world::Line *line)  // static
{
    if (collisionCallback)
    {
        collisionCallback(mob, line, &pob);
    }
}

bool Polyobj::blocked() const
{
    for (const auto *line : lines())
    {
        const dint localValidCount = ++world::World::validCount;

        bool collision = false;
        map().mobjBlockmap().forAllInBox(AABoxd(line->bounds().minX - DDMOBJ_RADIUS_MAX,
                                                line->bounds().minY - DDMOBJ_RADIUS_MAX,
                                                line->bounds().maxX + DDMOBJ_RADIUS_MAX,
                                                line->bounds().maxY + DDMOBJ_RADIUS_MAX)
                            , [this, &line, &collision, &localValidCount](void *object)
        {
            auto &mob = *(mobj_t *)object;

            // Already processed?
            if (mob.validCount == localValidCount) return LoopContinue;
            mob.validCount = localValidCount;  // now processed.

            if (mobCanBlockMovement(mob))
            {
                // Out of range?
                AABoxd mobBox = Mobj_Bounds(mob);
                if (   mobBox.maxX <= line->bounds().minX
                    || mobBox.minX >= line->bounds().maxX
                    || mobBox.maxY <= line->bounds().minY
                    || mobBox.minY >= line->bounds().maxY)
                   return LoopContinue;

                if (!line->boxOnSide(mobBox))
                {
                    // This map-object blocks our path!
                    NotifyCollision(*const_cast<Polyobj *>(this), &mob, const_cast<world::Line *>(line));
                    collision = true;
                }
            }

            // Process all map-objects...
            return LoopContinue;
        });

        if (collision) return true;
    }

    return false;  // All clear.
}

polyobj_s::polyobj_s(const Vec2d &origin_)
{
    zap(thinker);

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
    thinker.d = world::Factory::newPolyobjData();
    THINKER_DATA(thinker, world::PolyobjData).setThinker(&thinker);
}

polyobj_s::~polyobj_s()
{
    delete THINKER_DATA_MAYBE(thinker, world::PolyobjData);
}

world::PolyobjData &polyobj_s::data()
{
    return THINKER_DATA(thinker, world::PolyobjData);
}

const world::PolyobjData &polyobj_s::data() const
{
    return THINKER_DATA(thinker, world::PolyobjData);
}

void Polyobj::setCollisionCallback(void (*func) (mobj_t *mob, void *line, void *polyob))  // static
{
    collisionCallback = func;
}

world::Map &Polyobj::map() const
{
    /// @todo Do not assume the CURRENT map.
    return world::World::get().map();
}

mesh::Mesh &Polyobj::mesh() const
{
    return *data().mesh;
}

bool Polyobj::isLinked()
{
    return hasBspLeaf();
}

void Polyobj::unlink()
{
    if (_bspLeaf)
    {
        if (((world::BspLeaf *)_bspLeaf)->hasSubspace())
        {
            ((world::BspLeaf *)_bspLeaf)->subspace().unlink(*this);
        }
        _bspLeaf = nullptr;

        map().unlink(*this);
    }
}

void Polyobj::link()
{
    if (!_bspLeaf)
    {
        map().link(*this);

        // Find the center point of the polyobj.
        Vec2d avg;
        for (world::Line *line : lines())
        {
            avg += line->from().origin();
        }
        avg /= lineCount();

        // Given the center point determine in which BSP leaf the polyobj resides.
        _bspLeaf = &map().bspLeafAt(avg);
        if (((world::BspLeaf *)_bspLeaf)->hasSubspace())
        {
            ((world::BspLeaf *)_bspLeaf)->subspace().link(*this);
        }
    }
}

bool Polyobj::hasBspLeaf() const
{
    return _bspLeaf != nullptr;
}

world::BspLeaf &Polyobj::bspLeaf() const
{
    if (hasBspLeaf()) return *(world::BspLeaf *)_bspLeaf;
    /// @throw Polyobj::NotLinkedError Attempted while the polyobj is not linked to the BSP.
    throw Polyobj::NotLinkedError("Polyobj::bspLeaf", "Polyobj is not presently linked in the BSP");
}

bool Polyobj::hasSector() const
{
    return hasBspLeaf() && bspLeaf().hasSubspace();
}

world::Sector &Polyobj::sector() const
{
    return *bspLeaf().sectorPtr();
}

world::Sector *Polyobj::sectorPtr() const
{
    return hasBspLeaf() ? bspLeaf().sectorPtr() : nullptr;
}

SoundEmitter &Polyobj::soundEmitter()
{
    return *reinterpret_cast<SoundEmitter *>(this);
}

const SoundEmitter &Polyobj::soundEmitter() const
{
    return const_cast<const SoundEmitter &>(const_cast<Polyobj &>(*this).soundEmitter());
}

List<world::Line *> const &Polyobj::lines() const
{
    return data().lines;
}

List<world::Vertex *> const &Polyobj::uniqueVertexes() const
{
    return data().uniqueVertexes;
}

void Polyobj::buildUniqueVertexes()
{
    Set<world::Vertex *> vertexSet;
    for (auto *line : lines())
    {
        vertexSet.insert(&line->from());
        vertexSet.insert(&line->to());
    }

    List<world::Vertex *> &uniqueVertexes = data().uniqueVertexes;
    uniqueVertexes = compose<List<world::Vertex *>>(vertexSet.begin(), vertexSet.end());

    // Resize the coordinate vectors as they are implicitly linked to the unique vertexes.
    data().originalPts.resize(uniqueVertexes.count());
    data().prevPts.resize(uniqueVertexes.count());
}

void Polyobj::updateOriginalVertexCoords()
{
    auto origCoordsIt = data().originalPts.begin();
    for (auto *vertex : uniqueVertexes())
    {
        // The original coordinates are relative to the polyobj origin.
        (*origCoordsIt) = vertex->origin() - Vec2d(origin);
        origCoordsIt++;
    }
}

void Polyobj::updateBounds()
{
    bounds.clear();

    if (lineCount() > 0)
    {
        auto i = lines().begin();
        V2d_CopyBox(bounds.arvec2, (*i)->bounds().arvec2);
        for (; i != lines().end(); ++i)
        {
            V2d_UniteBox(bounds.arvec2, (*i)->bounds().arvec2);
        }
    }
}

void Polyobj::updateSurfaceTangents()
{
    for (auto *line : lines())
    {
        line->forAllSides([] (world::LineSide &side)
        {
            side.updateAllSurfaceNormals();
            return LoopContinue;
        });
    }
}

bool Polyobj::move(const Vec2d &delta)
{
    using world::Vertex;

    LOG_AS("Polyobj::move");
    //LOG_DEBUG("Applying delta %s to [%p]") << delta.asText() << this;

    unlink();
    {
        auto prevCoordsIt = data().prevPts.begin();
        for (Vertex *vertex : uniqueVertexes())
        {
            // Remember the previous coords in case we need to undo.
            (*prevCoordsIt) = vertex->origin();

            // Apply translation.
            vertex->setOrigin(vertex->origin() + delta);

            prevCoordsIt++;
        }

        Vec2d newOrigin = Vec2d(origin) + delta;
        V2d_Set(origin, newOrigin.x, newOrigin.y);

        updateBounds();
    }
    link();

    // With translation applied now determine if we collided with anything.
    if (blocked())
    {
        //LOG_DEBUG("Blocked by some map object? Undoing...");

        unlink();
        {
            auto prevCoordsIt = data().prevPts.begin();
            for (Vertex *vertex : uniqueVertexes())
            {
                vertex->setOrigin(*prevCoordsIt);
                prevCoordsIt++;
            }

            Vec2d newOrigin = Vec2d(origin) - delta;
            V2d_Set(origin, newOrigin.x, newOrigin.y);

            updateBounds();
        }
        link();

        return false;
    }

#if 0
    // Various parties may be interested in this change; signal it.
    notifyGeometryChanged(*this);
#endif

    return true;
}

/**
 * @param point      Point to be rotated (in-place).
 * @param about      Origin to rotate @a point relative to.
 * @param fineAngle  Angle to rotate (theta).
 */
static void rotatePoint2d(Vec2d &point, const Vec2d &about, duint fineAngle)
{
    const ddouble c = FIX2DBL(finecosine[fineAngle]);
    const ddouble s = FIX2DBL(finesine[fineAngle]);

    Vec2d orig = point;

    point.x = orig.x * c - orig.y * s + about.x;
    point.y = orig.y * c + orig.x * s + about.y;
}

bool Polyobj::rotate(angle_t delta)
{
    using world::Vertex;

    LOG_AS("Polyobj::rotate");
    //LOG_DEBUG("Applying delta %u (%f) to [%p]")
    //    << delta << (delta / float( ANGLE_MAX ) * 360) << this;

    unlink();
    {
        duint fineAngle = (angle + delta) >> ANGLETOFINESHIFT;

        auto origCoordsIt = data().originalPts.begin();
        auto prevCoordsIt = data().prevPts.begin();

        for (Vertex *vertex : uniqueVertexes())
        {
            // Remember the previous coords in case we need to undo.
            (*prevCoordsIt) = vertex->origin();

            // Apply rotation relative to the "original" coords.
            Vec2d newCoords = (*origCoordsIt);
            rotatePoint2d(newCoords, Vec3d(origin), fineAngle);
            vertex->setOrigin(newCoords);

            origCoordsIt++;
            prevCoordsIt++;
        }

        updateBounds();
        angle += delta;
    }
    link();

    // With rotation applied now determine if we collided with anything.
    if (blocked())
    {
        //LOG_DEBUG("Blocked by some map object? Undoing...");

        unlink();
        {
            auto prevCoordsIt = data().prevPts.begin();
            for (Vertex *vertex : uniqueVertexes())
            {
                vertex->setOrigin(*prevCoordsIt);
                prevCoordsIt++;
            }
            updateBounds();
            angle -= delta;
        }
        link();

        return false;
    }

    updateSurfaceTangents();

#if 0
    // Various parties may be interested in this change; signal it.
    notifyGeometryChanged(*this);
#endif
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

void Polyobj::setIndexInMap(int newIndex)
{
    data().indexInMap = newIndex;
}
