/** @file glbuffer.h  GL vertex buffer.
 *
 * @authors Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBGUI_GLBUFFER_H
#define LIBGUI_GLBUFFER_H

#include <QVector>

#include <de/libdeng2.h>
#include <de/Vector>
#include <de/Asset>
#include <utility>

#include "libgui.h"
#include "opengl.h"
#include "../VertexBuilder"

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
            TexBounds0,
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

#define LIBGUI_DECLARE_VERTEX_FORMAT(NumElems) \
    public:  static internal::AttribSpecs formatSpec(); \
    private: static internal::AttribSpec const _spec[NumElems]; \
    public:

#define LIBGUI_VERTEX_FORMAT_SPEC(TypeName, ExpectedSize) \
    internal::AttribSpecs TypeName::formatSpec() { \
        DENG2_ASSERT(sizeof(TypeName) == ExpectedSize); /* sanity check */ \
        return internal::AttribSpecs(_spec, sizeof(_spec)/sizeof(_spec[0])); \
    }

/**
 * Vertex format with 2D coordinates and one set of texture coordinates.
 */
struct LIBGUI_PUBLIC Vertex2Tex
{
    Vector2f pos;
    Vector2f texCoord;

    LIBGUI_DECLARE_VERTEX_FORMAT(2)
};

/**
 * Vertex format with 2D coordinates and a color.
 */
struct LIBGUI_PUBLIC Vertex2Rgba
{
    Vector2f pos;
    Vector4f rgba;

    LIBGUI_DECLARE_VERTEX_FORMAT(2)
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

    LIBGUI_DECLARE_VERTEX_FORMAT(3)
};

/**
 * Vertex format with 3D coordinates and one set of texture coordinates.
 */
struct LIBGUI_PUBLIC Vertex3Tex
{
    Vector3f pos;
    Vector2f texCoord;

    LIBGUI_DECLARE_VERTEX_FORMAT(2)
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

    LIBGUI_DECLARE_VERTEX_FORMAT(3)
};

/**
 * Vertex format with 3D coordinates, one set of texture coordinates with indirect
 * bounds, and an RGBA color.
 */
struct LIBGUI_PUBLIC Vertex3TexBoundsRgba
{
    Vector3f pos;
    Vector2f texCoord;  ///< mapped using texBounds
    Vector4f texBounds; ///< UV space: x, y, width, height
    Vector4f rgba;

    LIBGUI_DECLARE_VERTEX_FORMAT(4)
};

/**
 * Vertex format with 3D coordinates, two sets of texture coordinates with indirect
 * bounds, and an RGBA color.
 */
struct LIBGUI_PUBLIC Vertex3Tex2BoundsRgba
{
    Vector3f pos;
    Vector2f texCoord[2];
    Vector4f texBounds;    ///< UV space: x, y, width, height
    Vector4f rgba;

    LIBGUI_DECLARE_VERTEX_FORMAT(5)
};

/**
 * Vertex format with 3D coordinates, two sets of texture coordinates, and an
 * RGBA color.
 */
struct LIBGUI_PUBLIC Vertex3Tex2Rgba
{
    Vector3f pos;
    Vector2f texCoord[2];
    Vector4f rgba;

    LIBGUI_DECLARE_VERTEX_FORMAT(4)
};

/**
 * Vertex format with 3D coordinates, three sets of texture coordinates, and an
 * RGBA color.
 */
struct LIBGUI_PUBLIC Vertex3Tex3Rgba
{
    Vector3f pos;
    Vector2f texCoord[3];
    Vector4f rgba;

    LIBGUI_DECLARE_VERTEX_FORMAT(5)
};

/**
 * Vertex format with 3D coordinates, normal vector, one set of texture
 * coordinates, and an RGBA color.
 */
struct LIBGUI_PUBLIC Vertex3NormalTexRgba
{
    Vector3f pos;
    Vector3f normal;
    Vector2f texCoord;
    Vector4f rgba;

    LIBGUI_DECLARE_VERTEX_FORMAT(4)
};

/**
 * Vertex format with 3D coordinates, normal/tangent/bitangent vectors, one set of
 * texture coordinates, and an RGBA color.
 */
struct LIBGUI_PUBLIC Vertex3NormalTangentTex
{
    Vector3f pos;
    Vector3f normal;
    Vector3f tangent;
    Vector3f bitangent;
    Vector2f texCoord;

    LIBGUI_DECLARE_VERTEX_FORMAT(5)
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

    void draw(duint first = 0, dint count = -1) const;

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
    typedef typename VertexBuilder<VertexType>::Vertices Builder;

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
};

} // namespace de

#endif // LIBGUI_GLBUFFER_H
