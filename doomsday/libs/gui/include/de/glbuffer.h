/** @file glbuffer.h  GL vertex buffer.
 *
 * @authors Copyright (c) 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include <de/libcore.h>
#include <de/list.h>
#include <de/vector.h>
#include <de/asset.h>
#include <utility>

#include "libgui.h"
#include "opengl.h"
#include "de/vertexbuilder.h"

namespace de {

namespace internal
{
    /// Describes an attribute array inside a GL buffer.
    struct AttribSpec
    {
        enum Semantic {
            Position,
            TexCoord,
            TexCoord0,
            TexCoord1,
            TexCoord2,
            TexCoord3,
            TexBounds,
            TexBounds0,
            TexBounds1,
            TexBounds2,
            TexBounds3,
            Color,
            Normal,
            Tangent,
            Bitangent,
            Intensity,
            Direction,
            Origin,
            BoneIDs,
            BoneWeights,
            InstanceMatrix, // x4
            InstanceColor,
            Index,
            Index0,
            Index1,
            Index2,
            Index3,
            Texture,
            Texture0,
            Texture1,
            Texture2,
            Texture3,
            Flags,

            MaxSemantics
        };

        /**
         * Returns the name of the attribute variable that matches a semantic.
         * @param semantic  Attribute data semantic.
         * @return Variable name.
         */
        static inline const char *semanticVariableName(Semantic semantic) {
            switch (semantic) {
                case Position:       return "aVertex";
                case TexCoord:       return "aUV";
                case TexCoord0:      return "aUV0";
                case TexCoord1:      return "aUV1";
                case TexCoord2:      return "aUV2";
                case TexCoord3:      return "aUV3";
                case TexBounds:      return "aBounds";
                case TexBounds0:     return "aBounds0";
                case TexBounds1:     return "aBounds1";
                case TexBounds2:     return "aBounds2";
                case TexBounds3:     return "aBounds3";
                case Color:          return "aColor";
                case Normal:         return "aNormal";
                case Tangent:        return "aTangent";
                case Bitangent:      return "aBitangent";
                case Intensity:      return "aIntensity";
                case Direction:      return "aDirection";
                case Origin:         return "aOrigin";
                case BoneIDs:        return "aBoneIDs";
                case BoneWeights:    return "aBoneWeights";
                case InstanceMatrix: return "aInstanceMatrix"; // x4
                case InstanceColor:  return "aInstanceColor";
                case Index:          return "aIndex";
                case Index0:         return "aIndex0";
                case Index1:         return "aIndex1";
                case Index2:         return "aIndex2";
                case Index3:         return "aIndex3";
                case Texture:        return "aTexture";
                case Texture0:       return "aTexture0";
                case Texture1:       return "aTexture1";
                case Texture2:       return "aTexture2";
                case Texture3:       return "aTexture3";
                case Flags:          return "aFlags";
                default:             break;
            }
            return "";
        }

        Semantic semantic;
        dint     size;        ///< Number of components in an element.
        GLenum   type;        ///< Data type.
        bool     normalized;  ///< Whether to normalize non-floats to [0.f, 1.f].
        dsize    stride;      ///< Number of bytes between elements.
        duint    startOffset; ///< Offset in bytes from the start of the buffer.
    };

    typedef std::pair<const AttribSpec *, dsize> AttribSpecs;
}

#define LIBGUI_DECLARE_VERTEX_FORMAT(NumElems) \
    public:  static de::internal::AttribSpecs formatSpec(); \
    private: static de::internal::AttribSpec const _spec[NumElems]; \
    public:

#define LIBGUI_VERTEX_FORMAT_SPEC(TypeName, ExpectedSize) \
    de::internal::AttribSpecs TypeName::formatSpec() { \
        DE_ASSERT(sizeof(TypeName) == ExpectedSize); /* sanity check */ \
        return de::internal::AttribSpecs(_spec, sizeof(_spec)/sizeof(_spec[0])); \
    }

/**
 * Vertex format with 2D coordinates and one set of texture coordinates.
 */
struct LIBGUI_PUBLIC Vertex2Tex
{
    Vec2f pos;
    Vec2f texCoord;

    LIBGUI_DECLARE_VERTEX_FORMAT(2)
};

/**
 * Vertex format with 2D coordinates and a color.
 */
struct LIBGUI_PUBLIC Vertex2Rgba
{
    Vec2f pos;
    Vec4f rgba;

