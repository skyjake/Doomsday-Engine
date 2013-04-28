/** @file glbuffer.cpp  GL vertex buffer.
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

#include "de/GLBuffer"

namespace de {

using namespace internal;
using namespace gl;

// Vertex Formats ------------------------------------------------------------

AttribSpec const Vertex2TexRgba::_spec[3] = {
    { AttribSpec::Position,  2, GL_FLOAT, false, sizeof(Vertex2TexRgba), 0 },
    { AttribSpec::TexCoord0, 2, GL_FLOAT, false, sizeof(Vertex2TexRgba), 2 * sizeof(float) },
    { AttribSpec::Color,     4, GL_FLOAT, false, sizeof(Vertex2TexRgba), 4 * sizeof(float) }
};

AttribSpecs Vertex2TexRgba::formatSpec() {
    DENG2_ASSERT(sizeof(Vertex2TexRgba) == 8 * sizeof(float)); // sanity check
    return AttribSpecs(_spec, sizeof(_spec)/sizeof(_spec[0]));
}

AttribSpec const Vertex3TexRgba::_spec[3] = {
    { AttribSpec::Position,  3, GL_FLOAT, false, sizeof(Vertex3TexRgba), 0 },
    { AttribSpec::TexCoord0, 2, GL_FLOAT, false, sizeof(Vertex3TexRgba), 3 * sizeof(float) },
    { AttribSpec::Color,     4, GL_FLOAT, false, sizeof(Vertex3TexRgba), 5 * sizeof(float) }
};

AttribSpecs Vertex3TexRgba::formatSpec() {
    DENG2_ASSERT(sizeof(Vertex3TexRgba) == 9 * sizeof(float)); // sanity check
    return AttribSpecs(_spec, sizeof(_spec)/sizeof(_spec[0]));
}

// ---------------------------------------------------------------------------

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

    void enableArrays(bool enable)
    {
        DENG2_ASSERT(specs.first != 0); // must have a spec

        for(duint i = 0; i < specs.second; ++i)
        {
            AttribSpec const &spec = specs.first[i];
            GLuint index = GLuint(spec.semantic);
            if(enable)
            {
                glEnableVertexAttribArray(index);
                glVertexAttribPointer(index, spec.size, spec.type, spec.normalized, spec.stride,
                                      (void const *) dintptr(spec.startOffset));
            }
            else
            {
                glDisableVertexAttribArray(index);
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

    if(data && dataSize && count)
    {
        d->alloc();

        glBindBuffer(GL_ARRAY_BUFFER, d->name);
        glBufferData(GL_ARRAY_BUFFER, dataSize, data, Instance::glUsage(usage));
        glBindBuffer(GL_ARRAY_BUFFER, 0);

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

void GLBuffer::draw(duint first, dint count)
{
    if(!isReady()) return;

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
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    }
    else
    {
        if(count < 0) count = d->count;
        if(first + count > d->count) count = d->count - first;

        DENG2_ASSERT(count >= 0);

        glDrawArrays(Instance::glPrimitive(d->prim), first, count);
    }

    d->enableArrays(false);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void GLBuffer::setFormat(AttribSpecs const &format)
{
    d->specs = format;
}

} // namespace de
