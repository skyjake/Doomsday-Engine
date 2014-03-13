/** @file glbuffer.cpp  GL vertex buffer.
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

#include "de/GLBuffer"
#include "de/GLState"
#include "de/GLTarget"

namespace de {

using namespace internal;
using namespace gl;

// Vertex Format Layout ------------------------------------------------------

AttribSpec const Vertex2Tex::_spec[2] = {
    { AttribSpec::Position,  2, GL_FLOAT, false, sizeof(Vertex2Tex), 0 },
    { AttribSpec::TexCoord0, 2, GL_FLOAT, false, sizeof(Vertex2Tex), 2 * sizeof(float) }
};
LIBGUI_VERTEX_FORMAT_SPEC(Vertex2Tex, 4 * sizeof(float))

AttribSpec const Vertex2Rgba::_spec[2] = {
    { AttribSpec::Position,  2, GL_FLOAT, false, sizeof(Vertex2Rgba), 0 },
    { AttribSpec::Color,     4, GL_FLOAT, false, sizeof(Vertex2Rgba), 2 * sizeof(float) }
};
LIBGUI_VERTEX_FORMAT_SPEC(Vertex2Rgba, 6 * sizeof(float))

AttribSpec const Vertex2TexRgba::_spec[3] = {
    { AttribSpec::Position,  2, GL_FLOAT, false, sizeof(Vertex2TexRgba), 0 },
    { AttribSpec::TexCoord0, 2, GL_FLOAT, false, sizeof(Vertex2TexRgba), 2 * sizeof(float) },
    { AttribSpec::Color,     4, GL_FLOAT, false, sizeof(Vertex2TexRgba), 4 * sizeof(float) }
};
LIBGUI_VERTEX_FORMAT_SPEC(Vertex2TexRgba, 8 * sizeof(float))

AttribSpec const Vertex3Tex::_spec[2] = {
    { AttribSpec::Position,  3, GL_FLOAT, false, sizeof(Vertex3Tex), 0 },
    { AttribSpec::TexCoord0, 2, GL_FLOAT, false, sizeof(Vertex3Tex), 3 * sizeof(float) }
};
LIBGUI_VERTEX_FORMAT_SPEC(Vertex3Tex, 5 * sizeof(float))

AttribSpec const Vertex3TexRgba::_spec[3] = {
    { AttribSpec::Position,  3, GL_FLOAT, false, sizeof(Vertex3TexRgba), 0 },
    { AttribSpec::TexCoord0, 2, GL_FLOAT, false, sizeof(Vertex3TexRgba), 3 * sizeof(float) },
    { AttribSpec::Color,     4, GL_FLOAT, false, sizeof(Vertex3TexRgba), 5 * sizeof(float) }
};
LIBGUI_VERTEX_FORMAT_SPEC(Vertex3TexRgba, 9 * sizeof(float))

AttribSpec const Vertex3TexBoundsRgba::_spec[4] = {
    { AttribSpec::Position,   3, GL_FLOAT, false, sizeof(Vertex3TexBoundsRgba), 0 },
    { AttribSpec::TexCoord0,  2, GL_FLOAT, false, sizeof(Vertex3TexBoundsRgba), 3 * sizeof(float) },
    { AttribSpec::TexBounds0, 4, GL_FLOAT, false, sizeof(Vertex3TexBoundsRgba), 5 * sizeof(float) },
    { AttribSpec::Color,      4, GL_FLOAT, false, sizeof(Vertex3TexBoundsRgba), 9 * sizeof(float) }
};
LIBGUI_VERTEX_FORMAT_SPEC(Vertex3TexBoundsRgba, 13 * sizeof(float))

AttribSpec const Vertex3Tex2BoundsRgba::_spec[5] = {
    { AttribSpec::Position,   3, GL_FLOAT, false, sizeof(Vertex3Tex2BoundsRgba), 0 },
    { AttribSpec::TexCoord0,  2, GL_FLOAT, false, sizeof(Vertex3Tex2BoundsRgba), 3 * sizeof(float) },
    { AttribSpec::TexCoord1,  2, GL_FLOAT, false, sizeof(Vertex3Tex2BoundsRgba), 5 * sizeof(float) },
    { AttribSpec::TexBounds0, 4, GL_FLOAT, false, sizeof(Vertex3Tex2BoundsRgba), 7 * sizeof(float) },
    { AttribSpec::Color,      4, GL_FLOAT, false, sizeof(Vertex3Tex2BoundsRgba), 11 * sizeof(float) }
};
LIBGUI_VERTEX_FORMAT_SPEC(Vertex3Tex2BoundsRgba, 15 * sizeof(float))

AttribSpec const Vertex3Tex2Rgba::_spec[4] = {
    { AttribSpec::Position,  3, GL_FLOAT, false, sizeof(Vertex3Tex2Rgba), 0 },
    { AttribSpec::TexCoord0, 2, GL_FLOAT, false, sizeof(Vertex3Tex2Rgba), 3 * sizeof(float) },
    { AttribSpec::TexCoord1, 2, GL_FLOAT, false, sizeof(Vertex3Tex2Rgba), 5 * sizeof(float) },
    { AttribSpec::Color,     4, GL_FLOAT, false, sizeof(Vertex3Tex2Rgba), 7 * sizeof(float) }
};
LIBGUI_VERTEX_FORMAT_SPEC(Vertex3Tex2Rgba, 11 * sizeof(float))

AttribSpec const Vertex3Tex3Rgba::_spec[5] = {
    { AttribSpec::Position,  3, GL_FLOAT, false, sizeof(Vertex3Tex3Rgba), 0 },
    { AttribSpec::TexCoord0, 2, GL_FLOAT, false, sizeof(Vertex3Tex3Rgba), 3 * sizeof(float) },
    { AttribSpec::TexCoord1, 2, GL_FLOAT, false, sizeof(Vertex3Tex3Rgba), 5 * sizeof(float) },
    { AttribSpec::TexCoord2, 2, GL_FLOAT, false, sizeof(Vertex3Tex3Rgba), 7 * sizeof(float) },
    { AttribSpec::Color,     4, GL_FLOAT, false, sizeof(Vertex3Tex3Rgba), 9 * sizeof(float) }
};
LIBGUI_VERTEX_FORMAT_SPEC(Vertex3Tex3Rgba, 13 * sizeof(float))

AttribSpec const Vertex3NormalTexRgba::_spec[4] = {
    { AttribSpec::Position,  3, GL_FLOAT, false, sizeof(Vertex3NormalTexRgba), 0 },
    { AttribSpec::Normal,    3, GL_FLOAT, false, sizeof(Vertex3NormalTexRgba), 3 * sizeof(float) },
    { AttribSpec::TexCoord0, 2, GL_FLOAT, false, sizeof(Vertex3NormalTexRgba), 6 * sizeof(float) },
    { AttribSpec::Color,     4, GL_FLOAT, false, sizeof(Vertex3NormalTexRgba), 8 * sizeof(float) }
};
LIBGUI_VERTEX_FORMAT_SPEC(Vertex3NormalTexRgba, 12 * sizeof(float))

AttribSpec const Vertex3NormalTangentTex::_spec[5] = {
    { AttribSpec::Position,  3, GL_FLOAT, false, sizeof(Vertex3NormalTangentTex), 0 },
    { AttribSpec::Normal,    3, GL_FLOAT, false, sizeof(Vertex3NormalTangentTex), 3 * sizeof(float) },
    { AttribSpec::Tangent,   3, GL_FLOAT, false, sizeof(Vertex3NormalTangentTex), 6 * sizeof(float) },
    { AttribSpec::Bitangent, 3, GL_FLOAT, false, sizeof(Vertex3NormalTangentTex), 9 * sizeof(float) },
    { AttribSpec::TexCoord0, 2, GL_FLOAT, false, sizeof(Vertex3NormalTangentTex), 12 * sizeof(float) }
};
LIBGUI_VERTEX_FORMAT_SPEC(Vertex3NormalTangentTex, 14 * sizeof(float))

//----------------------------------------------------------------------------

DENG2_PIMPL(GLBuffer)
{
    GLuint name;
    GLuint idxName;
    dsize count;
    dsize idxCount;
    Primitive prim;
    AttribSpecs specs;

    Instance(Public *i) : Base(i), name(0), idxName(0), count(0), idxCount(0), prim(Points)
    {
        specs.first = 0;
        specs.second = 0;
    }

    ~Instance()
    {
        release();
        releaseIndices();
    }

    void alloc()
    {
        if(!name)
        {
            glGenBuffers(1, &name);
        }
    }

    void allocIndices()
    {
        if(!idxName)
        {
            glGenBuffers(1, &idxName);
        }
    }

    void release()
    {
        if(name)
        {
            glDeleteBuffers(1, &name);
            name = 0;
            count = 0;
        }
    }

    void releaseIndices()
    {
        if(idxName)
        {
            glDeleteBuffers(1, &idxName);
            idxName = 0;
            idxCount = 0;
        }
    }

    static GLenum glUsage(Usage u)
    {
        switch(u)
        {
        case Static:  return GL_STATIC_DRAW;
        case Dynamic: return GL_DYNAMIC_DRAW;
        case Stream:  return GL_STREAM_DRAW;
        }
        DENG2_ASSERT(false);
        return GL_STATIC_DRAW;
    }

    static GLenum glPrimitive(Primitive p)
    {
        switch(p)
        {
        case Points:        return GL_POINTS;
        case LineStrip:     return GL_LINE_STRIP;
        case LineLoop:      return GL_LINE_LOOP;
        case Lines:         return GL_LINES;
        case TriangleStrip: return GL_TRIANGLE_STRIP;
        case TriangleFan:   return GL_TRIANGLE_FAN;
        case Triangles:     return GL_TRIANGLES;
        }
        DENG2_ASSERT(false);
        return GL_TRIANGLES;
    }

    void enableArrays(bool enable) const
    {
        DENG2_ASSERT(specs.first != 0); // must have a spec

        for(duint i = 0; i < specs.second; ++i)
        {
            AttribSpec const &spec = specs.first[i];
            GLuint index = GLuint(spec.semantic);
            if(enable)
            {
                glEnableVertexAttribArray(index);
                LIBGUI_ASSERT_GL_OK();

                glVertexAttribPointer(index, spec.size, spec.type, spec.normalized, spec.stride,
                                      (void const *) dintptr(spec.startOffset));
                LIBGUI_ASSERT_GL_OK();
            }
            else
            {
                glDisableVertexAttribArray(index);
                LIBGUI_ASSERT_GL_OK();
            }
        }
    }
};

GLBuffer::GLBuffer() : d(new Instance(this))
{}

void GLBuffer::clear()
{
    setState(NotReady);
    d->release();
    d->releaseIndices();
}

void GLBuffer::setVertices(dsize count, void const *data, dsize dataSize, Usage usage)
{
    setVertices(Points, count, data, dataSize, usage);
}

void GLBuffer::setVertices(Primitive primitive, dsize count, void const *data, dsize dataSize, Usage usage)
{
    d->prim  = primitive;
    d->count = count;

    if(data)
    {
        d->alloc();

        if(dataSize && count)
        {
            glBindBuffer(GL_ARRAY_BUFFER, d->name);
            glBufferData(GL_ARRAY_BUFFER, dataSize, data, Instance::glUsage(usage));
            glBindBuffer(GL_ARRAY_BUFFER, 0);
        }

        setState(Ready);
    }
    else
    {
        d->release();

        setState(NotReady);
    }
}

void GLBuffer::setIndices(Primitive primitive, dsize count, Index const *indices, Usage usage)
{
    d->prim     = primitive;
    d->idxCount = count;

    if(indices && count)
    {
        d->allocIndices();

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, d->idxName);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, count * sizeof(Index), indices, Instance::glUsage(usage));
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    }
    else
    {
        d->releaseIndices();
    }
}

void GLBuffer::setIndices(Primitive primitive, Indices const &indices, Usage usage)
{
    setIndices(primitive, indices.size(), indices.constData(), usage);
}

void GLBuffer::draw(duint first, dint count) const
{
    if(!isReady()) return;

    // Mark the current target changed.
    GLState::current().target().markAsChanged();

    glBindBuffer(GL_ARRAY_BUFFER, d->name);
    d->enableArrays(true);

    if(d->idxName)
    {
        if(count < 0) count = d->idxCount;
        if(first + count > d->idxCount) count = d->idxCount - first;

        DENG2_ASSERT(count >= 0);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, d->idxName);
        glDrawElements(Instance::glPrimitive(d->prim), count, GL_UNSIGNED_SHORT,
                       (void const *) dintptr(first * 2));
        LIBGUI_ASSERT_GL_OK();
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    }
    else
    {
        if(count < 0) count = d->count;
        if(first + count > d->count) count = d->count - first;

        DENG2_ASSERT(count >= 0);

        glDrawArrays(Instance::glPrimitive(d->prim), first, count);
        LIBGUI_ASSERT_GL_OK();
    }

    d->enableArrays(false);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void GLBuffer::setFormat(AttribSpecs const &format)
{
    d->specs = format;
}

} // namespace de
