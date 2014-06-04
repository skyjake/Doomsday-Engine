/** @file convexsubspace.h  World map convex subspace.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2014 Daniel Swanson <danij@dengine.net>
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

#ifndef DENG_WORLD_CONVEXSUBSPACE_H
#define DENG_WORLD_CONVEXSUBSPACE_H

#include "Mesh"
#include "MapElement"
#include "SectorCluster"
#include <QSet>
#include <de/Error>

class BspLeaf;
struct polyobj_s;
#ifdef __CLIENT__
class Subsector;
#endif

/**
 * @ingroup world
 */
class ConvexSubspace : public de::MapElement
{
public:
    /// An invalid polygon was specified. @ingroup errors
    DENG2_ERROR(InvalidPolyError);

    /// Required sector cluster attribution is missing. @ingroup errors
    DENG2_ERROR(MissingClusterError);

    /// Linked-element lists/sets:
    typedef QSet<de::Mesh *>  Meshes;
    typedef QSet<polyobj_s *> Polyobjs;

public:
    /**
     * Attempt to construct a ConvexSubspace from the Face geometry provided.
     * Before the geometry is accepted it is first conformance tested to ensure
     * that it is a simple convex polygon.
     *
     * @param poly  Polygon to construct from. Ownership is unaffected.
     */
    static ConvexSubspace *newFromConvexPoly(de::Face &poly, BspLeaf *bspLeaf = 0);

    /**
     * Returns the BspLeaf to which the subspace is assigned.
     */
    BspLeaf &bspLeaf() const;

    /**
     * Change the BspLeaf attributed to the subspace to @a newBspLeaf.
     */
    void setBspLeaf(BspLeaf *newBspLeaf);

    /**
     * Provides access to the attributed convex geometry (a polygon).
     */
    de::Face &poly() const;

    /**
     * Determines whether the specified @a point in the map coordinate space
     * lies inside the convex polygon geometry of the subspace on the XY plane.
     *
     * @param point  Map space point to test.
     *
     * @return  @c true iff the point lies inside the subspace geometry.
     *
     * @see http://www.alienryderflex.com/polygon/
     */
    bool contains(de::Vector2d const &point) const;

    /**
     * Assign an additional mesh geometry to the subspace. Such @em extra meshes
     * are used to represent geometry which would otherwise result in a
     * non-manifold mesh if incorporated in the primary mesh for the map.
     *
     * @param mesh  New mesh to be assigned to the subspace. Ownership of the
     *              mesh is given to ConvexSubspace.
     */
    void assignExtraMesh(de::Mesh &mesh);

    /**
     * Provides access to the set of 'extra' mesh geometries for the subspace.
     *
     * @see assignExtraMesh()
     */
    Meshes const &extraMeshes() const;

    /**
     * Remove the given @a polyobj from the set of those linked to the subspace.
     *
     * @return  @c true= @a polyobj was linked and subsequently removed.
     */
    bool unlink(polyobj_s const &polyobj);

    /**
     * Add the given @a polyobj to the set of those linked to the subspace.
     * Ownership is unaffected. If the polyobj is already linked in this set
     * then nothing will happen.
     */
    void link(struct polyobj_s const &polyobj);

    /**
     * Provides access to the set of polyobjs linked to the subspace.
     */
    Polyobjs const &polyobjs() const;

    /**
     * Convenient method of returning the total number of polyobjs linked to the
     * subspace.
     */
    inline int polyobjCount() { return polyobjs().count(); }

    /**
     * Returns the vector described by the offset from the map coordinate space
     * origin to the top most, left most point of the geometry of the subspace.
     *
     * @see aaBox()
     */
    de::Vector2d const &worldGridOffset() const;

    /**
     * Returns @c true iff a SectorCluster is attributed to the subspace. The
     * only time a cluster might not be attributed is during initial map setup.
     */
    bool hasCluster() const;

    /**
     * Change the sector cluster attributed to the subspace.
     *
     * @param newCluster New sector cluster to attribute to the subspace.
     *                   Ownership is unaffected. Can be @c 0 (to clear the
     *                   attribution).
     *
     * @see hasCluster(), cluster()
     */
    void setCluster(SectorCluster *newCluster);

    /**
     * Returns the SectorCluster attributed to the subspace.
     *
     * @see hasCluster()
     */
    SectorCluster &cluster() const;

    /**
     * Convenient method returning Sector of the SectorCluster attributed to the
     * subspace.
     *
     * @see cluster()
     */
    inline Sector &sector() const { return cluster().sector(); }

    /**
     * Convenient method returning a pointer to the SectorCluster attributed to
     * the subspace. If not attributed then @c 0 is returned.
     *
     * @see hasCluster(), cluster()
     */
    inline SectorCluster *clusterPtr() const {
        return hasCluster()? &cluster() : 0;
    }

    /**
     * Returns the @em validCount of the subspace. Used by some legacy iteration
     * algorithms for marking subspaces as processed/visited.
     *
     * @todo Refactor away.
     */
    int validCount() const;

    /// @todo Refactor away.
    void setValidCount(int newValidCount);

#ifdef __CLIENT__
    /**
     * Convenient method returning the Subsector associated with the subspace,
     * from the attributed SectorCluster.
     *
     * @see hasCluster(), SectorCluster::subsector()
     */
    inline Subsector &subsector() const {
        return cluster().subsector(*this);
    }
#endif

private:
    ConvexSubspace(de::Face &convexPolygon, BspLeaf *bspLeaf = 0);

    DENG2_PRIVATE(d)
};

#endif // DENG_WORLD_CONVEXSUBSPACE_H
