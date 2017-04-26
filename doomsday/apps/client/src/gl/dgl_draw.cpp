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

uint constexpr MAX_TEX_COORDS = 2;

struct DGLDrawState
{
    struct Vertex
    {
        Vector3f  vertex;
        Vector4ub color { 255, 255, 255, 255 };
        Vector2f  texCoord[MAX_TEX_COORDS];

        Vertex() {}

        Vertex(Vertex const &other)
        {
            std::memcpy(this, &other, sizeof(Vertex));
        }

        Vertex &operator = (Vertex const &other)
        {
            std::memcpy(this, &other, sizeof(Vertex));
            return *this;
        }
    };

    // Indices for vertex attribute arrays.
    enum
    {
        VAA_VERTEX,
        VAA_COLOR,
        VAA_TEXCOORD0,
        VAA_TEXCOORD1,
        NUM_VERTEX_ATTRIB_ARRAYS
    };

    dglprimtype_t primType = DGL_NO_PRIMITIVE;
    int primIndex = 0;
    QVector<Vertex> vertices;

    struct GLData
    {
        GLProgram shader;
        GLUniform uMvpMatrix    { "uMvpMatrix",    GLUniform::Mat4  };
        GLUniform uTexMatrix0   { "uTexMatrix0",   GLUniform::Mat4  };
        GLUniform uTexMatrix1   { "uTexMatrix1",   GLUniform::Mat4  };
        GLUniform uTexEnabled   { "uTexEnabled",   GLUniform::Int   };
        GLUniform uTexMode      { "uTexMode",      GLUniform::Int   };
        GLUniform uTexModeColor { "uTexModeColor", GLUniform::Vec4  };
        GLUniform uAlphaLimit   { "uAlphaLimit",   GLUniform::Float };
        GLUniform uFogRange     { "uFogRange",     GLUniform::Vec4  };
        GLUniform uFogColor     { "uFogColor",     GLUniform::Vec4  };
        struct DrawBuffer
        {
            GLuint vertexArray = 0;
            GLBuffer arrayData;

            void release()
            {
                LIBGUI_GL.glDeleteVertexArrays(1, &vertexArray);
                arrayData.clear();
            }
        };
        QVector<DrawBuffer *> buffers;
        int bufferPos = 0;
    };
    std::unique_ptr<GLData> gl;

    DGLDrawState()
    {
        clearVertices();
    }

    void commitVertex()
    {
        vertices.append(vertices.last());
        ++primIndex;

        if (primType == DGL_QUADS)
        {
            if (primIndex == 4)
            {
                // 4 vertices become 6.
                //
                // 0--1     0--1   5
                // |  |      \ |   |\
                // |  |  =>   \|   | \
                // 3--2        2   4--3

                vertices.append(vertices.last());
                vertices.append(vertices.last());

                // 0 1 2  3 3 3  X
                int const N = vertices.size();
                vertices[N - 4] = vertices[N - 5];
                vertices[N - 2] = vertices[N - 7];

                primIndex = 0;
            }
        }
    }

    void clearVertices()
    {
        Vertex const last = (vertices.isEmpty()? Vertex() : vertices.last());
        vertices.clear();
        vertices.append(last);
        primIndex = 0;
        primType  = DGL_NO_PRIMITIVE;
    }

    int numVertices() const
    {
        // The last one is always the incomplete one.
        return vertices.size() - 1;
    }

    Vertex &vertex()
    {
        return vertices.last();
    }

    static Vector4ub colorFromFloat(Vector4f const &color)
    {
        Vector4i rgba = (color * 255 + Vector4f(0.5f, 0.5f, 0.5f, 0.5f))
                .toVector4i()
                .max(Vector4i(0, 0, 0, 0))
                .min(Vector4i(255, 255, 255, 255));
        return Vector4ub(dbyte(rgba.x), dbyte(rgba.y), dbyte(rgba.z), dbyte(rgba.w));
    }

    void beginPrimitive(dglprimtype_t primitive)
    {
        DENG2_ASSERT(primType == DGL_NO_PRIMITIVE);

        // We enter a Begin/End section.
        primType = primitive;
    }

    void endPrimitive()
    {
        if (primType != DGL_NO_PRIMITIVE)
        {
            drawPrimitives();
        }
        clearVertices();
    }

    void glInit()
    {
        DENG_ASSERT_GL_CONTEXT_ACTIVE();

        if (!gl)
        {
            gl.reset(new GLData);

            // Set up the shader.
            ClientApp::shaders().build(gl->shader, "dgl.draw")
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
                auto prog = gl->shader.glName();
                GL.glUseProgram(prog);
                GL.glUniform1i(GL.glGetUniformLocation(prog, "uTex0"), 0);
                GL.glUniform1i(GL.glGetUniformLocation(prog, "uTex1"), 1);
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

            gl->buffers.append(dbuf);
        }
        return *gl->buffers[gl->bufferPos++];
    }

