
/** @file subsector.h  Map subsector.
* @ingroup world
*
* @authors Copyright © 2013-2016 Daniel Swanson <danij@dengine.net>
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

#ifndef DENG_WORLD_SUBSECTOR_H
#define DENG_WORLD_SUBSECTOR_H

#ifdef __CLIENT__
#  include <QBitArray>
#endif
#include <QList>
#include <de/aabox.h>
#include <de/Observers>
#include <de/Vector>
#include <doomsday/world/mapelement.h>

#include "HEdge"

#ifdef __CLIENT__
#  include "world/audioenvironment.h"
#endif
#include "Line"
#include "Plane"
#include "Sector"

#ifdef __CLIENT__
#  include "render/lightgrid.h"
#endif

#ifdef __CLIENT__
class Shard;
#endif

namespace world {

/**
 * Top level map geometry component describing a cluster of adjacent map subspaces (one
 * or more common edge) which are @em all attributed to the same Sector of the parent Map.
 * In other words, a Subsector can be thought of as an "island" of traversable map space
 * somewhere in the void.
 *
 * @attention Should not be confused with the (more granular) id Tech 1 component of the
 * same name (now ConvexSubspace).
 */
class Subsector
#ifdef __CLIENT__
    : public de::LightGrid::IBlockLightSource
