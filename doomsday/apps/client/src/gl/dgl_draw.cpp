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
#include <de/legacy/concurrency.h>
#include <de/glinfo.h>
#include <de/glbuffer.h>
#include <de/glstate.h>
#include <de/glprogram.h>
#include <de/matrix.h>
#include "sys_system.h"
#include "gl/gl_draw.h"
#include "gl/sys_opengl.h"
#include "clientapp.h"

using namespace de;

static constexpr uint MAX_TEX_COORDS = 2;
static constexpr int  MAX_BATCH      = 16;

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
        NUM_VERTEX_ATTRIB_ARRAYS,
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
    Vertex        primVertices[4];
    List<Vertex>  vertices;

    struct GLData {
        GLProgram shader;

        GLState  batchState;
        Mat4f    batchMvpMatrix[MAX_BATCH];
        Mat4f    batchTexMatrix0[MAX_BATCH];
        Mat4f    batchTexMatrix1[MAX_BATCH];
        int      batchTexEnabled[MAX_BATCH];
        int      batchTexMode[MAX_BATCH];
        Vec4f    batchTexModeColor[MAX_BATCH];
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

        struct DrawBuffer {
            GLuint   vertexArray = 0;
            GLBuffer arrayData;

            void release()
            {
#if defined(DE_HAVE_VAOS)
                glDeleteVertexArrays(1, &vertexArray);
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

        List<DrawBuffer *> buffers;
        dsize bufferPos = 0;
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
            DE_ASSERT(!vertices.empty());
            DE_ASSERT(glPrimitive(batchPrimType) == GL_TRIANGLE_STRIP);

            // When committing multiple triangle strips, add a disconnection
            // between batches.
            vertices.push_back(vertices.back());
            vertices.push_back(currentVertex);
            resetPrimitive = false;
        }
    }

    void commitLine(Vertex start, Vertex end)
    {
        const Vec2f lineDir = (Vec2f(end.vertex) - Vec2f(start.vertex)).normalize();
        const Vec2f lineNormal{-lineDir.y, lineDir.x};

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

    static Vec4ub colorFromFloat(const Vec4f &color)
    {
        Vec4i rgba = (color * 255 + Vec4f(0.5f, 0.5f, 0.5f, 0.5f))
                .toVec4i()
                .max(Vec4i(0, 0, 0, 0))
                .min(Vec4i(255, 255, 255, 255));
        return Vec4ub(dbyte(rgba.x), dbyte(rgba.y), dbyte(rgba.z), dbyte(rgba.w));
    }

    void beginPrimitive(dglprimtype_t primitive)
    {
        glInit();

        DE_ASSERT(primType == DGL_NO_PRIMITIVE);

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
            DE_ASSERT(!vertices.empty());
            endBatch();
        }
        clearPrimitive();
    }

    void getBoundTextures(int &id0, int &id1)
    {
        glActiveTexture(GL_TEXTURE0);
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &id0);
        glActiveTexture(GL_TEXTURE1);
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &id1);
        glActiveTexture(GL_TEXTURE0 + duint(DGL_GetInteger(DGL_ACTIVE_TEXTURE)));
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
        DE_ASSERT_GL_CONTEXT_ACTIVE();

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

            // Sampler uniforms.
            {
                int samplers[2][MAX_BATCH];
                for (int i = 0; i < int(batchMaxSize); ++i)
                {
                    samplers[0][i] = i;
                    samplers[1][i] = int(batchMaxSize) + i;
                }

                auto prog = gl->shader.glName();
                glUseProgram(prog);
                glUniform1iv(glGetUniformLocation(prog, "uTex0[0]"), GLsizei(batchMaxSize), samplers[0]);
                LIBGUI_ASSERT_GL_OK();
                glUniform1iv(glGetUniformLocation(prog, "uTex1[0]"), GLsizei(batchMaxSize), samplers[1]);
                LIBGUI_ASSERT_GL_OK();
                glUseProgram(0);
            }
        }
    }

    void glDeinit()
    {
        if (gl)
        {
            for (GLData::DrawBuffer *dbuf : gl->buffers)
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
#if defined (DE_HAVE_VAOS)
            {
                glGenVertexArrays(1, &dbuf->vertexArray);
                glBindVertexArray(dbuf->vertexArray);
                for (uint i = 0; i < NUM_VERTEX_ATTRIB_ARRAYS; ++i)
                {
                    glEnableVertexAttribArray(i);
                }
                glBindVertexArray(0);
            }
#endif

            gl->buffers.append(dbuf);
        }
        return *gl->buffers[gl->bufferPos++];
    }

    void glBindArrays()
    {
        const uint stride = sizeof(Vertex);

        // Upload the vertex data.
        GLData::DrawBuffer &buf = nextBuffer();
        buf.arrayData.setData(&vertices[0], sizeof(Vertex) * vertices.size(), gfx::Dynamic);

#if defined (DE_HAVE_VAOS)
        glBindVertexArray(buf.vertexArray);
#else
        for (uint i = 0; i < NUM_VERTEX_ATTRIB_ARRAYS; ++i)
        {
            glEnableVertexAttribArray(i);
        }
#endif
        LIBGUI_ASSERT_GL_OK();

        glBindBuffer(GL_ARRAY_BUFFER, buf.arrayData.glName());
        LIBGUI_ASSERT_GL_OK();

        DE_ASSERT(glGetAttribLocation(gl->shader.glName(), "aVertex") == VAA_VERTEX);
        DE_ASSERT(glGetAttribLocation(gl->shader.glName(), "aColor") == VAA_COLOR);
        DE_ASSERT(glGetAttribLocation(gl->shader.glName(), "aTexCoord") == VAA_TEXCOORD0);
        DE_ASSERT(glGetAttribLocation(gl->shader.glName(), "aFragOffset") == VAA_FRAG_OFFSET);
        DE_ASSERT(glGetAttribLocation(gl->shader.glName(), "aBatchIndex") == VAA_BATCH_INDEX);

        // Updated pointers.
        glVertexAttribPointer(VAA_VERTEX,      3, GL_FLOAT,         GL_FALSE, stride, DENG2_OFFSET_PTR(Vertex, vertex));
        glVertexAttribPointer(VAA_COLOR,       4, GL_UNSIGNED_BYTE, GL_TRUE,  stride, DENG2_OFFSET_PTR(Vertex, color));
        glVertexAttribPointer(VAA_TEXCOORD0,   2, GL_FLOAT,         GL_FALSE, stride, DENG2_OFFSET_PTR(Vertex, texCoord[0]));
        glVertexAttribPointer(VAA_TEXCOORD1,   2, GL_FLOAT,         GL_FALSE, stride, DENG2_OFFSET_PTR(Vertex, texCoord[1]));
        glVertexAttribPointer(VAA_FRAG_OFFSET, 2, GL_FLOAT,         GL_FALSE, stride, DENG2_OFFSET_PTR(Vertex, fragOffset[0]));
        glVertexAttribPointer(VAA_BATCH_INDEX, 1, GL_FLOAT,         GL_FALSE, stride, DENG2_OFFSET_PTR(Vertex, batchIndex));
        LIBGUI_ASSERT_GL_OK();

        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    void glUnbindArrays()
    {
#if defined (DE_HAVE_VAOS)
        glBindVertexArray(0);
#else
        for (uint i = 0; i < NUM_VERTEX_ATTRIB_ARRAYS; ++i)
        {
            glDisableVertexAttribArray(i);
            LIBGUI_ASSERT_GL_OK();
        }
#endif
    }

    void glBindBatchTextures(duint count)
    {
        for (duint i = 0; i < count; ++i)
        {
            glActiveTexture(GLenum(GL_TEXTURE0 + i));
            glBindTexture(GL_TEXTURE_2D, GLuint(gl->batchTexture0[i]));
            glActiveTexture(GLenum(GL_TEXTURE0 + batchMaxSize + i));
            glBindTexture(GL_TEXTURE_2D, GLuint(gl->batchTexture1[i]));
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
        case DGL_NO_PRIMITIVE:      break;
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
            gl->uFragmentSize     = Vec2f(lineWidth, lineWidth) / glState.target().size();
        }
        else
        {
            gl->uFragmentSize = Vec2f();
        }
        DGL_FogParams(gl->uFogRange, gl->uFogColor);

        glState.apply();

        int oldTex[2];
        getBoundTextures(oldTex[0], oldTex[1]);

        glBindArrays();
        gl->shader.beginUse();
        glBindBatchTextures(batchLength);
        DE_ASSERT(gl->shader.validate());
        glDrawArrays(glPrimitive(batchPrimType), 0, numVertices()); ++s_drawCallCount;
        gl->shader.endUse();
        LIBGUI_ASSERT_GL_OK();
        glUnbindArrays();

        // Restore the previously bound OpenGL textures.
        {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, GLuint(oldTex[0]));
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, GLuint(oldTex[1]));
            glActiveTexture(GL_TEXTURE0 + duint(DGL_GetInteger(DGL_ACTIVE_TEXTURE)));
        }
    }
};