    LIBGUI_DECLARE_VERTEX_FORMAT(2)
};

/**
 * Vertex format with 2D coordinates, one set of texture coordinates, and an
 * RGBA color.
 */
struct LIBGUI_PUBLIC Vertex2TexRgba
{
    Vec2f pos;
    Vec2f texCoord;
    Vec4f rgba;

    LIBGUI_DECLARE_VERTEX_FORMAT(3)
};

/**
 * Vertex format with just 3D coordinates.
 */
struct LIBGUI_PUBLIC Vertex3
{
    Vec3f pos;

    LIBGUI_DECLARE_VERTEX_FORMAT(1)
};

/**
 * Vertex format with 3D coordinates and one set of texture coordinates.
 */
struct LIBGUI_PUBLIC Vertex3Tex
{
    Vec3f pos;
    Vec2f texCoord;

    LIBGUI_DECLARE_VERTEX_FORMAT(2)
};

/**
 * Vertex format with 3D coordinates, one set of texture coordinates, and an
 * RGBA color.
 */
struct LIBGUI_PUBLIC Vertex3TexRgba
{
    Vec3f pos;
    Vec2f texCoord;
    Vec4f rgba;

    LIBGUI_DECLARE_VERTEX_FORMAT(3)
};

/**
 * Vertex format with 3D coordinates, one set of texture coordinates with indirect
 * bounds, and an RGBA color.
 */
struct LIBGUI_PUBLIC Vertex3TexBoundsRgba
{
    Vec3f pos;
    Vec2f texCoord;  ///< mapped using texBounds
    Vec4f texBounds; ///< UV space: x, y, width, height
    Vec4f rgba;

    LIBGUI_DECLARE_VERTEX_FORMAT(4)
};

/**
 * Vertex format with 3D coordinates, two sets of texture coordinates with indirect
 * bounds, and an RGBA color.
 */
struct LIBGUI_PUBLIC Vertex3Tex2BoundsRgba
{
    Vec3f pos;
    Vec2f texCoord[2];
    Vec4f texBounds;    ///< UV space: x, y, width, height
    Vec4f rgba;

    LIBGUI_DECLARE_VERTEX_FORMAT(5)
};

/**
 * Vertex format with 3D coordinates, two sets of texture coordinates, and an
 * RGBA color.
 */
struct LIBGUI_PUBLIC Vertex3Tex2Rgba
{
    Vec3f pos;
    Vec2f texCoord[2];
    Vec4f rgba;

    LIBGUI_DECLARE_VERTEX_FORMAT(4)
};

/**
 * Vertex format with 3D coordinates, three sets of texture coordinates, and an
 * RGBA color.
 */
struct LIBGUI_PUBLIC Vertex3Tex3Rgba
{
    Vec3f pos;
    Vec2f texCoord[3];
    Vec4f rgba;

    LIBGUI_DECLARE_VERTEX_FORMAT(5)
};

/**
 * Vertex format with 3D coordinates, normal vector, one set of texture
 * coordinates, and an RGBA color.
 */
struct LIBGUI_PUBLIC Vertex3NormalTexRgba
{
    Vec3f pos;
    Vec3f normal;
    Vec2f texCoord;
    Vec4f rgba;

    LIBGUI_DECLARE_VERTEX_FORMAT(4)
};

/**
 * Vertex format with 3D coordinates, normal/tangent/bitangent vectors, one set of
 * texture coordinates, and an RGBA color.
 */
struct LIBGUI_PUBLIC Vertex3NormalTangentTex
{
    Vec3f pos;
    Vec3f normal;
    Vec3f tangent;
    Vec3f bitangent;
    Vec2f texCoord;

    LIBGUI_DECLARE_VERTEX_FORMAT(5)
};