#endif
{
public:
    /// Notified when the subsector is about to be deleted.
    DENG2_DEFINE_AUDIENCE(Deletion, void subsectorBeingDeleted(Subsector const &subsector))

    /**
     * Determines whether the specified @a hedge is an "internal" edge:
     *
     * - both the half-edge and it's twin have a face.
     * - both faces are assigned to a subspace.
     * - both subspaces are in the same subsector.
     *
     * @param hedge  Half-edge to test.
     *
     * @return  @c true= @a hedge is a subsector-internal edge.
     */
    static bool isInternalEdge(de::HEdge *hedge);

public:
    /**
     * Construct a new subsector comprised of the specified set of map subspace regions.
     * It is assumed that all the subspaces are attributed to the same Sector and there
     * is always at least one in the set.
     *
     * @param subspaces  Set of subspaces comprising the resulting subsector.
     */
    Subsector(QList<ConvexSubspace *> const &subspaces);

    /**
     * Returns the attributed Sector of the subsector.
     */
    Sector       &sector();
    Sector const &sector() const;

    /**
     * Returns the axis-aligned bounding box of the subsector.
     */
    AABoxd const &aaBox() const;

    /**
     * Returns the point defined by the center of the axis-aligned bounding box in the
     * map coordinate space.
     */
    inline de::Vector2d center() const {
        return (de::Vector2d(aaBox().min) + de::Vector2d(aaBox().max)) / 2;
    }

    /**
     * Returns @c true if @a height (up-axis offset) lies above/below the ceiling/floor
     * height of the subsector.
     */
    bool isHeightInVoid(de::ddouble height) const;

#ifdef __CLIENT__
    /**
     * Determines whether the subsector has positive world volume, i.e., the height of
     * the floor is lower than that of the ceiling plane.
     *
     * @param useSmoothedHeights  @c true= use the @em smoothed plane heights instead of
     * the @em sharp heights.
     */
    bool hasWorldVolume(bool useSmoothedHeights = true) const;
#endif

//- Planes ------------------------------------------------------------------------------

    /**
     * Returns @c true if at least one of the @em visual Planes of the subsector is using
     * a sky-masked Material.
     *
     * @see Surface::hasSkyMaskedMaterial()
     */
    bool hasSkyMaskPlane() const;

    /**
     * Returns the @em physical Plane of the subsector associated with @a planeIndex.
     *
     * @see floor(), ceiling()
     */
    Plane       &plane(de::dint planeIndex);
    Plane const &plane(de::dint planeIndex) const;

    /**
     * Returns the @em physical floor Plane of the subsector.
     *
     * @see ceiling(), plane()
     */
    inline Plane       &floor()       { return plane(Sector::Floor); }
    inline Plane const &floor() const { return plane(Sector::Floor); }

    /**
     * Returns the @em physical ceiling Plane of the subsector.
     *
     * @see floor(), plane()
     */
    inline Plane       &ceiling()       { return plane(Sector::Ceiling); }
    inline Plane const &ceiling() const { return plane(Sector::Ceiling); }

    /**
     * Returns the @em visual Plane of the subsector associated with @a planeIndex.
     *
     * @see visFloor(), visCeiling()
     */
    Plane       &visPlane(de::dint planeIndex);
    Plane const &visPlane(de::dint planeIndex) const;

    /**
     * Returns the @em visual floor Plane of the subsector.
     *
     * @see ceiling(), plane()
     */
    inline Plane       &visFloor()       { return visPlane(Sector::Floor); }
    inline Plane const &visFloor() const { return visPlane(Sector::Floor); }

    /**
     * Returns the @em visual ceiling Plane of the subsector.
     *
     * @see floor(), plane()
     */
    inline Plane       &visCeiling()       { return visPlane(Sector::Ceiling); }
    inline Plane const &visCeiling() const { return visPlane(Sector::Ceiling); }

    /**
     * Returns the total number of @em visual planes in the subsector.
     */
    inline de::dint visPlaneCount() const { return sector().planeCount(); }

    /**
     * To be called to force re-evaluation of mapped visual planes. This is only necessary
     * when a surface material change occurs on a boundary line of the subsector.
     */
    void markVisPlanesDirty();

//- Subspaces ---------------------------------------------------------------------------

    /**
     * Returns the total number of subspaces in the subsector.
     */
    de::dint subspaceCount() const;

    /**
     * Iterate ConvexSubspaces of the subsector.
     *
     * @param callback  Function to call for each ConvexSubspace.
     */
    de::LoopResult forAllSubspaces(std::function<de::LoopResult (ConvexSubspace &)> func) const;

    /**
     * Returns a rough approximation of the total area of the geometries of all subspaces
     * in the subsector (map units squared).
     */
    de::ddouble roughArea() const;

#ifdef __CLIENT__
//- Audio environment -------------------------------------------------------------------

    /**
     * POD: Environmental audio parameters.
     */
    struct AudioEnvironment
    {
        de::dfloat volume  = 0;
        de::dfloat space   = 0;
        de::dfloat decay   = 0;
        de::dfloat damping = 0;

        void reset() { volume = space = decay = damping = 0; }
    };

    /**
     * Returns the environmental audio config for the subsector. Note that if a reverb
     * update is scheduled it will be done at this time (@ref markReverbDirty()).
     */
    AudioEnvironment const &reverb() const;

    /**
     * Request re-calculation of the environmental audio (reverb) characteristics of the
     * subsector (deferred until necessary).
     *
     * To be called whenever any of the properties governing reverb properties have changed
     * (i.e., wall/plane material changes).
     */
    void markReverbDirty(bool yes = true);

//- Bias lighting ----------------------------------------------------------------------

    /**
     * Apply bias lighting changes to @em all geometry Shards within the subsector.
     *
     * @param changes  Digest of lighting changes to be applied.
     */
    void applyBiasChanges(QBitArray &changes);

    /**
     * Convenient method of determining the frameCount of the current bias render
     * frame. Used for tracking changes to bias sources/surfaces.
     *
     * @see Map::biasLastChangeOnFrame()
     */
    de::duint biasLastChangeOnFrame() const;

    /**
     * Returns the geometry Shard for the specified @a mapElement and geometry
     * group identifier @a geomId; otherwise @c 0.
     */
    Shard *findShard(MapElement &mapElement, de::dint geomId);

    /**
     * Generate/locate the geometry Shard for the specified @a mapElement and
     * geometry group identifier @a geomId.
     */
    Shard &shard(MapElement &mapElement, de::dint geomId);

    /**
     * Shards owned by the Subsector should call this periodically to update
     * their bias lighting contributions.
     *
     * @param shard  Shard to be updated (owned by the Subsector).
     *
     * @return  @c true if one or more BiasIllum contributors was updated.
    */
    bool updateBiasContributors(Shard *shard);

//- Implements LightGrid::IBlockLightSource ---------------------------------------------

    /**
     * Returns the unique identifier of the light source.
     */
    LightId lightSourceId() const;

    /**
     * Returns the final ambient light color for the source (which, may be affected by the
     * sky light color if one or more Plane Surfaces in the subsector are using a sky-masked
     * Material).
     */
    de::Vector3f lightSourceColorf() const;

    /**
     * Returns the final ambient light intensity for the source.
     * @see lightSourceColorf()
     */
    de::dfloat lightSourceIntensity(de::Vector3d const &viewPoint = de::Vector3d(0, 0, 0)) const;

    /**
     * Returns the final ambient light color and intensity for the source.
     * @see lightSourceColorf()
     */
    inline de::Vector4f lightSourceColorfIntensity() {
        return de::Vector4f(lightSourceColorf(), lightSourceIntensity());
    }

    /**
     * Returns the Z-axis bias scale factor for the light grid, block light source.
     */
    de::dint blockLightSourceZBias();

#endif  // __CLIENT__

private:
    DENG2_PRIVATE(d)
};