static DGLDrawState dglDraw;

unsigned int DGL_BatchMaxSize()
{
    // This determines how long DGL batch draws can be.
    int maxFragSamplers;
    glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &maxFragSamplers);
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
    Vec4f colorf = Vec4ub(dglDraw.currentVertex.color).toVec4f() / 255.0;
    std::memcpy(rgba, colorf.constPtr(), sizeof(float) * 4);
}

#undef DGL_Color3ub
DE_EXTERN_C void DGL_Color3ub(DGLubyte r, DGLubyte g, DGLubyte b)
{
    DE_ASSERT_IN_RENDER_THREAD();

    dglDraw.currentVertex.color[0] = r;
    dglDraw.currentVertex.color[1] = g;
    dglDraw.currentVertex.color[2] = b;
    dglDraw.currentVertex.color[3] = 255;
}

#undef DGL_Color3ubv
DE_EXTERN_C void DGL_Color3ubv(const DGLubyte *vec)
{
    DE_ASSERT_IN_RENDER_THREAD();

    dglDraw.currentVertex.color[0] = vec[0];
    dglDraw.currentVertex.color[1] = vec[1];
    dglDraw.currentVertex.color[2] = vec[2];
    dglDraw.currentVertex.color[3] = 255;
}

#undef DGL_Color4ub
DE_EXTERN_C void DGL_Color4ub(DGLubyte r, DGLubyte g, DGLubyte b, DGLubyte a)
{
    DE_ASSERT_IN_RENDER_THREAD();

    dglDraw.currentVertex.color[0] = r;
    dglDraw.currentVertex.color[1] = g;
    dglDraw.currentVertex.color[2] = b;
    dglDraw.currentVertex.color[3] = a;
}

