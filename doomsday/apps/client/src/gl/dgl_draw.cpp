/** @file dgl_draw.cpp  Drawing operations and vertex arrays.
 *
 * Emulates OpenGL 1.x drawing for legacy code.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2007-2015 Daniel Swanson <danij@dengine.net>
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

#include "de_base.h"
#include "gl/gl_main.h"

#include <cstdio>
#include <cstdlib>
#include <de/concurrency.h>
#include <de/GLInfo>
#include <de/GLBuffer>
#include <de/GLState>
#include <de/GLProgram>
#include <de/Matrix>
#include "sys_system.h"
#include "gl/gl_draw.h"
#include "gl/sys_opengl.h"
#include "clientapp.h"

using namespace de;

constexpr uint MAX_TEX_COORDS = 2;
constexpr int  MAX_BATCH      = 16;

static unsigned s_drawCallCount = 0;
static unsigned s_primSwitchCount = 0;
static unsigned s_minBatchLength = 0;
static unsigned s_maxBatchLength = 0;
static unsigned s_totalBatchCount = 0;

struct DGLDrawState
{
    struct Vertex
    {
        float   vertex[3];
        uint8_t color[4];
        struct { float s, t; } texCoord[MAX_TEX_COORDS];
        float   fragOffset[2];  // Multiplied by uFragmentOffset
        float   batchIndex;
    };

    // Indices for vertex attribute arrays.
    enum {
        VAA_VERTEX,
        VAA_COLOR,
        VAA_TEXCOORD0,
        VAA_TEXCOORD1,
        VAA_FRAG_OFFSET,
        VAA_BATCH_INDEX,
        NUM_VERTEX_ATTRIB_ARRAYS
    };

    dglprimtype_t   primType      = DGL_NO_PRIMITIVE;
    dglprimtype_t   batchPrimType = DGL_NO_PRIMITIVE;
    int             primIndex     = 0;
    duint           batchMaxSize;
    duint           currentBatchIndex;
    bool            resetPrimitive = false;
    Vertex          currentVertex  = {
        {0.f, 0.f, 0.f},
        {255, 255, 255, 255},
        {{0.f, 0.f}, {0.f, 0.f}},
        {0.f, 0.f},
        0.f};
    Vertex          primVertices[4];
    QVector<Vertex> vertices;

    struct GLData
    {
        GLProgram shader;

        GLState  batchState;
        Matrix4f batchMvpMatrix[MAX_BATCH];
        Matrix4f batchTexMatrix0[MAX_BATCH];
        Matrix4f batchTexMatrix1[MAX_BATCH];
        int      batchTexEnabled[MAX_BATCH];
        int      batchTexMode[MAX_BATCH];
        Vector4f batchTexModeColor[MAX_BATCH];
        float    batchAlphaLimit[MAX_BATCH];
        int      batchTexture0[MAX_BATCH];
        int      batchTexture1[MAX_BATCH];

        // Batched uniforms:
        GLUniform uMvpMatrix;
        GLUniform uTexMatrix0;
        GLUniform uTexMatrix1;
        GLUniform uTexEnabled;
        GLUniform uTexMode;
        GLUniform uTexModeColor;
        GLUniform uAlphaLimit;

        GLUniform uFragmentSize;
        GLUniform uFogRange;
        GLUniform uFogColor;

        struct DrawBuffer
        {
            GLuint   vertexArray = 0;
            GLBuffer arrayData;

            void release()
            {
#if defined (DENG_HAVE_VAOS)
                LIBGUI_GL.glDeleteVertexArrays(1, &vertexArray);
#endif
                arrayData.clear();
            }
        };

        GLData(duint batchSize)
            : uMvpMatrix    { "uMvpMatrix",    GLUniform::Mat4Array , batchSize }
            , uTexMatrix0   { "uTexMatrix0",   GLUniform::Mat4Array , batchSize }
            , uTexMatrix1   { "uTexMatrix1",   GLUniform::Mat4Array , batchSize }
            , uTexEnabled   { "uTexEnabled",   GLUniform::IntArray  , batchSize }
            , uTexMode      { "uTexMode",      GLUniform::IntArray  , batchSize }
            , uTexModeColor { "uTexModeColor", GLUniform::Vec4Array , batchSize }
            , uAlphaLimit   { "uAlphaLimit",   GLUniform::FloatArray, batchSize }
            , uFragmentSize { "uFragmentSize", GLUniform::Vec2 }
            , uFogRange     { "uFogRange",     GLUniform::Vec4 }
            , uFogColor     { "uFogColor",     GLUniform::Vec4 }
        {}

        QVector<DrawBuffer *> buffers;
        int bufferPos = 0;
    };
    std::unique_ptr<GLData> gl;

    DGLDrawState()
    {
        clearVertices();
    }

    void checkPrimitiveReset()
    {
        if (resetPrimitive)
        {
            DENG2_ASSERT(!vertices.empty());
            DENG2_ASSERT(glPrimitive(batchPrimType) == GL_TRIANGLE_STRIP);

            // When committing multiple triangle strips, add a disconnection
            // between batches.
            vertices.push_back(vertices.back());
            vertices.push_back(currentVertex);
            resetPrimitive = false;
        }
    }

    void commitLine(Vertex start, Vertex end)
    {
        const Vector2f lineDir = (Vector2f(end.vertex) - Vector2f(start.vertex)).normalize();
        const Vector2f lineNormal{-lineDir.y, lineDir.x};

        const bool disjoint = !vertices.empty();
        if (disjoint)
        {
            vertices.push_back(vertices.back());
        }

        // Start cap.
        {
            start.fragOffset[0] = -lineNormal.x;
            start.fragOffset[1] = -lineNormal.y;
            vertices.push_back(start);
            if (disjoint)
            {
                vertices.push_back(start);
            }
            start.fragOffset[0] = lineNormal.x;
            start.fragOffset[1] = lineNormal.y;
            vertices.push_back(start);
        }

        // End cap.
        {
            end.fragOffset[0] = -lineNormal.x;
            end.fragOffset[1] = -lineNormal.y;
            vertices.push_back(end);
            end.fragOffset[0] = lineNormal.x;
            end.fragOffset[1] = lineNormal.y;
            vertices.push_back(end);
        }
    }

    void commitVertex()
    {
        currentVertex.batchIndex = float(currentBatchIndex);
        ++primIndex;

        switch (primType)
        {
            case DGL_QUADS:
                primVertices[primIndex - 1] = currentVertex;
                if (primIndex == 4)
                {
                    /* 4 vertices become 6:

                       0--1     0--1   5
                       |  |      \ |   |\
                       |  |  =>   \|   | \
                       3--2        2   4--3  */

                    vertices.push_back(primVertices[0]);
                    vertices.push_back(primVertices[1]);
                    vertices.push_back(primVertices[2]);

                    vertices.push_back(primVertices[0]);
                    vertices.push_back(primVertices[2]);
                    vertices.push_back(primVertices[3]);

                    primIndex = 0;
                }
                break;

            case DGL_LINES:
                primVertices[primIndex - 1] = currentVertex;
                if (primIndex == 2)
                {
                    commitLine(primVertices[0], primVertices[1]);
                    primIndex = 0;
                }
                break;

            case DGL_LINE_LOOP:
            case DGL_LINE_STRIP:
                if (primIndex == 1)
                {
                    // Remember the first one for a loop.
                    primVertices[0] = currentVertex;
                }
                if (primIndex > 1)
                {
                    // Continue from the previous vertex.
                    commitLine(primVertices[1], currentVertex);
                }
                primVertices[1] = currentVertex;
                break;

            case DGL_TRIANGLE_FAN:
                if (primIndex == 1)
                {
                    if (!vertices.empty())
                    {
                        resetPrimitive = true;
                    }
                    checkPrimitiveReset();

                    // Fan origin.
                    primVertices[0] = currentVertex;
                }
                else if (primIndex > 2)
                {
                    vertices.push_back(primVertices[0]);
                }
                vertices.push_back(currentVertex);
                break;

            default:
                checkPrimitiveReset();
                vertices.push_back(currentVertex);
                break;
        }
    }

    void clearPrimitive()
    {
        primIndex = 0;
        primType  = DGL_NO_PRIMITIVE;
    }

    void clearVertices()
    {
        // currentVertex is unaffected.
        vertices.clear();
        clearPrimitive();
        currentBatchIndex = 0;
        resetPrimitive    = false;
    }

    inline int numVertices() const
    {
        return vertices.size();
    }

    static Vector4ub colorFromFloat(const Vector4f &color)
    {
        Vector4i rgba = (color * 255 + Vector4f(0.5f, 0.5f, 0.5f, 0.5f))
                .toVector4i()
                .max(Vector4i(0, 0, 0, 0))
                .min(Vector4i(255, 255, 255, 255));
        return Vector4ub(dbyte(rgba.x), dbyte(rgba.y), dbyte(rgba.z), dbyte(rgba.w));
    }

    void beginPrimitive(dglprimtype_t primitive)
    {
        glInit();

        DENG2_ASSERT(primType == DGL_NO_PRIMITIVE);

        if (batchPrimType != DGL_NO_PRIMITIVE && !isCompatible(batchPrimType, primitive))
        {
            ++s_primSwitchCount;
            flushBatches();
        }
        else
        {
            if (currentBatchIndex == MAX_BATCH - 1)
            {
                flushBatches();
            }
        }

        // We enter a Begin/End section.
        batchPrimType = primType = primitive;

        beginBatch();
    }

    void endPrimitive()
    {
        if (primType != DGL_NO_PRIMITIVE && !vertices.empty())
        {
            if (primType == DGL_LINE_LOOP)
            {
                // Close the loop.
                commitLine(currentVertex, primVertices[0]);
            }
            resetPrimitive = true;
            DENG2_ASSERT(!vertices.empty());
            endBatch();
        }
        clearPrimitive();
    }

    void getBoundTextures(int &id0, int &id1)
    {
        auto &GL = LIBGUI_GL;
        GL.glActiveTexture(GL_TEXTURE0);
        GL.glGetIntegerv(GL_TEXTURE_BINDING_2D, &id0);
        GL.glActiveTexture(GL_TEXTURE1);
        GL.glGetIntegerv(GL_TEXTURE_BINDING_2D, &id1);
        GL.glActiveTexture(GLenum(GL_TEXTURE0 + DGL_GetInteger(DGL_ACTIVE_TEXTURE)));
    }

    static inline bool isLinePrimitive(DGLenum p)
    {
        return p == DGL_LINES || p == DGL_LINE_STRIP || p == DGL_LINE_LOOP;
    }

    static inline bool isCompatible(DGLenum p1, DGLenum p2)
    {
        // Lines are not considered separate because they need the uFragmentSize uniform
        // for calculating thickness offsets.
        if (isLinePrimitive(p1) != isLinePrimitive(p2))
        {
            return false;
        }
        return glPrimitive(p1) == glPrimitive(p2);
    }

    void beginBatch()
    {
        const duint idx = currentBatchIndex;

        auto &dynamicState = GLState::current();

        if (idx == 0)
        {
            gl->batchState = dynamicState;
            zap(gl->batchTexture0);
            zap(gl->batchTexture1);
        }
        else
        {
#if defined (DENG2_DEBUG)
            // GLState must not change while batches are begin collected.
            // (Apart from the dynamic properties.)
            GLState bat = gl->batchState;
            GLState cur = dynamicState;
            for (auto *st : {&bat, &cur})
            {
                st->setAlphaLimit(0.f);
                st->setAlphaTest(false);
            }
            DENG2_ASSERT(bat == cur);
#endif
        }

        gl->batchMvpMatrix[idx]  = DGL_Matrix(DGL_PROJECTION) * DGL_Matrix(DGL_MODELVIEW);
        gl->batchTexMatrix0[idx] = DGL_Matrix(DGL_TEXTURE0);
        gl->batchTexMatrix1[idx] = DGL_Matrix(DGL_TEXTURE1);
        gl->batchTexEnabled[idx] =
            (DGL_GetInteger(DGL_TEXTURE0) ? 0x1 : 0) | (DGL_GetInteger(DGL_TEXTURE1) ? 0x2 : 0);
        gl->batchTexMode[idx]      = DGL_GetInteger(DGL_MODULATE_TEXTURE);
        gl->batchTexModeColor[idx] = DGL_ModulationColor();
        gl->batchAlphaLimit[idx]   = (dynamicState.alphaTest() ? dynamicState.alphaLimit() : -1.f);

        // TODO: There is no need to use OpenGL to remember the bound textures.
        // However, all DGL textures must be bound via DGL_Bind and not directly via OpenGL.

        getBoundTextures(gl->batchTexture0[idx], gl->batchTexture1[idx]);
    }

    void endBatch()
    {
        currentBatchIndex++;
        if (currentBatchIndex == batchMaxSize)
        {
            flushBatches();
        }
    }

    void flushBatches()
    {
#if defined (DENG2_DEBUG)
        if (DGL_GetInteger(DGL_FLUSH_BACKTRACE))
        {
            DENG2_PRINT_BACKTRACE();
        }
#endif
        if (currentBatchIndex > 0)
        {
            drawBatches();
        }
        clearVertices();
    }

    void glInit()
    {
        DENG_ASSERT_GL_CONTEXT_ACTIVE();

        if (!gl)
        {
            batchMaxSize = DGL_BatchMaxSize();

            gl.reset(new GLData(batchMaxSize));

            // Set up the shader.
            ClientApp::shaders().build(gl->shader, "dgl.draw")
                    << gl->uFragmentSize
                    << gl->uMvpMatrix
                    << gl->uTexMatrix0
                    << gl->uTexMatrix1
                    << gl->uTexEnabled
                    << gl->uTexMode
                    << gl->uTexModeColor
                    << gl->uAlphaLimit
                    << gl->uFogRange
                    << gl->uFogColor;

            auto &GL = LIBGUI_GL;

            // Sampler uniforms.
            {
                int samplers[2][MAX_BATCH];
                for (int i = 0; i < batchMaxSize; ++i)
                {
                    samplers[0][i] = i;
                    samplers[1][i] = batchMaxSize + i;
                }

                auto prog = gl->shader.glName();
                GL.glUseProgram(prog);
                GL.glUniform1iv(GL.glGetUniformLocation(prog, "uTex0[0]"), GLsizei(batchMaxSize), samplers[0]);
                LIBGUI_ASSERT_GL_OK();
                GL.glUniform1iv(GL.glGetUniformLocation(prog, "uTex1[0]"), GLsizei(batchMaxSize), samplers[1]);
                LIBGUI_ASSERT_GL_OK();
                GL.glUseProgram(0);
            }
        }
    }

    void glDeinit()
    {
        if (gl)
        {
            foreach (GLData::DrawBuffer *dbuf, gl->buffers)
            {
                dbuf->release();
                delete dbuf;
            }
            gl.reset();
        }
    }

    GLData::DrawBuffer &nextBuffer()
    {
        if (gl->bufferPos == gl->buffers.size())
        {
            auto *dbuf = new GLData::DrawBuffer;

            // Vertex array object.
#if defined (DENG_HAVE_VAOS)
            {
                auto &GL = LIBGUI_GL;
                GL.glGenVertexArrays(1, &dbuf->vertexArray);
                GL.glBindVertexArray(dbuf->vertexArray);
                for (uint i = 0; i < NUM_VERTEX_ATTRIB_ARRAYS; ++i)
                {
                    GL.glEnableVertexAttribArray(i);
                }
                GL.glBindVertexArray(0);
            }
#endif

            gl->buffers.append(dbuf);
        }
        return *gl->buffers[gl->bufferPos++];
    }

    void glBindArrays()
    {
        const uint stride = sizeof(Vertex);
        auto &GL = LIBGUI_GL;

        // Upload the vertex data.
        GLData::DrawBuffer &buf = nextBuffer();
        buf.arrayData.setData(&vertices[0], sizeof(Vertex) * vertices.size(), gl::Dynamic);

#if defined (DENG_HAVE_VAOS)
        GL.glBindVertexArray(buf.vertexArray);
#else
        for (uint i = 0; i < NUM_VERTEX_ATTRIB_ARRAYS; ++i)
        {
            GL.glEnableVertexAttribArray(i);
        }
#endif
        LIBGUI_ASSERT_GL_OK();

        GL.glBindBuffer(GL_ARRAY_BUFFER, buf.arrayData.glName());
        LIBGUI_ASSERT_GL_OK();

        DENG2_ASSERT(GL.glGetAttribLocation(gl->shader.glName(), "aVertex") == VAA_VERTEX);
        DENG2_ASSERT(GL.glGetAttribLocation(gl->shader.glName(), "aColor") == VAA_COLOR);
        DENG2_ASSERT(GL.glGetAttribLocation(gl->shader.glName(), "aTexCoord") == VAA_TEXCOORD0);
        DENG2_ASSERT(GL.glGetAttribLocation(gl->shader.glName(), "aFragOffset") == VAA_FRAG_OFFSET);
        DENG2_ASSERT(GL.glGetAttribLocation(gl->shader.glName(), "aBatchIndex") == VAA_BATCH_INDEX);

        // Updated pointers.
        GL.glVertexAttribPointer(VAA_VERTEX,      3, GL_FLOAT,         GL_FALSE, stride, DENG2_OFFSET_PTR(Vertex, vertex));
        GL.glVertexAttribPointer(VAA_COLOR,       4, GL_UNSIGNED_BYTE, GL_TRUE,  stride, DENG2_OFFSET_PTR(Vertex, color));
        GL.glVertexAttribPointer(VAA_TEXCOORD0,   2, GL_FLOAT,         GL_FALSE, stride, DENG2_OFFSET_PTR(Vertex, texCoord[0]));
        GL.glVertexAttribPointer(VAA_TEXCOORD1,   2, GL_FLOAT,         GL_FALSE, stride, DENG2_OFFSET_PTR(Vertex, texCoord[1]));
        GL.glVertexAttribPointer(VAA_FRAG_OFFSET, 2, GL_FLOAT,         GL_FALSE, stride, DENG2_OFFSET_PTR(Vertex, fragOffset[0]));
        GL.glVertexAttribPointer(VAA_BATCH_INDEX, 1, GL_FLOAT,         GL_FALSE, stride, DENG2_OFFSET_PTR(Vertex, batchIndex));
        LIBGUI_ASSERT_GL_OK();

        GL.glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    void glUnbindArrays()
    {
        auto &GL = LIBGUI_GL;

#if defined (DENG_HAVE_VAOS)
        GL.glBindVertexArray(0);
#else
        for (uint i = 0; i < NUM_VERTEX_ATTRIB_ARRAYS; ++i)
        {
            GL.glDisableVertexAttribArray(i);
            LIBGUI_ASSERT_GL_OK();
        }
#endif
    }

    void glBindBatchTextures(duint count)
    {
        auto &GL = LIBGUI_GL;
        for (duint i = 0; i < count; ++i)
        {
            GL.glActiveTexture(GLenum(GL_TEXTURE0 + i));
            GL.glBindTexture(GL_TEXTURE_2D, GLuint(gl->batchTexture0[i]));
            GL.glActiveTexture(GLenum(GL_TEXTURE0 + batchMaxSize + i));
            GL.glBindTexture(GL_TEXTURE_2D, GLuint(gl->batchTexture1[i]));
        }
    }

    static GLenum glPrimitive(DGLenum primitive)
    {
        switch (primitive)
        {
        case DGL_POINTS:            return GL_POINTS;
        case DGL_LINES:             return GL_TRIANGLE_STRIP;
        case DGL_LINE_LOOP:         return GL_TRIANGLE_STRIP;
        case DGL_LINE_STRIP:        return GL_TRIANGLE_STRIP;
        case DGL_TRIANGLES:         return GL_TRIANGLES;
        case DGL_TRIANGLE_FAN:      return GL_TRIANGLE_STRIP;
        case DGL_TRIANGLE_STRIP:    return GL_TRIANGLE_STRIP;
        case DGL_QUADS:             return GL_TRIANGLES;

        case DGL_NO_PRIMITIVE:      /*DENG2_ASSERT(!"No primitive type specified");*/ break;
        }
        return GL_NONE;
    }

    /**
     * Draws all the primitives currently stored in the vertex array.
     */
    void drawBatches()
    {
        const auto batchLength = duint(currentBatchIndex);

        s_minBatchLength = de::min(s_minBatchLength, batchLength);
        s_maxBatchLength = de::max(s_maxBatchLength, batchLength);
        s_totalBatchCount += batchLength;

        // Batched uniforms.
        gl->uMvpMatrix.set(gl->batchMvpMatrix, batchLength);
        gl->uTexMatrix0.set(gl->batchTexMatrix0, batchLength);
        gl->uTexMatrix1.set(gl->batchTexMatrix1, batchLength);
        gl->uTexEnabled.set(gl->batchTexEnabled, batchLength);
        gl->uTexMode.set(gl->batchTexMode, batchLength);
        gl->uTexModeColor.set(gl->batchTexModeColor, batchLength);
        gl->uAlphaLimit.set(gl->batchAlphaLimit, batchLength);

        const GLState &glState = gl->batchState;

        // Non-batched uniforms.
        if (isLinePrimitive(batchPrimType))
        {
            // We can't draw a line thinner than one pixel.
            const float lineWidth = std::max(.5f, GL_state.currentLineWidth);
            gl->uFragmentSize     = Vector2f(lineWidth, lineWidth) / glState.target().size();
        }
        else
        {
            gl->uFragmentSize = Vector2f();
        }
        DGL_FogParams(gl->uFogRange, gl->uFogColor);

        auto &GL = LIBGUI_GL;

        glState.apply();

        int oldTex[2];
        getBoundTextures(oldTex[0], oldTex[1]);

        glBindArrays();
        gl->shader.beginUse();
        glBindBatchTextures(batchLength);
        DENG2_ASSERT(gl->shader.validate());
        GL.glDrawArrays(glPrimitive(batchPrimType), 0, numVertices()); ++s_drawCallCount;
        gl->shader.endUse();
        LIBGUI_ASSERT_GL_OK();
        glUnbindArrays();

        // Restore the previously bound OpenGL textures.
        {
            GL.glActiveTexture(GL_TEXTURE0);
            GL.glBindTexture(GL_TEXTURE_2D, GLuint(oldTex[0]));
            GL.glActiveTexture(GL_TEXTURE1);
            GL.glBindTexture(GL_TEXTURE_2D, GLuint(oldTex[1]));
            GL.glActiveTexture(GLenum(GL_TEXTURE0 + DGL_GetInteger(DGL_ACTIVE_TEXTURE)));
        }
    }
};

