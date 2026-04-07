/** @file mesh.h  Mesh Geometry Data Structure.
 *
 * @authors Copyright © 2008-2015 Daniel Swanson <danij@dengine.net>
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

#include <de/error.h>
#include <de/list.h>
#include <de/vector.h>

#include "../world/mapelement.h"

namespace world
{
    class Vertex;
    class MapElement;
}

namespace mesh {

class Face;
class HEdge;

/**
 * Two dimensioned mesh geometry data structure employing the half-edge model, more formally
 * known as "Doubly connected edge list" (DCEL).
 *
 * @see http://en.wikipedia.org/wiki/Doubly_connected_edge_list
 *
 * @ingroup data
 */
class LIBDOOMSDAY_PUBLIC Mesh
{
public:
    typedef de::List<world::Vertex *> Vertices;
    typedef de::List<Face *>          Faces;
    typedef de::List<HEdge *>         HEdges;

    /**
     * Base class for all elements of a mesh.
     */
    class LIBDOOMSDAY_PUBLIC Element
    {
        DE_NO_COPY  (Element)
        DE_NO_ASSIGN(Element)

    public:
        /// Required map element is missing. @ingroup errors
        DE_ERROR(MissingMapElementError);

    public:
        explicit Element(Mesh &mesh);

        virtual ~Element() {}

        /**
         * Returns the mesh the element is a part of.
         */
        Mesh &mesh() const;

        /**
         * Returns @c true iff a map element is attributed.
         */
        bool hasMapElement() const;

        /**
         * Returns the map element attributed to the mesh element.
         *
         * @see hasMapElement()
         */
        world::MapElement       &mapElement();
        const world::MapElement &mapElement() const;

        template <class MapElementType>
        MapElementType &mapElementAs() {
            return mapElement().as<MapElementType>();
        }

        template <class MapElementType>
        const MapElementType &mapElementAs() const {
            return mapElement().as<MapElementType>();
        }

        /**
         * Change the map element to which the mesh element is attributed.
         *
         * @param newMapElement  MapElement to attribute to the mesh element. Ownership is
         *                       unaffected. Use @c nullptr (to clear the attribution).
         *
         * @see mapElement()
         */
        void setMapElement(world::MapElement *newMapElement);

    private:
        Mesh *             _owner      = nullptr;
        world::MapElement *_mapElement = nullptr; ///< Attributed MapElement (not owned).
    };

public:
    Mesh() = default;
    ~Mesh();

    /**
     * Clear the mesh destroying all geometry elements.
     */
    void clear();

    /**
     * Construct a new vertex.
     */
    world::Vertex *newVertex(const de::Vec2d &origin = {});

    /**
     * Construct a new half-edge.
     */
    HEdge *newHEdge(world::Vertex &vertex);

    /**
     * Construct a new face.
     */
    Face *newFace();

    /**
     * Remove the specified @a vertex from the mesh, destroying the vertex. If @a vertex is
     * not owned by the mesh then nothing will happen.
     */
    void removeVertex(world::Vertex &vertex);

    /**
     * Remove the specified @a hedge from the mesh, destroying the half-edge. If @a hedge is
     * not owned by the mesh then nothing will happen.
     */
    void removeHEdge(HEdge &hedge);

    /**
     * Remove the specified @a face from the mesh, destroying the face. If @a face is not owned
     * by the mesh then nothing will happen.
     */
    void removeFace(Face &face);

    /**
     * Returns the total number of vertexes in the mesh.
     */
    inline int vertexCount() const { return vertices().count(); }

    /**
     * Returns the total number of faces in the mesh.
     */
    inline int faceCount() const { return faces().count(); }

    /**
     * Returns the total number of half-edges in the mesh.
     */
    inline int hedgeCount() const { return hedges().count(); }

    /**
     * Returns @c true iff there are no vertexes in the mesh.
     */
    inline bool verticesIsEmpty() const { return vertices().isEmpty(); }

    /**
     * Returns @c true iff there are no faces in the mesh.
     */
    inline bool facesIsEmpty() const { return faces().isEmpty(); }

    /**
     * Returns @c true iff there are no half-edges in the mesh.
     */
    inline bool hedgesIsEmpty() const { return hedges().isEmpty(); }

    /**
     * Provides access to the set of all vertexes in the mesh.
     */
    const Vertices &vertices() const;

    /**
     * Provides access to the set of all faces in the mesh.
     */
    const Faces &faces() const;

    /**
     * Provides access to the set of all half-edges in the mesh.
     */
    const HEdges &hedges() const;

private:
    Vertices _vertices; // All vertices in the mesh.
    HEdges   _hedges;   // All half-edges in the mesh.
    Faces    _faces;    // All faces in the mesh.
};

typedef Mesh::Element MeshElement;

}  // namespace mesh
