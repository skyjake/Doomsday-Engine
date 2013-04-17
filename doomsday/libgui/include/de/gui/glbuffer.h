/** @file glbuffer.h  GL vertex buffer.
 *
 * @authors Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#ifndef LIBGUI_GLBUFFER_H
#define LIBGUI_GLBUFFER_H

#include <de/libdeng2.h>
#include <de/Vector>
#include <de/Asset>
#include <vector>

#include "libgui.h"
#include "opengl.h"

namespace de {

namespace internal
{
    /// Describes an attribute array inside a GL buffer.
    struct AttribSpec
    {
        dint size;              ///< Number of components in an element.
        GLenum type;            ///< Data type.
        bool normalized;        ///< Whether to normalize non-floats to [0.f, 1.f].
        dsize stride;           ///< Number of bytes between elements.
        duint startOffset;      ///< Offset in bytes from the start of the buffer.
    };

    typedef std::pair<AttribSpec const *, duint> AttribSpecs;
}

/**
 * Vertex format with 2D coordinates, one set of texture coordinates, and an
 * RGBA color.
 */
struct LIBGUI_PUBLIC Vertex2TexRgba
{
    Vector2f pos;
    Vector2f texCoord;
    Vector4f rgba;

    static internal::AttribSpecs formatSpec();

private:
    static internal::AttribSpec const _spec[3];
};

/**
 * GL vertex buffer.
 *
 * Supports both indexed and non-indexed drawing. The primitive type has to be
 * specified either when setting the vertices (for non-indexed drawing) or when
 * specifying the indices (for indexed drawing).
 *
 * @note Compatible with OpenGL ES 2.0.
 *
 * @ingroup gl
 */
class LIBGUI_PUBLIC GLBuffer : public Asset
{
public:
    enum Usage
    {
        Static,
        Dynamic,
        Stream
    };

    enum Primitive
    {
        Points,
        LineStrip,
        LineLoop,
        Lines,
        TriangleStrip,
        TriangleFan,
        Triangles
    };

    typedef duint16 Index;

public:
    GLBuffer();

    void clear();

    void setVertices(dsize count, void const *data, dsize dataSize, Usage usage);

    void setVertices(Primitive primitive, dsize count, void const *data, dsize dataSize, Usage usage);

    void setIndices(Primitive primitive, dsize count, Index const *indices, Usage usage);

    void draw(duint first = 0, dint count = -1);

protected:
    void setFormat(internal::AttribSpecs const &format);

private:
    DENG2_PRIVATE(d)
};

/**
 * Template for a vertex buffer with a specific vertex format.
 */
template <typename VertexType>
class GLBufferT : public GLBuffer
{
public:
    typedef std::vector<VertexType> Vertices;

public:
    GLBufferT() {
        setFormat(VertexType::formatSpec());
    }

    void setVertices(VertexType const *vertices, dsize count, Usage usage) {
        GLBuffer::setVertices(count, vertices, sizeof(VertexType) * count, usage);
    }

    void setVertices(Vertices const &vertices, Usage usage) {
        GLBuffer::setVertices(vertices.size(), &vertices[0], sizeof(VertexType) * vertices.size(), usage);
    }

    void setVertices(Primitive primitive, VertexType const *vertices, dsize count, Usage usage) {
        GLBuffer::setVertices(primitive, count, vertices, sizeof(VertexType) * count, usage);
    }

    void setVertices(Primitive primitive, Vertices const &vertices, Usage usage) {
        GLBuffer::setVertices(primitive, vertices.size(), &vertices[0], sizeof(VertexType) * vertices.size(), usage);
    }
};

} // namespace de

#endif // LIBGUI_GLBUFFER_H