static DGLDrawState dglDraw;

unsigned int DGL_BatchMaxSize()
{
    auto &GL = LIBGUI_GL;

    // This determines how long DGL batch draws can be.
    int maxFragSamplers;
    GL.glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &maxFragSamplers);

    return duint(de::min(MAX_BATCH, de::max(8, maxFragSamplers / 2))); // DGL needs two per draw.
}

void DGL_Shutdown()
{
    dglDraw.glDeinit();
}

void DGL_BeginFrame()
{
//    qDebug() << "draw calls:" << s_drawCallCount << "prim switch:" << s_primSwitchCount
//             << "batch min/max/avg:" << s_minBatchLength << s_maxBatchLength
//             << (s_drawCallCount ? float(s_totalBatchCount) / float(s_drawCallCount) : 0.f);

    s_drawCallCount   = 0;
    s_totalBatchCount = 0;
    s_primSwitchCount = 0;
    s_maxBatchLength  = 0;
    s_minBatchLength  = std::numeric_limits<decltype(s_minBatchLength)>::max();

    if (dglDraw.gl)
    {
        // Reuse buffers every frame.
        dglDraw.gl->bufferPos = 0;
    }
}

void DGL_Flush()
{
    // Finish all batched draws.
    dglDraw.flushBatches();
}