    void glBindArrays()
    {
        uint const stride = sizeof(Vertex);
        auto &GL = LIBGUI_GL;

        // Upload the vertex data.
        GLData::DrawBuffer &buf = nextBuffer();
        buf.arrayData.setData(&vertices[0], sizeof(Vertex) * vertices.size(), gl::Dynamic);

        GL.glBindVertexArray(buf.vertexArray);
        LIBGUI_ASSERT_GL_OK();

        GL.glBindBuffer(GL_ARRAY_BUFFER, buf.arrayData.glName());
        LIBGUI_ASSERT_GL_OK();

        Vertex const *basePtr = nullptr;

        // Updated pointers.
        GL.glVertexAttribPointer(VAA_VERTEX,    3, GL_FLOAT,         GL_FALSE, stride, &basePtr->vertex);
        GL.glVertexAttribPointer(VAA_COLOR,     4, GL_UNSIGNED_BYTE, GL_TRUE,  stride, &basePtr->color);
        GL.glVertexAttribPointer(VAA_TEXCOORD0, 2, GL_FLOAT,         GL_FALSE, stride, &basePtr->texCoord[0]);
        GL.glVertexAttribPointer(VAA_TEXCOORD1, 2, GL_FLOAT,         GL_FALSE, stride, &basePtr->texCoord[1]);
        LIBGUI_ASSERT_GL_OK();

        GL.glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    void glUnbindArrays()
    {
        LIBGUI_GL.glBindVertexArray(0);
        LIBGUI_ASSERT_GL_OK();
    }

    GLenum glPrimitive() const
    {
        switch (primType)
        {
        case DGL_POINTS:            return GL_POINTS;
        case DGL_LINES:             return GL_LINES;
        case DGL_LINE_LOOP:         return GL_LINE_LOOP;
        case DGL_LINE_STRIP:        return GL_LINE_STRIP;
        case DGL_TRIANGLES:         return GL_TRIANGLES;
        case DGL_TRIANGLE_FAN:      return GL_TRIANGLE_FAN;
        case DGL_TRIANGLE_STRIP:    return GL_TRIANGLE_STRIP;
        case DGL_QUADS:             return GL_TRIANGLES; // converted

        case DGL_NO_PRIMITIVE:      DENG2_ASSERT(!"No primitive type specified"); break;
        }
        return GL_NONE;
    }

    /**
     * Draws all the primitives currently stored in the vertex array.
     */
    void drawPrimitives()
    {
        glInit();

        GLState const &glState = GLState::current();

        // Update uniforms.
        gl->uMvpMatrix    = DGL_Matrix(DGL_PROJECTION) * DGL_Matrix(DGL_MODELVIEW);
        gl->uTexMatrix0   = DGL_Matrix(DGL_TEXTURE0);
        gl->uTexMatrix1   = DGL_Matrix(DGL_TEXTURE1);
        gl->uTexEnabled   = (DGL_GetInteger(DGL_TEXTURE0)? 0x1 : 0) |
                            (DGL_GetInteger(DGL_TEXTURE1)? 0x2 : 0);
        gl->uTexMode      = DGL_GetInteger(DGL_MODULATE_TEXTURE);
        gl->uTexModeColor = DGL_ModulationColor();
        gl->uAlphaLimit   = (glState.alphaTest()? glState.alphaLimit() : 0.f);
        DGL_FogParams(gl->uFogRange, gl->uFogColor);

        GLState::current().apply();

        gl->shader.beginUse();
        {
            glBindArrays();
            LIBGUI_GL.glDrawArrays(glPrimitive(), 0, numVertices());
            LIBGUI_ASSERT_GL_OK();
            glUnbindArrays();
        }
        gl->shader.endUse();
    }
};

static DGLDrawState dglDraw;

void DGL_Shutdown()
{
    dglDraw.glDeinit();
}

void DGL_BeginFrame()
{
    if (dglDraw.gl)
    {
        // Reuse buffers every frame.
        dglDraw.gl->bufferPos = 0;
    }
}

void DGL_CurrentColor(DGLubyte *rgba)
{
    std::memcpy(rgba, dglDraw.vertex().color.constPtr(), 4);
}

void DGL_CurrentColor(float *rgba)
{
    Vector4f colorf = Vector4ub(dglDraw.vertex().color).toVector4f() / 255.0;
    std::memcpy(rgba, colorf.constPtr(), sizeof(float) * 4);
}

#undef DGL_Color3ub
DENG_EXTERN_C void DGL_Color3ub(DGLubyte r, DGLubyte g, DGLubyte b)
{
    DENG_ASSERT_IN_MAIN_THREAD();

    dglDraw.vertex().color = Vector4ub(r, g, b, 255);
}

#undef DGL_Color3ubv
DENG_EXTERN_C void DGL_Color3ubv(DGLubyte const *vec)
{
    DENG_ASSERT_IN_MAIN_THREAD();

    dglDraw.vertex().color = Vector4ub(Vector3ub(vec), 255);
}

#undef DGL_Color4ub
DENG_EXTERN_C void DGL_Color4ub(DGLubyte r, DGLubyte g, DGLubyte b, DGLubyte a)
{
    DENG_ASSERT_IN_MAIN_THREAD();

    dglDraw.vertex().color = Vector4ub(r, g, b, a);
}

#undef DGL_Color4ubv
DENG_EXTERN_C void DGL_Color4ubv(DGLubyte const *vec)
{
    DENG_ASSERT_IN_MAIN_THREAD();

    dglDraw.vertex().color = Vector4ub(vec);
}

#undef DGL_Color3f
DENG_EXTERN_C void DGL_Color3f(float r, float g, float b)
{
    DENG_ASSERT_IN_MAIN_THREAD();

    dglDraw.vertex().color = DGLDrawState::colorFromFloat(Vector4f(r, g, b, 1.f));
}

#undef DGL_Color3fv
DENG_EXTERN_C void DGL_Color3fv(float const *vec)
{
    DENG_ASSERT_IN_MAIN_THREAD();

    dglDraw.vertex().color = DGLDrawState::colorFromFloat(Vector4f(Vector3f(vec), 1.f));
}

#undef DGL_Color4f
DENG_EXTERN_C void DGL_Color4f(float r, float g, float b, float a)
{
    DENG_ASSERT_IN_MAIN_THREAD();

    dglDraw.vertex().color = DGLDrawState::colorFromFloat(Vector4f(r, g, b, a));
}

#undef DGL_Color4fv
DENG_EXTERN_C void DGL_Color4fv(float const *vec)
{
    DENG_ASSERT_IN_MAIN_THREAD();

    dglDraw.vertex().color = DGLDrawState::colorFromFloat(Vector4f(vec));
}

#undef DGL_TexCoord2f
DENG_EXTERN_C void DGL_TexCoord2f(byte target, float s, float t)
{
    DENG_ASSERT_IN_MAIN_THREAD();
    DENG2_ASSERT(target < MAX_TEX_COORDS);

    if (target < MAX_TEX_COORDS)
    {
        dglDraw.vertex().texCoord[target] = Vector2f(s, t);
    }
}

#undef DGL_TexCoord2fv
DENG_EXTERN_C void DGL_TexCoord2fv(byte target, float const *vec)
{
    DENG_ASSERT_IN_MAIN_THREAD();
    DENG2_ASSERT(target < MAX_TEX_COORDS);

    if (target < MAX_TEX_COORDS)
    {
        dglDraw.vertex().texCoord[target] = Vector2f(vec);
    }
}

#undef DGL_Vertex2f
DENG_EXTERN_C void DGL_Vertex2f(float x, float y)
{
    DENG_ASSERT_IN_MAIN_THREAD();

    dglDraw.vertex().vertex = Vector3f(x, y, 0.f);
    dglDraw.commitVertex();
}

#undef DGL_Vertex2fv
DENG_EXTERN_C void DGL_Vertex2fv(const float* vec)
{
    DENG_ASSERT_IN_MAIN_THREAD();

    if (vec)
    {
        dglDraw.vertex().vertex = Vector3f(vec[0], vec[1], 0.f);
    }
    dglDraw.commitVertex();
}

#undef DGL_Vertex3f
DENG_EXTERN_C void DGL_Vertex3f(float x, float y, float z)
{
    DENG_ASSERT_IN_MAIN_THREAD();

    dglDraw.vertex().vertex = Vector3f(x, y, z);

    dglDraw.commitVertex();
}

#undef DGL_Vertex3fv
DENG_EXTERN_C void DGL_Vertex3fv(const float* vec)
{
    DENG_ASSERT_IN_MAIN_THREAD();

    if (vec)
    {
        dglDraw.vertex().vertex = Vector3f(vec);
    }
    dglDraw.commitVertex();
}

#undef DGL_Vertices2ftv
DENG_EXTERN_C void DGL_Vertices2ftv(int num, const dgl_ft2vertex_t* vec)
{
    DENG_ASSERT_IN_MAIN_THREAD();

    for(; num > 0; num--, vec++)
    {
        DGL_TexCoord2fv(0, vec->tex);
        DGL_Vertex2fv(vec->pos);
    }
}

#undef DGL_Vertices3ftv
DENG_EXTERN_C void DGL_Vertices3ftv(int num, const dgl_ft3vertex_t* vec)
{
    DENG_ASSERT_IN_MAIN_THREAD();
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
    DENG_ASSERT_IN_MAIN_THREAD();
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

    DENG_ASSERT_IN_MAIN_THREAD();
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

    DENG_ASSERT_IN_MAIN_THREAD();
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