/**
 * Subsector half-edge circulator. Used like an iterator, for circumnavigating the boundary
 * half-edges of a subsector.
 *
 * Subsector-internal edges (i.e., where both half-edge faces reference the same subsector)
 * are automatically skipped during traversal. Otherwise behavior is the same as a "regular"
 * half-edge face circulator.
 *
 * Also provides static search utilities for convenient, one-time use of this specialized
 * search logic (avoiding circulator instantiation).
 *
 * @ingroup world
 */
class SubsectorCirculator
{
public:
    /// Attempt to dereference a NULL circulator. @ingroup errors
    DENG2_ERROR(NullError);

public:
    /**
     * Construct a new subsector circulator.
     *
     * @param hedge  Half-edge to circulate. It is assumed the half-edge lies on the
     * @em boundary of the subsector and is not an "internal" edge.
     */
    SubsectorCirculator(de::HEdge *hedge = nullptr)
        : _hedge(hedge)
        , _current(hedge)
        , _subsec(hedge? getSubsector(*hedge) : nullptr)
    {}

    /**
     * Intended as a convenient way to employ the specialized circulator logic to locate
     * the relative back of the next/previous neighboring half-edge. Particularly useful
     * when a geometry traversal requires a switch from the subsector to face boundary,
     * or when navigating the so-called "one-ring" of a vertex.
     */
    static de::HEdge &findBackNeighbor(de::HEdge const &hedge, de::ClockDirection direction)
    {
        return getNeighbor(hedge, direction, getSubsector(hedge)).twin();
    }

    /**
     * Returns the neighbor half-edge in the specified @a direction around the
     * boundary of the subsector.
     *
     * @param direction  Relative direction of the desired neighbor.
     */
    de::HEdge &neighbor(de::ClockDirection direction) {
        _current = &getNeighbor(*_current, direction, _subsec);
        return *_current;
    }

    /// Returns the next half-edge (clockwise) and advances the circulator.
    inline de::HEdge &next()     { return neighbor(de::Clockwise); }

    /// Returns the previous half-edge (anticlockwise) and advances the circulator.
    inline de::HEdge &previous() { return neighbor(de::Anticlockwise); }

    /// Advance to the next half-edge (clockwise).
    inline SubsectorCirculator &operator ++ () {
        next(); return *this;
    }
    /// Advance to the previous half-edge (anticlockwise).
    inline SubsectorCirculator &operator -- () {
        previous(); return *this;
    }

    /// Returns @c true iff @a other references the same half-edge as "this"
    /// circulator; otherwise returns false.
    inline bool operator == (SubsectorCirculator const &other) const {
        return _current == other._current;
    }
    inline bool operator != (SubsectorCirculator const &other) const {
        return !(*this == other);
    }

    /// Returns @c true iff the range of the circulator [c, c) is not empty.
    inline operator bool () const { return _hedge != nullptr; }

    /// Makes the circulator operate on @a hedge.
    SubsectorCirculator &operator = (de::HEdge &hedge) {
        _hedge   = _current = &hedge;
        _subsec = getSubsector(hedge);
        return *this;
    }

    /// Returns the current half-edge of a non-empty sequence.
    de::HEdge &operator * () const {
        if (!_current)
        {
            /// @throw NullError Attempted to dereference a "null" circulator.
            throw NullError("SubsectorCirculator::operator *", "Circulator references an empty sequence");
        }
        return *_current;
    }

    /// Returns a pointer to the current half-edge (might be @c nullptr, meaning the
    /// circulator references an empty sequence).
    de::HEdge *operator -> () { return _current; }

private:
    static Subsector *getSubsector(de::HEdge const &hedge);

    static de::HEdge &getNeighbor(de::HEdge const &hedge, de::ClockDirection direction,
                                  Subsector const *subsector = nullptr);

    de::HEdge *_hedge;
    de::HEdge *_current;
    Subsector *_subsec;
};

}  // namespace world

#endif  // DENG_WORLD_SUBSECTOR_H