void DGL_CurrentColor(DGLubyte *rgba)
{
    std::memcpy(rgba, dglDraw.currentVertex.color, 4);
}

void DGL_CurrentColor(float *rgba)
{
    Vector4f colorf = Vector4ub(dglDraw.currentVertex.color).toVector4f() / 255.0;
    std::memcpy(rgba, colorf.constPtr(), sizeof(float) * 4);
}

#undef DGL_Color3ub
DENG_EXTERN_C void DGL_Color3ub(DGLubyte r, DGLubyte g, DGLubyte b)
{
    DENG2_ASSERT_IN_RENDER_THREAD();

    dglDraw.currentVertex.color[0] = r;
    dglDraw.currentVertex.color[1] = g;
    dglDraw.currentVertex.color[2] = b;
    dglDraw.currentVertex.color[3] = 255;
}

#undef DGL_Color3ubv
DENG_EXTERN_C void DGL_Color3ubv(DGLubyte const *vec)
{
    DENG2_ASSERT_IN_RENDER_THREAD();

    dglDraw.currentVertex.color[0] = vec[0];
    dglDraw.currentVertex.color[1] = vec[1];
    dglDraw.currentVertex.color[2] = vec[2];
    dglDraw.currentVertex.color[3] = 255;
}

#undef DGL_Color4ub
DENG_EXTERN_C void DGL_Color4ub(DGLubyte r, DGLubyte g, DGLubyte b, DGLubyte a)
{
    DENG2_ASSERT_IN_RENDER_THREAD();

    dglDraw.currentVertex.color[0] = r;
    dglDraw.currentVertex.color[1] = g;
    dglDraw.currentVertex.color[2] = b;
    dglDraw.currentVertex.color[3] = a;
}

