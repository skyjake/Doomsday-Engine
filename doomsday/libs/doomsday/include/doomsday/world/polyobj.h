/** @file polyobj.h  Map polyobj.
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

#include "../mesh/mesh.h"
#include "dd_share.h"  // SoundEmitter

#include <de/list.h>
#include <de/error.h>
#include <de/vector.h>

namespace world
{
    class BspLeaf;
    class Line;
    class Map;
    class PolyobjData;
    class Sector;
    class Vertex;
}

/// Storage needed for a polyobj_s instance, plus the user data section (if any).
#define POLYOBJ_SIZE        gx.polyobjSize

/**
 * World polyobj. Moveable Polygonal Map-Object (Polyobj).
 */
typedef struct LIBDOOMSDAY_PUBLIC polyobj_s
{
public:
    /// The polyobj is not presently linked in the BSP. @ingroup errors
    DE_ERROR(NotLinkedError);

    static void setCollisionCallback(void (*func) (struct mobj_s *mobj, void *line, void *polyobj));

public:
    DD_BASE_POLYOBJ_ELEMENTS()

public:
    polyobj_s(const de::Vec2d &origin = de::Vec2d());

    /// @note: Does nothing about the user data section.
    ~polyobj_s();

    world::PolyobjData &data();
    const world::PolyobjData &data() const;

    /**
     * Returns the map in which the polyobj exists.
     */
    world::Map &map() const;

    /**
     * Provides access to the mesh owned by the polyobj.
     */
    mesh::Mesh &mesh() const;

    /**
     * Returns @c true if the polyobj is presently linked in the owning Map.
     */
    bool isLinked();

    /**
     * (Re)link the polyobj in the owning Map. Ownership is not affected. To be
     * called @em after rotation and/or translation to re-link the polyobj and
     * thereby complete the process.
     *
     * Linking only occurs if the polyobj is not presently linked (subsequent
     * calls are ignored).
     */
    void link();

    /**
     * Unlink the polyobj in the owning Map. To be called @em before attempting
     * to rotate and/or translate the polyobj to initiate the process.
     *
     * Unlinking only occurs if the polyobj is presently linked (subsequent
     * calls are ignored).
     */
    void unlink();

    /**
     * Returns @c true iff a BspLeaf is linked to the polyobj.
     */
    bool hasBspLeaf() const;

    /**
     * Returns the BSP leaf in which the polyobj is presently linked.
     *
     * @see isLinked();
     */
    world::BspLeaf &bspLeaf() const;

    /**
     * Convenience accessor which determines whether a BspLeaf with an attributed
     * sector is linked to the polyobj.
     *
     * @see hasBspLeaf(), BspLeaf::hasSector()
     */
    bool hasSector() const;

    /**
     * Convenience accessor which returns the Sector of the BspLeaf linked to the
     * polyobj.
     *
     * @see bspLeaf(), BspLeaf::sector()
     */
    world::Sector &sector() const;

    /**
     * Convenience accessor which returns a pointer to the Sector of the BspLeaf
     * linked to the polyobj.
     *
     * @return  Sector attributed to the linked BspLeaf; otherwise @c nullptr.
     *
     * @see hasBspLeaf(), BspLeaf::sectorPtr()
     */
    world::Sector *sectorPtr() const;

    /**
     * Returns the sound emitter for the polyobj.
     */
    SoundEmitter       &soundEmitter();
    const SoundEmitter &soundEmitter() const;

    /**
     * Provides access to the list of Lines for the polyobj.
     */
    de::List<world::Line *> const &lines() const;

    /**
     * Returns the total number of Lines for the polyobj.
     */
    inline de::duint lineCount() const { return lines().count(); }

    /**
     * To be called once all Lines have been added in order to compile the list
     * of unique vertexes for the polyobj. A vertex referenced by multiple lines
     * is only included once in this list.
     */
    void buildUniqueVertexes();

    /**
     * Provides access to the list of unique vertexes for the polyobj.
     *
     * @see buildUniqueVertex()
     */
    de::List<world::Vertex *> const &uniqueVertexes() const;

    /**
     * Returns the total number of unique Vertexes for the polyobj.
     *
     * @see buildUniqueVertexes()
     */
    inline de::duint uniqueVertexCount() const { return uniqueVertexes().count(); }

    /**
     * Update the original coordinates of all vertexes using the current coordinate
     * values. To be called once initialization has completed to finalize the polyobj.
     *
     * @pre Unique vertex list has already been built.
     *
     * @see buildUniqueVertexes()
     */
    void updateOriginalVertexCoords();

    /**
     * Translate the origin of the polyobj in the map coordinate space.
     *
     * @param delta  Movement delta on the X|Y plane.
     */
    bool move(const de::Vec2d &delta);

    /**
     * @overload
     */
    inline bool move(double x, double y) {
        return move(de::Vec2d(x, y));
    }

    /**
     * Rotate the angle of the polyobj in the map coordinate space.
     *
     * @param angle  World angle delta.
     */
    bool rotate(angle_t angle);

    /**
     * Update the axis-aligned bounding box for the polyobj (map coordinate
     * space) to encompass the points defined by it's vertices.
     *
     * @todo Should be private.
     */
    void updateBounds();

    /**
     * Update the tangent space vectors for all surfaces of the polyobj,
     * according to the points defined by the relevant Line's vertices.
     */
    void updateSurfaceTangents();

    /**
     * Change the tag associated with the polyobj.
     *
     * @param newTag  New tag.
     */
    void setTag(int newTag);

    /**
     * Change the associated sequence type of the polyobj.
     *
     * @param newType  New sequence type.
     */
    void setSequenceType(int newType);

    /**
     * Returns the original index of the polyobj.
     */
    int indexInMap() const;

    /**
     * Change the original index of the polyobj.
     *
     * @param newIndex  New original index.
     */
    void setIndexInMap(int newIndex);

private:
    /**
    * @param pob   Poly-object in collision.
    * @param mob   Map-object that @a line of the poly-object is in collision with.
    * @param line  Poly-object line that @a mob is in collision with.
    */
    static void NotifyCollision(struct polyobj_s &pob, struct mobj_s *mob, world::Line *line);

    bool blocked() const;

} Polyobj;