namespace gfx {

enum Usage {
    Static,      ///< modified once and used many times
    Dynamic,     ///< modified repeatedly and used many times
    Stream,      ///< modified once and used at most a few times
    StaticRead,  ///< read from GL, queried by app; modified once and used many times
    DynamicRead, ///< read from GL, queried by app; modified repeatedly and used many times
    StreamRead,  ///< read from GL, queried by app; modified once and used at most a few times
    StaticCopy,  ///< read from GL, used by GL; modified once and used many times
    DynamicCopy, ///< read from GL, used by GL; modified repeatedly and used many times
    StreamCopy,  ///< read from GL, used by GL; modified once and used at most a few times
};

enum Primitive { Points, LineStrip, LineLoop, Lines, TriangleStrip, TriangleFan, Triangles };

} // namespace gfx

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
    typedef List<Index> Indices;

    struct LIBGUI_PUBLIC DrawRanges {
        List<GLint>   first;
        List<GLsizei> count;

        inline void clear() {
            first.clear();
            count.clear();
        }

        inline void append(const Rangeui &range) {
            first << range.start;
            count << range.size();
        }

        inline void append(const Rangez &range) {
            first << GLint(range.start);
            count << GLsizei(range.size());
        }

        inline dsize size() const {
            DE_ASSERT(first.size() == count.size());
            return first.size();
        }
    };

    enum Type {
        VertexArray, // array buffer, or an array buffer with element array buffer
#if defined (DE_HAVE_TEXTURE_BUFFER)
        Texture,     // texture buffer
#endif
    };

public:
    GLBuffer(Type bufferType = VertexArray);

    void clear();

    void setVertices(dsize count, const void *data, dsize dataSize, gfx::Usage usage);

    void setVertices(gfx::Primitive primitive, dsize count, const void *data, dsize dataSize, gfx::Usage usage);

    void setIndices(gfx::Primitive primitive, dsize count, const Index *indices, gfx::Usage usage);

    void setIndices(gfx::Primitive primitive, const Indices &indices, gfx::Usage usage);

    void setData(const void *data, dsize dataSize, gfx::Usage usage);

    void setData(dsize startOffset, const void *data, dsize dataSize);

    void setUninitializedData(dsize dataSize, gfx::Usage usage);

    /**
     * Draws the buffer.
     *
     * Requires that a GLProgram is in use so that attribute locations can be determined.
     *
     * @param ranges  Range of vertices to draw. If nullptr, all vertices are drawn.
     */
    void draw(const DrawRanges *ranges = nullptr) const;

    void drawWithIndices(const GLBuffer &indexBuffer) const;

    void drawWithIndices(gfx::Primitive primitive, const Index *indices, dsize count) const;

    /**
     * Draws the buffer with instancing. One instance of the buffer is drawn per
     * each element in the provided @a instanceAttribs buffer.
     *
     * Requires that a GLProgram is in use so that attribute locations can be determined.
     * Also GL_ARB_instanced_arrays and GL_ARB_draw_instanced must be available.
     *
     * @param instanceAttribs  Buffer containing the attributes data for each instance.
     * @param first            First vertex in this buffer to start drawing from.
     * @param count            Number of vertices in this buffer to draw in each instance.
     */
    void drawInstanced(const GLBuffer &instanceAttribs, duint first = 0, dint count = -1) const;

    /**
     * Returns the number of vertices in the buffer.
     */
    dsize count() const;

    void setFormat(const internal::AttribSpecs &format);

    GLuint glName() const;

    static duint drawCount();
    static void resetDrawCount();

private:
    DE_PRIVATE(d)
};

/**
 * Template for a vertex buffer with a specific vertex format.
 */
template <typename VertexType>
class GLBufferT : public GLBuffer
{
public:
    typedef VertexType Type;
    typedef List<VertexType> Vertices;
    typedef typename VertexBuilder<VertexType>::Vertices Builder;

public:
    GLBufferT() {
        setFormat(VertexType::formatSpec());
    }

    void setVertices(const VertexType *vertices, dsize count, gfx::Usage usage) {
        GLBuffer::setVertices(count, vertices, sizeof(VertexType) * count, usage);
    }

    void setVertices(const Vertices &vertices, gfx::Usage usage) {
        GLBuffer::setVertices(vertices.size(), vertices.data(), sizeof(VertexType) * vertices.size(), usage);
    }

    void setVertices(gfx::Primitive primitive, const VertexType *vertices, dsize count, gfx::Usage usage) {
        GLBuffer::setVertices(primitive, count, vertices, sizeof(VertexType) * count, usage);
    }

    void setVertices(gfx::Primitive primitive, const Vertices &vertices, gfx::Usage usage) {
        GLBuffer::setVertices(primitive, vertices.size(), vertices.data(), sizeof(VertexType) * vertices.size(), usage);
    }
};

} // namespace de

#endif // LIBGUI_GLBUFFER_H