#undef DGL_Color4ubv
DENG_EXTERN_C void DGL_Color4ubv(DGLubyte const *vec)
{
    DENG2_ASSERT_IN_RENDER_THREAD();

    dglDraw.currentVertex.color[0] = vec[0];
    dglDraw.currentVertex.color[1] = vec[1];
    dglDraw.currentVertex.color[2] = vec[2];
    dglDraw.currentVertex.color[3] = vec[3];
}

#undef DGL_Color3f
DENG_EXTERN_C void DGL_Color3f(float r, float g, float b)
{
    DENG2_ASSERT_IN_RENDER_THREAD();

    const auto color = DGLDrawState::colorFromFloat({r, g, b, 1.f});
    dglDraw.currentVertex.color[0] = color.x;
    dglDraw.currentVertex.color[1] = color.y;
    dglDraw.currentVertex.color[2] = color.z;
    dglDraw.currentVertex.color[3] = color.w;
}

#undef DGL_Color3fv
DENG_EXTERN_C void DGL_Color3fv(float const *vec)
{
    DENG2_ASSERT_IN_RENDER_THREAD();

    const auto color = DGLDrawState::colorFromFloat({Vector3f(vec), 1.f});
    dglDraw.currentVertex.color[0] = color.x;
    dglDraw.currentVertex.color[1] = color.y;
    dglDraw.currentVertex.color[2] = color.z;
    dglDraw.currentVertex.color[3] = color.w;
}

