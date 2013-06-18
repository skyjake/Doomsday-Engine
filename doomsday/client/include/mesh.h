/** @file mesh.h Mesh Geometry Data Structure.
 *
 * @authors Copyright © 2008-2013 Daniel Swanson <danij@dengine.net>
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

#ifndef DENG_DATA_MESH_H
#define DENG_DATA_MESH_H

#include <QList>

#include <de/Vector>

#include "MapElement"

class Vertex;

namespace de {

class Face;
class HEdge;

/**
 * Two dimensioned mesh geometry data structure employing the half-edge model
 * (more formally known as "Doubly connected edge list" (DECL)).
 *
 * @see http://en.wikipedia.org/wiki/Doubly_connected_edge_list
 *
 * @ingroup data
 */
class Mesh
{
public:
    typedef QList<Vertex *> Vertexes;
    typedef QList<Face *> Faces;
    typedef QList<HEdge *> HEdges;

    /**
     * Base class for all elements of a mesh.
     */
    class Element
    {
        DENG2_NO_COPY  (Element)
        DENG2_NO_ASSIGN(Element)

    public:
        explicit Element(Mesh &mesh);

        virtual ~Element() {}

        /**
         * Returns the mesh the element is a part of.
         */
        Mesh &mesh() const;

        /**
         * Returns a pointer to the map element attributed to the mesh element.
         * May return @c 0 if not attributed.
         */
        MapElement *mapElement() const;

        /**
         * Change the map element to which the mesh element is attributed.
         *
         * @param newMapElement  MapElement to attribute to the mesh element.
         *                       Ownership is unaffected. Can be @c 0 (to
         *                       clear the attribution).
         *
         * @see mapElement()
         */
        void setMapElement(MapElement const *newMapElement);

    private:
        DENG2_PRIVATE(d)
    };

public:
    Mesh();

    /**
     * Clear the mesh destroying all geometry elements.
     */
    void clear();

    /**
     * Construct a new vertex.
     */
    Vertex *newVertex(Vector2d const &origin = de::Vector2d());

    /**
     * Construct a new half-edge.
     */
    HEdge *newHEdge(Vertex &vertex);

    /**
     * Construct a new face.
     */
    Face *newFace();

    /**
     * Remove the specified @a vertex from the mesh, destroying the vertex.
     * If @a vertex is not owned by the mesh then nothing will happen.
     */
    void removeVertex(Vertex &vertex);

    /**
     * Remove the specified @a hedge from the mesh, destroying the half-edge.
     * If @a hedge is not owned by the mesh then nothing will happen.
     */
    void removeHEdge(HEdge &hedge);

    /**
     * Remove the specified @a face from the mesh, destroying the face.
     * If @a face is not owned by the mesh then nothing will happen.
     */
    void removeFace(Face &face);

    /**
     * Returns the total number of vertexes in the mesh.
     */
    inline int vertexCount() const { return vertexes().count(); }

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
    inline bool vertexesIsEmpty() const { return vertexes().isEmpty(); }

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
    Vertexes const &vertexes() const;

    /**
     * Provides access to the set of all faces in the mesh.
     */
    Faces const &faces() const;

    /**
     * Provides access to the set of all half-edges in the mesh.
     */
    HEdges const &hedges() const;

private:
    DENG2_PRIVATE(d)
};

} // namespace de

#endif // DENG_DATA_MESH_H
