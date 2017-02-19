/** @file glsubbuffer.h  Sub-range of a larger GLBuffer.
 *
 * @authors Copyright (c) 2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * LGPL: http://www.gnu.org/licenses/lgpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser
 * General Public License for more details. You should have received a copy of
 * the GNU Lesser General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#ifndef LIBGUI_GLSUBBUFFER_H
#define LIBGUI_GLSUBBUFFER_H

#include <de/GLBuffer>

namespace de {

/**
 * Sub-range of a larger GLBuffer.
 */
class GLSubBuffer
{
public:
    GLSubBuffer(Rangeui16 const &hostRange, GLBuffer &hostBuffer);

    /**
     * Returns the number of vertices currently being used. This may be smaller than
     * the total reserved size, but never larger.
     *
     * @return Number of vertices in use.
     */
    dsize size() const;

    void clear();

    void setVertices(dsize count, void const *data);

    void setBatchVertices(int batchIndex, dsize count, void *data);

    GLBuffer &hostBuffer() const;

    void setHostBuffer(GLBuffer &hostBuffer);

    /**
     * Sets the element range of the sub-buffer within the host buffer.
     *
     * @param range  Range of elements in the host buffer belonging to this sub-buffer.
     */
    void setHostRange(Rangeui16 const &range);

    Rangeui16 const &hostRange() const;

    template <typename VertexType>
    void setVertices(QVector<VertexType> const &vertices) {
        setVertices(sizeof(VertexType), vertices.size(), vertices.constData());
    }

    void setFormat(internal::AttribSpecs const &format);

private:
    DENG2_PRIVATE(d)
};

/**
 * Template for a vertex sub-buffer with a specific vertex format.
 */
/*template <typename VertexType>
class GLSubBufferT : public GLSubBuffer
{
public:
    typedef VertexType Type;
    typedef QVector<VertexType> Vertices;
    typedef typename VertexBuilder<VertexType>::Vertices Builder;

public:
    void setVertices(VertexType const *vertices, dsize count) {
        setFormat(VertexType::formatSpec());
        GLSubBuffer::setVertices(sizeof(VertexType), count, vertices);
    }

    void setVertices(Vertices const &vertices) {
        setFormat(VertexType::formatSpec());
        GLSubBuffer::setVertices(sizeof(VertexType), vertices.size(), vertices.constData());
    }
};
*/
} // namespace de

#endif // LIBGUI_GLSUBBUFFER_H