#undef DGL_Color4f
DENG_EXTERN_C void DGL_Color4f(float r, float g, float b, float a)
{
    DENG2_ASSERT_IN_RENDER_THREAD();

    const auto color = DGLDrawState::colorFromFloat({r, g, b, a});
    dglDraw.currentVertex.color[0] = color.x;
    dglDraw.currentVertex.color[1] = color.y;
    dglDraw.currentVertex.color[2] = color.z;
    dglDraw.currentVertex.color[3] = color.w;
}

#undef DGL_Color4fv
DENG_EXTERN_C void DGL_Color4fv(float const *vec)
{
    DENG2_ASSERT_IN_RENDER_THREAD();

    const auto color = DGLDrawState::colorFromFloat(vec);
    dglDraw.currentVertex.color[0] = color.x;
    dglDraw.currentVertex.color[1] = color.y;
    dglDraw.currentVertex.color[2] = color.z;
    dglDraw.currentVertex.color[3] = color.w;
}

#undef DGL_TexCoord2f
DENG_EXTERN_C void DGL_TexCoord2f(byte target, float s, float t)
{
    DENG2_ASSERT_IN_RENDER_THREAD();
    DENG2_ASSERT(target < MAX_TEX_COORDS);

    if (target < MAX_TEX_COORDS)
    {
        dglDraw.currentVertex.texCoord[target].s = s;
        dglDraw.currentVertex.texCoord[target].t = t;
    }
}

