/** @file glbuffer.cpp  GL vertex buffer.
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

#include "de/GLBuffer"
#include "de/GLState"
#include "de/GLFramebuffer"
#include "de/GLProgram"
#include "de/GLInfo"

namespace de {

#ifdef DENG2_DEBUG
extern int GLDrawQueue_queuedElems;
#endif

using namespace internal;
using namespace gl;

// Vertex Format Layout ------------------------------------------------------

AttribSpec const Vertex2Tex::_spec[2] = {
    { AttribSpec::Position,  2, GL_FLOAT, false, sizeof(Vertex2Tex), 0 },
    { AttribSpec::TexCoord,  2, GL_FLOAT, false, sizeof(Vertex2Tex), 2 * sizeof(float) }
};
LIBGUI_VERTEX_FORMAT_SPEC(Vertex2Tex, 4 * sizeof(float))

AttribSpec const Vertex2Rgba::_spec[2] = {
    { AttribSpec::Position, 2, GL_FLOAT, false, sizeof(Vertex2Rgba), 0 },
    { AttribSpec::Color,    4, GL_FLOAT, false, sizeof(Vertex2Rgba), 2 * sizeof(float) }
};
LIBGUI_VERTEX_FORMAT_SPEC(Vertex2Rgba, 6 * sizeof(float))

AttribSpec const Vertex2TexRgba::_spec[3] = {
    { AttribSpec::Position,  2, GL_FLOAT, false, sizeof(Vertex2TexRgba), 0 },
    { AttribSpec::TexCoord,  2, GL_FLOAT, false, sizeof(Vertex2TexRgba), 2 * sizeof(float) },
    { AttribSpec::Color,     4, GL_FLOAT, false, sizeof(Vertex2TexRgba), 4 * sizeof(float) }
};
LIBGUI_VERTEX_FORMAT_SPEC(Vertex2TexRgba, 8 * sizeof(float))

AttribSpec const Vertex3::_spec[1] = {
    { AttribSpec::Position, 3, GL_FLOAT, false, sizeof(Vertex3), 0 },
};
LIBGUI_VERTEX_FORMAT_SPEC(Vertex3, 3 * sizeof(float))

AttribSpec const Vertex3Tex::_spec[2] = {
    { AttribSpec::Position,  3, GL_FLOAT, false, sizeof(Vertex3Tex), 0 },
    { AttribSpec::TexCoord,  2, GL_FLOAT, false, sizeof(Vertex3Tex), 3 * sizeof(float) }
};
LIBGUI_VERTEX_FORMAT_SPEC(Vertex3Tex, 5 * sizeof(float))

AttribSpec const Vertex3TexRgba::_spec[3] = {
    { AttribSpec::Position,  3, GL_FLOAT, false, sizeof(Vertex3TexRgba), 0 },
    { AttribSpec::TexCoord,  2, GL_FLOAT, false, sizeof(Vertex3TexRgba), 3 * sizeof(float) },
    { AttribSpec::Color,     4, GL_FLOAT, false, sizeof(Vertex3TexRgba), 5 * sizeof(float) }
};
LIBGUI_VERTEX_FORMAT_SPEC(Vertex3TexRgba, 9 * sizeof(float))

AttribSpec const Vertex3TexBoundsRgba::_spec[4] = {
    { AttribSpec::Position,   3, GL_FLOAT, false, sizeof(Vertex3TexBoundsRgba), 0 },
    { AttribSpec::TexCoord,   2, GL_FLOAT, false, sizeof(Vertex3TexBoundsRgba), 3 * sizeof(float) },
    { AttribSpec::TexBounds,  4, GL_FLOAT, false, sizeof(Vertex3TexBoundsRgba), 5 * sizeof(float) },
    { AttribSpec::Color,      4, GL_FLOAT, false, sizeof(Vertex3TexBoundsRgba), 9 * sizeof(float) }
};
LIBGUI_VERTEX_FORMAT_SPEC(Vertex3TexBoundsRgba, 13 * sizeof(float))

AttribSpec const Vertex3Tex2BoundsRgba::_spec[5] = {
    { AttribSpec::Position,   3, GL_FLOAT, false, sizeof(Vertex3Tex2BoundsRgba), 0 },
    { AttribSpec::TexCoord0,  2, GL_FLOAT, false, sizeof(Vertex3Tex2BoundsRgba), 3 * sizeof(float) },
    { AttribSpec::TexCoord1,  2, GL_FLOAT, false, sizeof(Vertex3Tex2BoundsRgba), 5 * sizeof(float) },
    { AttribSpec::TexBounds,  4, GL_FLOAT, false, sizeof(Vertex3Tex2BoundsRgba), 7 * sizeof(float) },
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
    { AttribSpec::TexCoord,  2, GL_FLOAT, false, sizeof(Vertex3NormalTexRgba), 6 * sizeof(float) },
    { AttribSpec::Color,     4, GL_FLOAT, false, sizeof(Vertex3NormalTexRgba), 8 * sizeof(float) }
};
LIBGUI_VERTEX_FORMAT_SPEC(Vertex3NormalTexRgba, 12 * sizeof(float))

AttribSpec const Vertex3NormalTangentTex::_spec[5] = {
    { AttribSpec::Position,  3, GL_FLOAT, false, sizeof(Vertex3NormalTangentTex), 0 },
    { AttribSpec::Normal,    3, GL_FLOAT, false, sizeof(Vertex3NormalTangentTex), 3 * sizeof(float) },
    { AttribSpec::Tangent,   3, GL_FLOAT, false, sizeof(Vertex3NormalTangentTex), 6 * sizeof(float) },
    { AttribSpec::Bitangent, 3, GL_FLOAT, false, sizeof(Vertex3NormalTangentTex), 9 * sizeof(float) },
    { AttribSpec::TexCoord,  2, GL_FLOAT, false, sizeof(Vertex3NormalTangentTex), 12 * sizeof(float) }
};
LIBGUI_VERTEX_FORMAT_SPEC(Vertex3NormalTangentTex, 14 * sizeof(float))

//----------------------------------------------------------------------------

static duint drawCounter = 0;

DENG2_PIMPL(GLBuffer)
{
    GLuint           bufferType      = GL_ARRAY_BUFFER;
    GLuint           vao             = 0;
    GLProgram const *vaoBoundProgram = nullptr;
    GLuint           name            = 0;
    GLuint           idxName         = 0;
    dsize            count           = 0;
    dsize            idxCount        = 0;
    GLenum           prim            = GL_POINTS;
    DrawRanges       defaultRange; ///< The default is all vertices.
    AttribSpecs      specs{nullptr, 0};

    Impl(Public *i, Type type)
        : Base(i)
        , bufferType(type == Texture? GL_TEXTURE_BUFFER : GL_ARRAY_BUFFER)
    {}

    ~Impl()
    {
        release();
        releaseIndices();
        releaseArray();
    }

    void allocArray()
    {
        if (bufferType != GL_ARRAY_BUFFER) return;
#if defined (DENG_HAVE_VAOS)
        if (!vao)
        {
            LIBGUI_GL.glGenVertexArrays(1, &vao);
        }
#endif
    }

    void releaseArray()
    {
#if defined (DENG_HAVE_VAOS)
        if (vao)
        {
            LIBGUI_GL.glDeleteVertexArrays(1, &vao);
            vao = 0;
            vaoBoundProgram = nullptr;
        }
#endif
    }

    void alloc()
    {
        if (!name)
        {
            LIBGUI_GL.glGenBuffers(1, &name);
        }
    }

    void allocIndices()
    {
        if (bufferType != GL_ARRAY_BUFFER) return;
        if (!idxName)
        {
            LIBGUI_GL.glGenBuffers(1, &idxName);
        }
    }

    void release()
    {
        if (name)
        {
            LIBGUI_GL.glDeleteBuffers(1, &name);
            name = 0;
            count = 0;
            vaoBoundProgram = nullptr;
        }
    }

    void releaseIndices()
    {
        if (idxName)
        {
            LIBGUI_GL.glDeleteBuffers(1, &idxName);
            idxName = 0;
            idxCount = 0;
        }
    }

    static GLenum glUsage(Usage u)
    {
        switch (u)
        {
        case Static:      return GL_STATIC_DRAW;
        case StaticRead:  return GL_STATIC_READ;
        case StaticCopy:  return GL_STATIC_COPY;
        case Dynamic:     return GL_DYNAMIC_DRAW;
        case DynamicRead: return GL_DYNAMIC_READ;
        case DynamicCopy: return GL_DYNAMIC_COPY;
        case Stream:      return GL_STREAM_DRAW;
        case StreamRead:  return GL_STREAM_READ;
        case StreamCopy:  return GL_STREAM_COPY;
        }
        DENG2_ASSERT(false);
        return GL_STATIC_DRAW;
    }

    static GLenum glPrimitive(Primitive p)
    {
        switch (p)
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

    void setAttribPointer(GLuint index, AttribSpec const &spec, int divisor, int part = 0) const
    {
        DENG2_ASSERT(!part || spec.type == GL_FLOAT);

        auto &GL = LIBGUI_GL;

        GL.glEnableVertexAttribArray(index + part);
        LIBGUI_ASSERT_GL_OK();

        GL.glVertexAttribPointer(
            index + part,
                                        min(4, spec.size),
                                        spec.type,
                                        spec.normalized,
            GLsizei(spec.stride),
            reinterpret_cast<void const *>(dintptr(spec.startOffset + part * 4 * sizeof(float))));
        LIBGUI_ASSERT_GL_OK();

#if defined (DENG_HAVE_INSTANCES)
        GL.glVertexAttribDivisor(index + part, divisor);
        LIBGUI_ASSERT_GL_OK();
#else
        DENG2_UNUSED(divisor);
#endif
    }

    void enableArrays(bool enable, int divisor = 0, GLuint vaoName = 0)
    {
        auto &GL = LIBGUI_GL;

        if (!enable)
        {
#if defined (DENG_HAVE_VAOS)
            GL.glBindVertexArray(0);
#endif
            return;
        }

        DENG2_ASSERT(GLProgram::programInUse());
        DENG2_ASSERT(specs.first != 0); // must have a spec
#if defined (DENG_HAVE_VAOS)
        DENG2_ASSERT(vaoName || vao);
#else
        DENG2_UNUSED(vaoName);
#endif

#if defined (DENG_HAVE_VAOS)
        GL.glBindVertexArray(vaoName? vaoName : vao);
#endif
        GL.glBindBuffer(GL_ARRAY_BUFFER, name);

        // Arrays are updated for a particular program.
        vaoBoundProgram = GLProgram::programInUse();

        for (duint i = 0; i < specs.second; ++i)
        {
            AttribSpec const &spec = specs.first[i];

            int index = vaoBoundProgram->attributeLocation(spec.semantic);
            if (index < 0) continue; // Not used.

            if (spec.size == 16)
            {
                // Attributes with more than 4 elements must be broken down.
                for (int part = 0; part < 4; ++part)
                {
                    if (enable)
                    {
                        setAttribPointer(index, spec, divisor, part);
                    }
                    else
                    {
                        GL.glDisableVertexAttribArray(index + part);
                        LIBGUI_ASSERT_GL_OK();
                    }
                }
            }
            else
            {
                if (enable)
                {
                    setAttribPointer(index, spec, divisor);
                }
                else
                {
                    GL.glDisableVertexAttribArray(index);
                    LIBGUI_ASSERT_GL_OK();
                }
            }
        }

        GL.glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    void bindArray(bool doBind)
    {
#if defined (DENG_HAVE_VAOS)
        if (doBind)
        {
            DENG2_ASSERT(vao != 0);
            DENG2_ASSERT(GLProgram::programInUse());
            if (vaoBoundProgram != GLProgram::programInUse())
            {
                enableArrays(true);
            }
            else
            {
                // Just bind it, the setup is already good.
                LIBGUI_GL.glBindVertexArray(vao);
            }
        }
        else
        {
            LIBGUI_GL.glBindVertexArray(0);
        }
#else
        enableArrays(doBind);
#endif
    }
};

GLBuffer::GLBuffer(Type bufferType) : d(new Impl(this, bufferType))
{}

void GLBuffer::clear()
{
    setState(NotReady);
    d->release();
    d->releaseIndices();
    d->releaseArray();
}

void GLBuffer::setVertices(dsize count, void const *data, dsize dataSize, Usage usage)
{
    setVertices(Points, count, data, dataSize, usage);
}

void GLBuffer::setVertices(Primitive primitive, dsize count, void const *data, dsize dataSize, Usage usage)
{
    DENG2_ASSERT(d->bufferType == GL_ARRAY_BUFFER);

    d->prim         = Impl::glPrimitive(primitive);
    d->count        = count;
    d->defaultRange = DrawRanges{{0}, {GLsizei(count)}};

    if (data)
    {
        d->allocArray();
        d->alloc();

        if (dataSize && count)
        {
            auto &GL = LIBGUI_GL;
            GL.glBindBuffer(d->bufferType, d->name);
            GL.glBufferData(d->bufferType, dataSize, data, Impl::glUsage(usage));
            GL.glBindBuffer(d->bufferType, 0);
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
    d->prim         = Impl::glPrimitive(primitive);
    d->idxCount     = count;
    d->defaultRange = DrawRanges{{0}, {GLsizei(count)}};

    if (indices && count)
    {
        d->allocArray();
        d->allocIndices();

        auto &GL = LIBGUI_GL;
        GL.glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, d->idxName);
        GL.glBufferData(GL_ELEMENT_ARRAY_BUFFER, GLsizeiptr(count * sizeof(Index)),
                        indices, Impl::glUsage(usage));
        GL.glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
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

void GLBuffer::setData(void const *data, dsize dataSize, gl::Usage usage)
{
    if (data && dataSize)
    {
        d->alloc();

        auto &GL = LIBGUI_GL;
        GL.glBindBuffer(d->bufferType, d->name);
        GL.glBufferData(d->bufferType, GLsizeiptr(dataSize), data, Impl::glUsage(usage));
        GL.glBindBuffer(d->bufferType, 0);
    }
    else
    {
        d->release();
    }
}

void GLBuffer::setData(dsize startOffset, void const *data, dsize dataSize)
{
    DENG2_ASSERT(isReady());

    if (data && dataSize)
    {
        auto &GL = LIBGUI_GL;
        GL.glBindBuffer   (d->bufferType, d->name);
        GL.glBufferSubData(d->bufferType, GLintptr(startOffset), GLsizeiptr(dataSize), data);
        GL.glBindBuffer   (d->bufferType, 0);
    }
}

void GLBuffer::setUninitializedData(dsize dataSize, gl::Usage usage)
{
    d->count = 0;
    d->defaultRange = DrawRanges{{0}, {0}};

    d->allocArray();
    d->alloc();

    LIBGUI_GL.glBindBuffer(d->bufferType, d->name);
    LIBGUI_GL.glBufferData(d->bufferType, GLsizeiptr(dataSize), nullptr, Impl::glUsage(usage));
    LIBGUI_GL.glBindBuffer(d->bufferType, 0);

    setState(Ready);
}

void GLBuffer::draw(DrawRanges const *ranges) const
{
    if (!isReady() || !GLProgram::programInUse()) return;

    DENG2_ASSERT(d->bufferType == GL_ARRAY_BUFFER);

    // Mark the current target changed.
    GLState::current().target().markAsChanged();

    auto &GL = LIBGUI_GL;

    d->bindArray(true);

#if defined (DENG2_DEBUG)
    // Check that the shader program is ready to be used.
    {
        const GLuint progName = GLProgram::programInUse()->glName();
        GLint isValid;
        LIBGUI_GL.glValidateProgram(progName);
        LIBGUI_GL.glGetProgramiv(progName, GL_VALIDATE_STATUS, &isValid);
        if (!isValid)
        {
            qDebug() << "[GLProgram] Program" << progName << "status invalid";

            dint32 logSize;
            dint32 count;
            GL.glGetProgramiv(progName, GL_INFO_LOG_LENGTH, &logSize);

            Block log(logSize);
            GL.glGetProgramInfoLog(progName, logSize, &count, reinterpret_cast<GLchar *>(log.data()));

            qDebug() << "Program info log:" << log.constData();
        }
    }
#endif

    const DrawRanges &drawRanges = (ranges? *ranges : d->defaultRange);

    if (d->idxName)
    {
        GL.glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, d->idxName);
        DENG2_ASSERT(GLProgram::programInUse()->validate());
        if (drawRanges.size() == 1)
        {
            GL.glDrawElements(d->prim,
                              drawRanges.count[0],
                              GL_UNSIGNED_SHORT,
                              reinterpret_cast<const void *>(dintptr(drawRanges.first[0] * 2)));
            LIBGUI_ASSERT_GL_OK();
        }
        else
        {
            const void **indices = new const void *[drawRanges.size()];
            for (int i = 0; i < drawRanges.size(); ++i)
            {
                indices[i] = reinterpret_cast<const void *>(dintptr(drawRanges.first[i] * 2));
            }

            GL.glMultiDrawElements(d->prim,
                                   drawRanges.count.data(),
                                   GL_UNSIGNED_SHORT,
                                   indices,
                                   drawRanges.size());
            LIBGUI_ASSERT_GL_OK();

            delete [] indices;
        }

        GL.glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    }
    else
    {
        DENG2_ASSERT(GLProgram::programInUse()->validate());
        if (drawRanges.size() == 1)
        {
            GL.glDrawArrays(d->prim, drawRanges.first[0], drawRanges.count[0]);
            LIBGUI_ASSERT_GL_OK();
        }
        else
        {
            GL.glMultiDrawArrays(d->prim,
                                 drawRanges.first.data(),
                                 drawRanges.count.data(),
                                 drawRanges.size());
            LIBGUI_ASSERT_GL_OK();
        }
    }
    ++drawCounter;

#ifdef DENG2_DEBUG
    DENG2_ASSERT(GLDrawQueue_queuedElems == 0);
#endif

    d->bindArray(false);
}

void GLBuffer::drawWithIndices(GLBuffer const &indexBuffer) const
{
    if (!isReady() || !indexBuffer.d->idxName || !GLProgram::programInUse()) return;

    // Mark the current target changed.
    GLState::current().target().markAsChanged();

    auto &GL = LIBGUI_GL;

    d->bindArray(true);

    DENG2_ASSERT(GLProgram::programInUse()->validate());

    GL.glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer.d->idxName);
    GL.glDrawElements(indexBuffer.d->prim,
                      GLsizei(indexBuffer.d->idxCount),
                      GL_UNSIGNED_SHORT, nullptr);
    LIBGUI_ASSERT_GL_OK();
    GL.glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    ++drawCounter;

    d->bindArray(false);
}

void GLBuffer::drawWithIndices(gl::Primitive primitive, Index const *indices, dsize count) const
{
    if (!isReady() || !indices || !count || !GLProgram::programInUse()) return;

    GLState::current().target().markAsChanged();

    auto &GL = LIBGUI_GL;

    d->bindArray(true);
    DENG2_ASSERT(GLProgram::programInUse()->validate());
    GL.glDrawElements(Impl::glPrimitive(primitive), GLsizei(count), GL_UNSIGNED_SHORT, indices);
    LIBGUI_ASSERT_GL_OK();
    ++drawCounter;

    d->bindArray(false);
}

void GLBuffer::drawInstanced(GLBuffer const &instanceAttribs, duint first, dint count) const
{
#if defined (DENG_HAVE_INSTANCES)

    if (!isReady() || !instanceAttribs.isReady() || !GLProgram::programInUse())
    {
#if defined (DENG2_DEBUG)
        qDebug() << "[GLBuffer] isReady:" << isReady()
                 << "instAttr isReady:" << instanceAttribs.isReady()
                 << "progInUse:" << GLProgram::programInUse();
#endif
        return;
    }

    LIBGUI_ASSERT_GL_OK();

    // Mark the current target changed.
    GLState::current().target().markAsChanged();

    auto &GL = LIBGUI_GL;

    d->enableArrays(true);

    LIBGUI_ASSERT_GL_OK();

    // Set up the instance data, using this buffer's VAO.
    instanceAttribs.d->enableArrays(true, 1 /* per instance */, d->vao);

    LIBGUI_ASSERT_GL_OK();

    if (d->idxName)
    {
        if (count < 0) count = d->idxCount;
        if (first + count > d->idxCount) count = d->idxCount - first;

        DENG2_ASSERT(count >= 0);

        GL.glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, d->idxName);
        DENG2_ASSERT(GLProgram::programInUse()->validate());
        GL.glDrawElementsInstanced(d->prim, count, GL_UNSIGNED_SHORT,
                                   reinterpret_cast<void const *>(dintptr(first * 2)),
                                   GLsizei(instanceAttribs.count()));
        LIBGUI_ASSERT_GL_OK();
        GL.glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    }
    else
    {
        if (count < 0) count = d->count;
        if (first + count > d->count) count = d->count - first;

        DENG2_ASSERT(count >= 0);
        DENG2_ASSERT(GLProgram::programInUse()->validate());

        GL.glDrawArraysInstanced(d->prim, first, count,
                                 GLsizei(instanceAttribs.count()));
        LIBGUI_ASSERT_GL_OK();
    }

    d->enableArrays(false);
    instanceAttribs.d->enableArrays(false);

#else

    // Instanced drawing is not available.
    DENG2_UNUSED(instanceAttribs);
    DENG2_UNUSED(first);
    DENG2_UNUSED(count);

#endif
}

dsize GLBuffer::count() const
{
    return d->count;
}

void GLBuffer::setFormat(AttribSpecs const &format)
{
    d->specs = format;
}

GLuint GLBuffer::glName() const
{
    return d->name;
}

duint GLBuffer::drawCount() // static
{
    return drawCounter;
}

void GLBuffer::resetDrawCount() // static
{
    drawCounter = 0;
}

} // namespace de
