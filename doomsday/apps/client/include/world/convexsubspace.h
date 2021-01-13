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

#ifndef DENG_WORLD_CONVEXSUBSPACE_H
#define DENG_WORLD_CONVEXSUBSPACE_H

#include <functional>
#include <de/Error>
#include <de/Vector>
#include <doomsday/world/MapElement>
#include "world/bspleaf.h"
#include "Subsector"
#ifdef __CLIENT__
#  include "Line"
#endif
#include "Mesh"

#ifdef __CLIENT__
class Lumobj;
#endif

namespace world {

#ifdef __CLIENT__
class AudioEnvironment;
#endif

/**
 * On client side a convex subspace also provides / links to various geometry data assets
 * and properties used to visualize the subspace.
 */
class ConvexSubspace : public MapElement
{
public:
    /// An invalid polygon was specified. @ingroup errors
    DENG2_ERROR(InvalidPolyError);

    /// Required BspLeaf attribution is missing. @ingroup errors
    //DENG2_ERROR(MissingBspLeafError);

    /// Required subsector attribution is missing. @ingroup errors
    DENG2_ERROR(MissingSubsectorError);

public:

    /**
     * Attempt to construct a ConvexSubspace from the Face geometry provided. Before the
     * geometry is accepted it is first conformance tested to ensure that it is a simple
     * convex polygon.
     *
     * @param poly  Polygon to construct from. Ownership is unaffected.
     */
    static ConvexSubspace *newFromConvexPoly(de::Face &poly, BspLeaf *bspLeaf = nullptr);

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
    bool contains(de::Vector2d const &point) const;

    /**
     * Provides access to the attributed convex geometry (a polygon).
     */
    de::Face &poly() const;

    /**
     * Assign an additional mesh geometry to the subspace. Such @em extra meshes are used
     * to represent geometry which would otherwise result in a non-manifold mesh if they
     * were incorporated in the primary mesh for the map.
     *
     * @param mesh  New mesh to be assigned to the subspace. Ownership of the mesh is given
     *              to ConvexSubspace.
     */
    void assignExtraMesh(de::Mesh &mesh);

    /**
     * Iterate through the 'extra' meshes of the subspace.
     *
     * @param func  Callback to make for each Mesh.
     */
    de::LoopResult forAllExtraMeshes(std::function<de::LoopResult (de::Mesh &)> func) const;

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
        DENG2_ASSERT(_subsector != nullptr); 
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
        DENG2_ASSERT(_bspLeaf != nullptr);
        return *_bspLeaf;
        // @throw MissingBspLeafError  Attempted with no BspLeaf attributed.
        //throw MissingBspLeafError("ConvexSubspace::bspLeaf", "No BSP leaf is attributed");
    }

    void setBspLeaf(BspLeaf *newBspLeaf);

//- Poly objects ------------------------------------------------------------------------

    /**
     * Returns the total number of Polyobjs linked to the subspace.
     */
    de::dint polyobjCount() const;

    /**
     * Iterate all Polyobjs linked in the subspace.
     *
     * @param callback  Call to make for each.
     */
    de::LoopResult forAllPolyobjs(std::function<de::LoopResult (struct polyobj_s &)> callback) const;

    /**
     * Remove the given @a polyobj from the set of those linked to the subspace.
     *
     * @return  @c true= @a polyobj was linked and subsequently removed.
     */
    bool unlink(struct polyobj_s const &polyobj);

    /**
     * Add the given @a polyobj to the set of those linked to the subspace. Ownership
     * is unaffected. If the polyobj is already linked in this set then nothing will
     * happen.
     */
    void link(struct polyobj_s const &polyobj);

#ifdef __CLIENT__
// --------------------------------------------------------------------------------------

    /**
     * Returns a pointer to the face geometry half-edge which has been chosen for use as
     * the base for a triangle fan GL primitive. May return @c nullptr if no suitable base
     * was determined.
     */
    de::HEdge *fanBase() const;

    /**
     * Returns the number of vertices needed for a triangle fan GL primitive.
     *
     * @note When first called after a face geometry is assigned a new 'base' half-edge for
     * the triangle fan primitive will be determined.
     *
     * @see fanBase()
     */
    de::dint fanVertexCount() const;

    /**
     * Returns the frame number of the last time mobj sprite projection was performed for
     * the subspace.
     */
    de::dint lastSpriteProjectFrame() const;
    void setLastSpriteProjectFrame(de::dint newFrameNumber);

//- Audio environment -------------------------------------------------------------------

    /**
     * Recalculate the environmental audio characteristics (reverb) of the subspace.
     */
    bool updateAudioEnvironment();

    /**
     * Provides access to the cached audio environment characteristics of the subspace for
     * efficient accumulation.
     */
    AudioEnvironment const &audioEnvironment() const;

//- Fake radio ---------------------------------------------------------------------------

    /**
     * Returns the total number of shadow line sides linked in the subspace.
     */
    de::dint shadowLineCount() const;

    /**
     * Clear the list of fake radio shadow line sides for the subspace.
     */
    void clearShadowLines();

    /**
     * Iterate through the set of fake radio shadow lines for the subspace.
     *
     * @param func  Callback to make for each LineSide.
     */
    de::LoopResult forAllShadowLines(std::function<de::LoopResult (LineSide &)> func) const;

    /**
     * Add the specified line @a side to the set of fake radio shadow lines for the
     * subspace. If the line is already present in this set then nothing will happen.
     *
     * @param side  Map line side to add to the set.
     */
    void addShadowLine(LineSide &side);

//- Luminous objects --------------------------------------------------------------------

    /**
     * Returns the total number of Lumobjs linked to the subspace.
     */
    de::dint lumobjCount() const;

    /**
     * Iterate all Lumobjs linked in the subspace.
     *
     * @param callback  Call to make for each.
     */
    de::LoopResult forAllLumobjs(std::function<de::LoopResult (Lumobj &)> callback) const;

    /**
     * Clear @em all linked Lumobj in the subspace.
     */
    void unlinkAllLumobjs();

    /**
     * Unlink @a lumobj from the subspace.
     *
     * @see link()
     */
    void unlink(Lumobj &lumobj);

    /**
     * Link @a lumobj in the subspace (if already linked nothing happens).
     *
     * @see unlink()
     */
    void link(Lumobj &lumobj);
#endif  // __CLIENT__

    /**
     * Returns the @em validCount of the subspace. Used by some legacy algorithms to avoid
     * repeat processing.
     *
     * @todo Refactor away.
     */
    de::dint validCount() const;
    void setValidCount(de::dint newValidCount);

private:
    ConvexSubspace(de::Face &convexPolygon, BspLeaf *bspLeaf = nullptr);

    DENG2_PRIVATE(d)

    // Heavily used; visible for inline access:
    Subsector *_subsector = nullptr;        ///< Attributed subsector (if any, not owned).
    BspLeaf *_bspLeaf = nullptr;            ///< Attributed BSP leaf (if any, not owned).
};

}  // namespace world

#endif  // DENG_WORLD_CONVEXSUBSPACE_H