#undef DGL_TexCoord2fv
DENG_EXTERN_C void DGL_TexCoord2fv(byte target, float const *vec)
{
    DENG2_ASSERT_IN_RENDER_THREAD();
    DENG2_ASSERT(target < MAX_TEX_COORDS);

    if (target < MAX_TEX_COORDS)
    {
        dglDraw.currentVertex.texCoord[target].s = vec[0];
        dglDraw.currentVertex.texCoord[target].t = vec[1];
    }
}

#undef DGL_Vertex2f
DENG_EXTERN_C void DGL_Vertex2f(float x, float y)
{
    DENG2_ASSERT_IN_RENDER_THREAD();

    dglDraw.currentVertex.vertex[0] = x;
    dglDraw.currentVertex.vertex[1] = y;
    dglDraw.currentVertex.vertex[2] = 0.f;
    dglDraw.commitVertex();
}

#undef DGL_Vertex2fv
DENG_EXTERN_C void DGL_Vertex2fv(const float* vec)
{
    DENG2_ASSERT_IN_RENDER_THREAD();

    if (vec)
    {
        dglDraw.currentVertex.vertex[0] = vec[0];
        dglDraw.currentVertex.vertex[1] = vec[1];
        dglDraw.currentVertex.vertex[2] = 0.f;
    }
    dglDraw.commitVertex();
}

