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

#pragma once

#include "../mesh/hedge.h"
#include "sector.h"

#include <de/legacy/aabox.h>
#include <de/id.h>
#include <de/list.h>
#include <de/observers.h>
#include <de/vector.h>

namespace world {

class ConvexSubspace;

/**
 * Top level map geometry component describing a cluster of adjacent map subspaces (one
 * or more common edge) which are @em all attributed to the same Sector of the parent Map.
 * In other words, a Subsector can be thought of as an "island" of traversable map space
 * somewhere in the void.
 *
 * @attention Should not be confused with the (more granular) id Tech 1 component of the
 * same name (now ConvexSubspace).
 */
class LIBDOOMSDAY_PUBLIC Subsector
{
public:
    /// Notified when the subsector is about to be deleted.
    DE_DEFINE_AUDIENCE(Deletion, void subsectorBeingDeleted(const Subsector &subsector))

public:
    /**
     * Construct a new subsector comprised of the specified set of map subspace regions.
     * It is assumed that all the subspaces are attributed to the same Sector and there
     * is always at least one in the set.
     *
     * @param subspaces  Set of subspaces comprising the resulting subsector.
     */
    Subsector(const de::List<ConvexSubspace *> &subspaces);

    virtual ~Subsector();

    DE_CAST_METHODS()

    /**
     * Returns a humman-friendly, textual description of the subsector.
     */
    virtual de::String description() const;

    /**
     * Returns the automatically generated, unique identifier of the subsector.
     */
    de::Id id() const;

    /**
     * Returns the Sector attributed to the subsector.
     */
    Sector       &sector();
    const Sector &sector() const;

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
    static bool isInternalEdge(mesh::HEdge *hedge);

//- Subspaces ---------------------------------------------------------------------------

    /**
     * Returns the axis-aligned bounding box of the subsector.
     */
    const AABoxd &bounds() const;

    /**
     * Returns the point defined by the center of the axis-aligned bounding box in the
     * map coordinate space.
     */
    de::Vec2d center() const;

    /**
     * Returns a rough approximation of the total area of the geometries of all subspaces
     * in the subsector (map units squared).
     */
    double roughArea() const;

    /**
     * Returns the total number of subspaces in the subsector.
     */
    int subspaceCount() const;

    /**
     * Convenient method returning the first subspace in the subsector.
     */
    ConvexSubspace &firstSubspace() const;

    /**
     * Iterate ConvexSubspaces of the subsector.
     *
     * @param callback  Function to call for each ConvexSubspace.
     */
    de::LoopResult forAllSubspaces(const std::function<de::LoopResult (ConvexSubspace &)>& func) const;

    /**
     * Returns a list containing the first half-edge from each of the edge loops described
     * by the subspace geometry.
     */
    de::List<mesh::HEdge *> listUniqueBoundaryEdges() const;
    
private:
    DE_PRIVATE(d)
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
class LIBDOOMSDAY_PUBLIC SubsectorCirculator
{
public:
    /// Attempt to dereference a NULL circulator. @ingroup errors
    DE_ERROR(NullError);

public:
    /**
     * Construct a new subsector circulator.
     *
     * @param hedge  Half-edge to circulate. It is assumed the half-edge lies on the
     * @em boundary of the subsector and is not an "internal" edge.
     */
    SubsectorCirculator(mesh::HEdge *hedge = nullptr)
        : _hedge(hedge)
        , _current(hedge)
        , _subsec(hedge? hedge->subsector() : nullptr)
    {}

    /**
     * Intended as a convenient way to employ the specialized circulator logic to locate
     * the relative back of the next/previous neighboring half-edge. Particularly useful
     * when a geometry traversal requires a switch from the subsector to face boundary,
     * or when navigating the so-called "one-ring" of a vertex.
     */
    static mesh::HEdge &findBackNeighbor(const mesh::HEdge &hedge, de::ClockDirection direction)
    {
        return getNeighbor(hedge, direction, hedge.subsector()).twin();
    }

    /**
     * Returns the neighbor half-edge in the specified @a direction around the
     * boundary of the subsector.
     *
     * @param direction  Relative direction of the desired neighbor.
     */
    mesh::HEdge &neighbor(de::ClockDirection direction) {
        _current = &getNeighbor(*_current, direction, _subsec);
        return *_current;
    }

    /// Returns the next half-edge (clockwise) and advances the circulator.
    inline mesh::HEdge &next()     { return neighbor(de::Clockwise); }

    /// Returns the previous half-edge (CounterClockwise) and advances the circulator.
    inline mesh::HEdge &previous() { return neighbor(de::CounterClockwise); }

    /// Advance to the next half-edge (clockwise).
    inline SubsectorCirculator &operator ++ () {
        next(); return *this;
    }
    /// Advance to the previous half-edge (CounterClockwise).
    inline SubsectorCirculator &operator -- () {
        previous(); return *this;
    }

    /// Returns @c true iff @a other references the same half-edge as "this"
    /// circulator; otherwise returns false.
    inline bool operator == (const SubsectorCirculator &other) const {
        return _current == other._current;
    }
    inline bool operator != (const SubsectorCirculator &other) const {
        return !(*this == other);
    }

    /// Returns @c true iff the range of the circulator [c, c) is not empty.
    inline operator bool () const { return _hedge != nullptr; }

    /// Makes the circulator operate on @a hedge.
    SubsectorCirculator &operator = (mesh::HEdge &hedge) {
        _hedge   = _current = &hedge;
        _subsec = hedge.subsector();
        return *this;
    }

    /// Returns the current half-edge of a non-empty sequence.
    mesh::HEdge &operator * () const {
        if (!_current) {
            /// @throw NullError Attempted to dereference a "null" circulator.
            throw NullError("SubsectorCirculator::operator *", "Circulator references an empty sequence");
        }
        return *_current;
    }

    /// Returns a pointer to the current half-edge (might be @c nullptr, meaning the
    /// circulator references an empty sequence).
    mesh::HEdge *operator -> () { return _current; }

private:
    //static Subsector *getSubsector(const mesh::HEdge &hedge);

    static mesh::HEdge &getNeighbor(const mesh::HEdge &hedge, de::ClockDirection direction,
                                  const Subsector *subsector = nullptr);

    mesh::HEdge *_hedge;
    mesh::HEdge *_current;
    Subsector *_subsec;
};

}  // namespace world