#undef DGL_Color4ubv
DE_EXTERN_C void DGL_Color4ubv(const DGLubyte *vec)
{
    DE_ASSERT_IN_RENDER_THREAD();

    dglDraw.currentVertex.color[0] = vec[0];
    dglDraw.currentVertex.color[1] = vec[1];
    dglDraw.currentVertex.color[2] = vec[2];
    dglDraw.currentVertex.color[3] = vec[3];
}

#undef DGL_Color3f
DE_EXTERN_C void DGL_Color3f(float r, float g, float b)
{
    DE_ASSERT_IN_RENDER_THREAD();

    const auto color = DGLDrawState::colorFromFloat({r, g, b, 1.f});
    dglDraw.currentVertex.color[0] = color.x;
    dglDraw.currentVertex.color[1] = color.y;
    dglDraw.currentVertex.color[2] = color.z;
    dglDraw.currentVertex.color[3] = color.w;
}

#undef DGL_Color3fv
DE_EXTERN_C void DGL_Color3fv(const float *vec)
{
    DE_ASSERT_IN_RENDER_THREAD();

    const auto color = DGLDrawState::colorFromFloat({Vec3f(vec), 1.f});
    dglDraw.currentVertex.color[0] = color.x;
    dglDraw.currentVertex.color[1] = color.y;
    dglDraw.currentVertex.color[2] = color.z;
    dglDraw.currentVertex.color[3] = color.w;
}

#undef DGL_Color4f
DE_EXTERN_C void DGL_Color4f(float r, float g, float b, float a)
{
    DE_ASSERT_IN_RENDER_THREAD();

    const auto color = DGLDrawState::colorFromFloat({r, g, b, a});
    dglDraw.currentVertex.color[0] = color.x;
    dglDraw.currentVertex.color[1] = color.y;
    dglDraw.currentVertex.color[2] = color.z;
    dglDraw.currentVertex.color[3] = color.w;
}