#undef DGL_Vertex3f
DENG_EXTERN_C void DGL_Vertex3f(float x, float y, float z)
{
    DENG2_ASSERT_IN_RENDER_THREAD();

    dglDraw.currentVertex.vertex[0] = x;
    dglDraw.currentVertex.vertex[1] = y;
    dglDraw.currentVertex.vertex[2] = z;

    dglDraw.commitVertex();
}

#undef DGL_Vertex3fv
DENG_EXTERN_C void DGL_Vertex3fv(const float* vec)
{
    DENG2_ASSERT_IN_RENDER_THREAD();

    if (vec)
    {
        dglDraw.currentVertex.vertex[0] = vec[0];
        dglDraw.currentVertex.vertex[1] = vec[1];
        dglDraw.currentVertex.vertex[2] = vec[2];
    }
    dglDraw.commitVertex();
}

#undef DGL_Vertices2ftv
DENG_EXTERN_C void DGL_Vertices2ftv(int num, const dgl_ft2vertex_t* vec)
{
    DENG2_ASSERT_IN_RENDER_THREAD();

    for(; num > 0; num--, vec++)
    {
        DGL_TexCoord2fv(0, vec->tex);
        DGL_Vertex2fv(vec->pos);
    }
}

#undef DGL_Vertices3ftv
DENG_EXTERN_C void DGL_Vertices3ftv(int num, const dgl_ft3vertex_t* vec)
{
    DENG2_ASSERT_IN_RENDER_THREAD();
    DENG_ASSERT_GL_CONTEXT_ACTIVE();

    for(; num > 0; num--, vec++)
    {
        DGL_TexCoord2fv(0, vec->tex);
        DGL_Vertex3fv(vec->pos);
    }
}

#undef DGL_Vertices3fctv
DENG_EXTERN_C void DGL_Vertices3fctv(int num, const dgl_fct3vertex_t* vec)
{
    DENG2_ASSERT_IN_RENDER_THREAD();
    DENG_ASSERT_GL_CONTEXT_ACTIVE();

    for(; num > 0; num--, vec++)
    {
        DGL_Color4fv(vec->color);
        DGL_TexCoord2fv(0, vec->tex);
        DGL_Vertex3fv(vec->pos);
    }
}

