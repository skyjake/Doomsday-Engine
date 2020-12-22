/** @file convexsubspace.h  Map convex subspace.
 * @ingroup world
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

#pragma once

#include "mapelement.h"
#include "bspleaf.h"
#include "subsector.h"
#include "../mesh/mesh.h"

#include <de/error.h>
#include <de/vector.h>

namespace world {

class LIBDOOMSDAY_PUBLIC ConvexSubspace : public MapElement
{
public:
    /// An invalid polygon was specified. @ingroup errors
    DE_ERROR(InvalidPolyError);

    /// Required subsector attribution is missing. @ingroup errors
    DE_ERROR(MissingSubsectorError);

public:
    ConvexSubspace(mesh::Face &convexPolygon, BspLeaf *bspLeaf = nullptr);

    /**
     * Attempt to construct a ConvexSubspace from the Face geometry provided. Before the
     * geometry is accepted it is first conformance tested to ensure that it is a simple
     * convex polygon.
     *
     * @param poly  Polygon to construct from. Ownership is unaffected.
     */
    static ConvexSubspace *newFromConvexPoly(mesh::Face &poly, BspLeaf *bspLeaf = nullptr);

    /**
     * Determines whether the specified @a point in the map coordinate space lies inside
     * the convex polygon described by the geometry of the subspace on the XY plane.
     *
     * @param point  Map space point to test.
     *
     * @return  @c true iff the point lies inside the subspace geometry.
     *
     * @see http://www.alienryderflex.com/polygon/
     */
    bool contains(const de::Vec2d &point) const;

    /**
     * Provides access to the attributed convex geometry (a polygon).
     */
    mesh::Face &poly() const;

    /**
     * Assign an additional mesh geometry to the subspace. Such @em extra meshes are used
     * to represent geometry which would otherwise result in a non-manifold mesh if they
     * were incorporated in the primary mesh for the map.
     *
     * @param mesh  New mesh to be assigned to the subspace. Ownership of the mesh is given
     *              to ConvexSubspace.
     */
    void assignExtraMesh(mesh::Mesh &mesh);

    /**
     * Iterate through the 'extra' meshes of the subspace.
     *
     * @param func  Callback to make for each Mesh.
     */
    de::LoopResult forAllExtraMeshes(const std::function<de::LoopResult (mesh::Mesh &)>& func) const;

    /**
     * Returns @c true if a Subsector is attributed to the subspace. The only time that a
     * subsector might not be attributed is during initial map setup.
     */
    inline bool hasSubsector() const { return _subsector != nullptr; }

    /**
     * Returns the Subsector attributed to the subspace.
     *
     * @see hasSubsector()
     */
    inline Subsector &subsector() const { 
        DE_ASSERT(_subsector != nullptr); 
        return *_subsector; 
    }

    inline Subsector *subsectorPtr() const { 
        return _subsector; 
    }

    /**
     * Change the subsector attributed to the subspace.
     *
     * @param newSubsector New subsector to attribute to the subspace (Use @c nullptr
     *                   to clear). Ownership is unaffected.
     *
     * @see hasSubsector(), subsector()
     */
    void setSubsector(Subsector *newSubsector);

    /**
     * Convenient method returning Sector of the Subsector attributed to the subspace.
     *
     * @see subsector()
     */
    inline Sector &sector() const { return subsector().sector(); }

    /**
     * Returns the BspLeaf to which the subspace is assigned.
     */
    inline BspLeaf &bspLeaf() const {
        DE_ASSERT(_bspLeaf != nullptr);
        return *_bspLeaf;
    }

    void setBspLeaf(BspLeaf *newBspLeaf);

//- Poly objects ------------------------------------------------------------------------

    /**
     * Returns the total number of Polyobjs linked to the subspace.
     */
    int polyobjCount() const;

    /**
     * Iterate all Polyobjs linked in the subspace.
     *
     * @param callback  Call to make for each.
     */
    de::LoopResult forAllPolyobjs(const std::function<de::LoopResult (struct polyobj_s &)>& callback) const;

    /**
     * Remove the given @a polyobj from the set of those linked to the subspace.
     *
     * @return  @c true= @a polyobj was linked and subsequently removed.
     */
    bool unlink(const struct polyobj_s &polyobj);

    /**
     * Add the given @a polyobj to the set of those linked to the subspace. Ownership
     * is unaffected. If the polyobj is already linked in this set then nothing will
     * happen.
     */
    void link(const struct polyobj_s &polyobj);

    /**
     * Returns the @em validCount of the subspace. Used by some legacy algorithms to avoid
     * repeat processing.
     *
     * @todo Refactor away.
     */
    int validCount() const;

    void setValidCount(int newValidCount);

protected:
    DE_PRIVATE(d)

    // Heavily used; visible for inline access:
    Subsector *_subsector = nullptr;        ///< Attributed subsector (if any, not owned).
    BspLeaf *_bspLeaf = nullptr;            ///< Attributed BSP leaf (if any, not owned).
};

}  // namespace world
