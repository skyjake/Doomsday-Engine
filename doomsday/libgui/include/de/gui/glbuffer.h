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

#include <QVector>

#include <de/libdeng2.h>
#include <de/Vector>
#include <de/Asset>
#include <utility>

#include "libgui.h"
#include "opengl.h"

namespace de {

namespace internal
{
    /// Describes an attribute array inside a GL buffer.
    struct AttribSpec
    {
        enum Semantic {
            Position,
            TexCoord0,
            TexCoord1,
            TexCoord2,
            TexCoord3,
            Color,
            Normal,
            Tangent,
            Bitangent
        };

        Semantic semantic;
        dint size;              ///< Number of components in an element.
        GLenum type;            ///< Data type.
        bool normalized;        ///< Whether to normalize non-floats to [0.f, 1.f].
        dsize stride;           ///< Number of bytes between elements.
        duint startOffset;      ///< Offset in bytes from the start of the buffer.
    };

    typedef std::pair<AttribSpec const *, duint> AttribSpecs;
}

#define DENG2_DECLARE_VERTEX_FORMAT(NumElems) \
    public:  static internal::AttribSpecs formatSpec(); \
    private: static internal::AttribSpec const _spec[NumElems]; \
    public:

/**
 * Vertex format with 2D coordinates and one set of texture coordinates.
 */
struct LIBGUI_PUBLIC Vertex2Tex
{
    Vector2f pos;
    Vector2f texCoord;

    DENG2_DECLARE_VERTEX_FORMAT(2)
};

/**
 * Vertex format with 2D coordinates and a color.
 */
struct LIBGUI_PUBLIC Vertex2Rgba
{
    Vector2f pos;
    Vector4f rgba;

    DENG2_DECLARE_VERTEX_FORMAT(2)
};

/**
 * Vertex format with 2D coordinates, one set of texture coordinates, and an
 * RGBA color.
 */
struct LIBGUI_PUBLIC Vertex2TexRgba
{
    Vector2f pos;
    Vector2f texCoord;
    Vector4f rgba;

    DENG2_DECLARE_VERTEX_FORMAT(3)
};

/**
 * Vertex format with 3D coordinates, one set of texture coordinates, and an
 * RGBA color.
 */
struct LIBGUI_PUBLIC Vertex3TexRgba
{
    Vector3f pos;
    Vector2f texCoord;
    Vector4f rgba;

    DENG2_DECLARE_VERTEX_FORMAT(3)
};

namespace gl
{
    enum Usage {
        Static,
        Dynamic,
        Stream
    };
    enum Primitive {
        Points,
        LineStrip,
        LineLoop,
        Lines,
        TriangleStrip,
        TriangleFan,
        Triangles
    };
}

/**
 * GL vertex buffer.
 *
 * Supports both indexed and non-indexed drawing. The primitive type has to be
 * specified either when setting the vertices (for non-indexed drawing) or when
 * specifying the indices (for indexed drawing).
 *
 * @note Compatible with OpenGL ES 2.0.
 *
 * @todo Add a method for replacing a portion of the existing data in the buffer
 * (using glBufferSubData).
 *
 * @ingroup gl
 */
class LIBGUI_PUBLIC GLBuffer : public Asset
{
public:
    typedef duint16 Index;
    typedef QVector<Index> Indices;

public:
    GLBuffer();

    void clear();

    void setVertices(dsize count, void const *data, dsize dataSize, gl::Usage usage);

    void setVertices(gl::Primitive primitive, dsize count, void const *data, dsize dataSize, gl::Usage usage);

    void setIndices(gl::Primitive primitive, dsize count, Index const *indices, gl::Usage usage);

    void setIndices(gl::Primitive primitive, Indices const &indices, gl::Usage usage);

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
    typedef VertexType Type;
    typedef QVector<VertexType> Vertices;

public:
    GLBufferT() {
        setFormat(VertexType::formatSpec());
    }

    void setVertices(VertexType const *vertices, dsize count, gl::Usage usage) {
        GLBuffer::setVertices(count, vertices, sizeof(VertexType) * count, usage);
    }

    void setVertices(Vertices const &vertices, gl::Usage usage) {
        GLBuffer::setVertices(vertices.size(), vertices.constData(), sizeof(VertexType) * vertices.size(), usage);
    }

    void setVertices(gl::Primitive primitive, VertexType const *vertices, dsize count, gl::Usage usage) {
        GLBuffer::setVertices(primitive, count, vertices, sizeof(VertexType) * count, usage);
    }

    void setVertices(gl::Primitive primitive, Vertices const &vertices, gl::Usage usage) {
        GLBuffer::setVertices(primitive, vertices.size(), vertices.constData(), sizeof(VertexType) * vertices.size(), usage);
    }

    static void concatenate(Vertices const &triStripSequence, Vertices &longTriStrip) {
        if(!triStripSequence.size()) return;
        if(!longTriStrip.isEmpty()) {
            longTriStrip << longTriStrip.back();
            longTriStrip << triStripSequence.front();
        }
        longTriStrip << triStripSequence;
    }
};

} // namespace de

#endif // LIBGUI_GLBUFFER_H