#undef DGL_Begin
DENG_EXTERN_C void DGL_Begin(dglprimtype_t mode)
{
    if (novideo) return;

    DENG2_ASSERT_IN_RENDER_THREAD();
    DENG_ASSERT_GL_CONTEXT_ACTIVE();

    dglDraw.beginPrimitive(mode);
}

void DGL_AssertNotInPrimitive(void)
{
    DENG_ASSERT(dglDraw.primType == DGL_NO_PRIMITIVE);
}

#undef DGL_End
DENG_EXTERN_C void DGL_End(void)
{
    if (novideo) return;

    DENG2_ASSERT_IN_RENDER_THREAD();
    DENG_ASSERT_GL_CONTEXT_ACTIVE();

    dglDraw.endPrimitive();
}

#undef DGL_DrawLine
DENG_EXTERN_C void DGL_DrawLine(float x1, float y1, float x2, float y2, float r,
    float g, float b, float a)
{
    GL_DrawLine(x1, y1, x2, y2, r, g, b, a);
}

#undef DGL_DrawRect
DENG_EXTERN_C void DGL_DrawRect(RectRaw const *rect)
{
    if (!rect) return;
    GL_DrawRect(Rectanglei::fromSize(Vector2i(rect->origin.xy),
                                     Vector2ui(rect->size.width, rect->size.height)));
}

#undef DGL_DrawRect2
DENG_EXTERN_C void DGL_DrawRect2(int x, int y, int w, int h)
{
    GL_DrawRect2(x, y, w, h);
}

#undef DGL_DrawRectf
DENG_EXTERN_C void DGL_DrawRectf(RectRawf const *rect)
{
    GL_DrawRectf(rect);
}

#undef DGL_DrawRectf2
DENG_EXTERN_C void DGL_DrawRectf2(double x, double y, double w, double h)
{
    GL_DrawRectf2(x, y, w, h);
}

#undef DGL_DrawRectf2Color
DENG_EXTERN_C void DGL_DrawRectf2Color(double x, double y, double w, double h, float r, float g, float b, float a)
{
    DENG_ASSERT_IN_MAIN_THREAD();

    DGL_Color4f(r, g, b, a);
    GL_DrawRectf2(x, y, w, h);
}

#undef DGL_DrawRectf2Tiled
DENG_EXTERN_C void DGL_DrawRectf2Tiled(double x, double y, double w, double h, int tw, int th)
{
    GL_DrawRectf2Tiled(x, y, w, h, tw, th);
}

#undef DGL_DrawCutRectfTiled
DENG_EXTERN_C void DGL_DrawCutRectfTiled(RectRawf const *rect, int tw, int th, int txoff, int tyoff,
    RectRawf const *cutRect)
{
    GL_DrawCutRectfTiled(rect, tw, th, txoff, tyoff, cutRect);
}

#undef DGL_DrawCutRectf2Tiled
DENG_EXTERN_C void DGL_DrawCutRectf2Tiled(double x, double y, double w, double h, int tw, int th,
    int txoff, int tyoff, double cx, double cy, double cw, double ch)
{
    GL_DrawCutRectf2Tiled(x, y, w, h, tw, th, txoff, tyoff, cx, cy, cw, ch);
}

#undef DGL_DrawQuadOutline
DENG_EXTERN_C void DGL_DrawQuadOutline(Point2Raw const *tl, Point2Raw const *tr,
    Point2Raw const *br, Point2Raw const *bl, float const color[4])
{
    if(!tl || !tr || !br || !bl || (color && !(color[CA] > 0))) return;

    DENG_ASSERT_IN_MAIN_THREAD();

    if(color) DGL_Color4fv(color);
    DGL_Begin(DGL_LINE_STRIP);
        DGL_Vertex2f(tl->x, tl->y);
        DGL_Vertex2f(tr->x, tr->y);
        DGL_Vertex2f(br->x, br->y);
        DGL_Vertex2f(bl->x, bl->y);
        DGL_Vertex2f(tl->x, tl->y);
    DGL_End();
}

#undef DGL_DrawQuad2Outline
DENG_EXTERN_C void DGL_DrawQuad2Outline(int tlX, int tlY, int trX, int trY,
    int brX, int brY, int blX, int blY, const float color[4])
{
    Point2Raw tl, tr, bl, br;
    tl.x = tlX;
    tl.y = tlY;
    tr.x = trX;
    tr.y = trY;
    br.x = brX;
    br.y = brY;
    bl.x = blX;
    bl.y = blY;
    DGL_DrawQuadOutline(&tl, &tr, &br, &bl, color);
}