#undef DGL_Color4fv
DE_EXTERN_C void DGL_Color4fv(const float *vec)
{
    DE_ASSERT_IN_RENDER_THREAD();

    const auto color = DGLDrawState::colorFromFloat(Vec4f(vec));
    dglDraw.currentVertex.color[0] = color.x;
    dglDraw.currentVertex.color[1] = color.y;
    dglDraw.currentVertex.color[2] = color.z;
    dglDraw.currentVertex.color[3] = color.w;
}

#undef DGL_TexCoord2f
DE_EXTERN_C void DGL_TexCoord2f(byte target, float s, float t)
{
    DE_ASSERT_IN_RENDER_THREAD();
    DE_ASSERT(target < MAX_TEX_COORDS);

    if (target < MAX_TEX_COORDS)
    {
        dglDraw.currentVertex.texCoord[target].s = s;
        dglDraw.currentVertex.texCoord[target].t = t;
    }
}

#undef DGL_TexCoord2fv
DE_EXTERN_C void DGL_TexCoord2fv(byte target, const float *vec)
{
    DE_ASSERT_IN_RENDER_THREAD();
    DE_ASSERT(target < MAX_TEX_COORDS);

    if (target < MAX_TEX_COORDS)
    {
        dglDraw.currentVertex.texCoord[target].s = vec[0];
        dglDraw.currentVertex.texCoord[target].t = vec[1];
    }
}

#undef DGL_Vertex2f
DE_EXTERN_C void DGL_Vertex2f(float x, float y)
{
    DE_ASSERT_IN_RENDER_THREAD();

    dglDraw.currentVertex.vertex[0] = x;
    dglDraw.currentVertex.vertex[1] = y;
    dglDraw.currentVertex.vertex[2] = 0.f;
    dglDraw.commitVertex();
}

#undef DGL_Vertex2fv
DE_EXTERN_C void DGL_Vertex2fv(const float* vec)
{
    DE_ASSERT_IN_RENDER_THREAD();

    if (vec)
    {
        dglDraw.currentVertex.vertex[0] = vec[0];
        dglDraw.currentVertex.vertex[1] = vec[1];
        dglDraw.currentVertex.vertex[2] = 0.f;
    }
    dglDraw.commitVertex();
}

#undef DGL_Vertex3f
DE_EXTERN_C void DGL_Vertex3f(float x, float y, float z)
{
    DE_ASSERT_IN_RENDER_THREAD();

    dglDraw.currentVertex.vertex[0] = x;
    dglDraw.currentVertex.vertex[1] = y;
    dglDraw.currentVertex.vertex[2] = z;

    dglDraw.commitVertex();
}

#undef DGL_Vertex3fv
DE_EXTERN_C void DGL_Vertex3fv(const float* vec)
{
    DE_ASSERT_IN_RENDER_THREAD();

    if (vec)
    {
        dglDraw.currentVertex.vertex[0] = vec[0];
        dglDraw.currentVertex.vertex[1] = vec[1];
        dglDraw.currentVertex.vertex[2] = vec[2];
    }
    dglDraw.commitVertex();
}

#undef DGL_Vertices2ftv
DE_EXTERN_C void DGL_Vertices2ftv(int num, const dgl_ft2vertex_t* vec)
{
    DE_ASSERT_IN_RENDER_THREAD();

    for(; num > 0; num--, vec++)
    {
        DGL_TexCoord2fv(0, vec->tex);
        DGL_Vertex2fv(vec->pos);
    }
}

#undef DGL_Vertices3ftv
DE_EXTERN_C void DGL_Vertices3ftv(int num, const dgl_ft3vertex_t* vec)
{
    DE_ASSERT_IN_RENDER_THREAD();
    DE_ASSERT_GL_CONTEXT_ACTIVE();

    for(; num > 0; num--, vec++)
    {
        DGL_TexCoord2fv(0, vec->tex);
        DGL_Vertex3fv(vec->pos);
    }
}

