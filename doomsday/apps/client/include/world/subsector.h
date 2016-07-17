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

#include <QList>
#include <de/aabox.h>
#include <de/Observers>
#include <de/Vector>
#include "Sector"
#include "HEdge"

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

    virtual ~Subsector();

    DENG2_AS_IS_METHODS()

    /**
     * Returns the Sector attributed to the subsector.
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

//- Planes ------------------------------------------------------------------------------

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