#undef DGL_Vertices3fctv
DE_EXTERN_C void DGL_Vertices3fctv(int num, const dgl_fct3vertex_t* vec)
{
    DE_ASSERT_IN_RENDER_THREAD();
    DE_ASSERT_GL_CONTEXT_ACTIVE();

    for(; num > 0; num--, vec++)
    {
        DGL_Color4fv(vec->color);
        DGL_TexCoord2fv(0, vec->tex);
        DGL_Vertex3fv(vec->pos);
    }
}

#undef DGL_Begin
DE_EXTERN_C void DGL_Begin(dglprimtype_t mode)
{
    if (novideo) return;

    DE_ASSERT_IN_RENDER_THREAD();
    DE_ASSERT_GL_CONTEXT_ACTIVE();

    dglDraw.beginPrimitive(mode);
}

void DGL_AssertNotInPrimitive(void)
{
    DE_ASSERT(dglDraw.primType == DGL_NO_PRIMITIVE);
}

#undef DGL_End
DE_EXTERN_C void DGL_End(void)
{
    if (novideo) return;

    DE_ASSERT_IN_RENDER_THREAD();
    DE_ASSERT_GL_CONTEXT_ACTIVE();

    dglDraw.endPrimitive();
}

#undef DGL_DrawLine
DE_EXTERN_C void DGL_DrawLine(float x1, float y1, float x2, float y2, float r,
    float g, float b, float a)
{
    GL_DrawLine(x1, y1, x2, y2, r, g, b, a);
}

#undef DGL_DrawRect
DE_EXTERN_C void DGL_DrawRect(const RectRaw *rect)
{
    if (!rect) return;
    GL_DrawRect(Rectanglei::fromSize(Vec2i(rect->origin.xy),
                                     Vec2ui(rect->size.width, rect->size.height)));
}

#undef DGL_DrawRect2
DE_EXTERN_C void DGL_DrawRect2(int x, int y, int w, int h)
{
    GL_DrawRect2(x, y, w, h);
}

#undef DGL_DrawRectf
DE_EXTERN_C void DGL_DrawRectf(const RectRawf *rect)
{
    GL_DrawRectf(rect);
}

#undef DGL_DrawRectf2
DE_EXTERN_C void DGL_DrawRectf2(double x, double y, double w, double h)
{
    GL_DrawRectf2(x, y, w, h);
}

#undef DGL_DrawRectf2Color
DE_EXTERN_C void DGL_DrawRectf2Color(double x, double y, double w, double h, float r, float g, float b, float a)
{
    DE_ASSERT_IN_MAIN_THREAD();

    DGL_Color4f(r, g, b, a);
    GL_DrawRectf2(x, y, w, h);
}

#undef DGL_DrawRectf2Tiled
DE_EXTERN_C void DGL_DrawRectf2Tiled(double x, double y, double w, double h, int tw, int th)
{
    GL_DrawRectf2Tiled(x, y, w, h, tw, th);
}

#undef DGL_DrawCutRectfTiled
DE_EXTERN_C void DGL_DrawCutRectfTiled(const RectRawf *rect, int tw, int th, int txoff, int tyoff,
    const RectRawf *cutRect)
{
    GL_DrawCutRectfTiled(rect, tw, th, txoff, tyoff, cutRect);
}

#undef DGL_DrawCutRectf2Tiled
DE_EXTERN_C void DGL_DrawCutRectf2Tiled(double x, double y, double w, double h, int tw, int th,
    int txoff, int tyoff, double cx, double cy, double cw, double ch)
{
    GL_DrawCutRectf2Tiled(x, y, w, h, tw, th, txoff, tyoff, cx, cy, cw, ch);
}

#undef DGL_DrawQuadOutline
DE_EXTERN_C void DGL_DrawQuadOutline(const Point2Raw *tl, const Point2Raw *tr,
    const Point2Raw *br, const Point2Raw *bl, float const color[4])
{
    if(!tl || !tr || !br || !bl || (color && !(color[CA] > 0))) return;

    DE_ASSERT_IN_MAIN_THREAD();

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
DE_EXTERN_C void DGL_DrawQuad2Outline(int tlX, int tlY, int trX, int trY,
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
