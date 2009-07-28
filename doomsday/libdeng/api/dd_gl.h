/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2007-2009 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 2006 Jamie Jones <jamie_jones_au@yahoo.com.au>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

/**
 * dd_gl.h: Doomsday graphics library.
 */

#ifndef __DOOMSDAY_GL_H__
#define __DOOMSDAY_GL_H__

#include "dd_export.h"

enum {
    // Values
    DGL_SCISSOR_BOX,
    DGL_ACTIVE_TEXTURE,

    DGL_CURRENT_COLOR_R,
    DGL_CURRENT_COLOR_G,
    DGL_CURRENT_COLOR_B,
    DGL_CURRENT_COLOR_A,
    DGL_CURRENT_COLOR_RGBA,

    // List modes
    DGL_COMPILE = 0x3000,
    DGL_COMPILE_AND_EXECUTE,

    // Matrices
    DGL_MODELVIEW = 0x4000,
    DGL_PROJECTION,
    DGL_TEXTURE,

    // Caps
    DGL_TEXTURING = 0x5000,
    DGL_SCISSOR_TEST,
    DGL_FOG,
    DGL_MODULATE_ADD_COMBINE,
    DGL_MODULATE_TEXTURE,
    DGL_LINE_SMOOTH,
    DGL_POINT_SMOOTH,

    // Blending functions
    DGL_ZERO = 0x6000,
    DGL_ONE,
    DGL_DST_COLOR,
    DGL_ONE_MINUS_DST_COLOR,
    DGL_DST_ALPHA,
    DGL_ONE_MINUS_DST_ALPHA,
    DGL_SRC_COLOR,
    DGL_ONE_MINUS_SRC_COLOR,
    DGL_SRC_ALPHA,
    DGL_ONE_MINUS_SRC_ALPHA,
    DGL_SRC_ALPHA_SATURATE,
    DGL_ADD,
    DGL_SUBTRACT,
    DGL_REVERSE_SUBTRACT,

    // Miscellaneous
    DGL_MIN_FILTER = 0xF000,
    DGL_MAG_FILTER,
    DGL_ANISO_FILTER,
    DGL_NEAREST,
    DGL_LINEAR,
    DGL_NEAREST_MIPMAP_NEAREST,
    DGL_LINEAR_MIPMAP_NEAREST,
    DGL_NEAREST_MIPMAP_LINEAR,
    DGL_LINEAR_MIPMAP_LINEAR,
    DGL_CLAMP,
    DGL_CLAMP_TO_EDGE,
    DGL_REPEAT,
    DGL_LINE_WIDTH,
    DGL_POINT_SIZE
};

// Types.
typedef unsigned char DGLubyte;
typedef unsigned int DGLuint;
typedef int DGLsizei;

// Texture formats:
typedef enum dgltexformat_e {
    DGL_RGB,
    DGL_RGBA,
    DGL_COLOR_INDEX_8,
    DGL_COLOR_INDEX_8_PLUS_A8,
    DGL_LUMINANCE,
    DGL_DEPTH_COMPONENT,
    DGL_LUMINANCE_PLUS_A8
} dgltexformat_t;

typedef enum dglprimtype_e {
    DGL_LINES,
    DGL_TRIANGLES,
    DGL_TRIANGLE_FAN,
    DGL_TRIANGLE_STRIP,
    DGL_QUADS,
    DGL_QUAD_STRIP,
    DGL_POINTS
} dglprimtype_t;

#define DDNUM_BLENDMODES    9

typedef enum blendmode_e {
    BM_ZEROALPHA = -1,
    BM_NORMAL,
    BM_ADD,
    BM_DARK,
    BM_SUBTRACT,
    BM_REVERSE_SUBTRACT,
    BM_MUL,
    BM_INVERSE,
    BM_INVERSE_MUL,
    BM_ALPHA_SUBTRACT
} blendmode_t;

typedef struct dgl_vertex_s {
    float           xyz[4]; // The fourth is padding.
} dgl_vertex_t;

typedef struct dgl_texcoord_s {
    float           st[2];
} dgl_texcoord_t;

typedef struct dgl_color_s {
    byte            rgba[4];
} dgl_color_t;

typedef struct {
    DGLubyte        rgb[3];
} dgl_rgb_t;

typedef struct {
    DGLubyte        rgba[4];
} dgl_rgba_t;

// A 2-vertex with texture coordinates, using floats
typedef struct {
    float           pos[2];
    float           tex[2];
} dgl_ft2vertex_t;

// A 3-vertex with texture coordinates, using floats
typedef struct {
    float           pos[3];
    float           tex[2];
} dgl_ft3vertex_t;

// A 3-vertex with texture coordinates and a color, using floats
typedef struct {
    float           pos[3];
    float           tex[2];
    float           color[4];
} dgl_fct3vertex_t;

// A colored 3-vertex, using floats
typedef struct {
    float           pos[3];
    float           color[4];
} dgl_fc3vertex_t;

DENG_API int             DGL_Enable(int cap);
DENG_API void            DGL_Disable(int cap);

DENG_API boolean         DGL_GetIntegerv(int name, int* vec);
DENG_API int             DGL_GetInteger(int name);
DENG_API boolean         DGL_SetInteger(int name, int value);
DENG_API float           DGL_GetFloat(int name);
DENG_API boolean         DGL_SetFloat(int name, float value);

DENG_API void            DGL_Ortho(float left, float top, float right, float bottom, float znear, float zfar);
DENG_API void            DGL_Scissor(int x, int y, int width, int height);

DENG_API void            DGL_MatrixMode(int mode);
DENG_API void            DGL_PushMatrix(void);
DENG_API void            DGL_PopMatrix(void);
DENG_API void            DGL_LoadIdentity(void);

DENG_API void            DGL_Translatef(float x, float y, float z);
DENG_API void            DGL_Rotatef(float angle, float x, float y, float z);
DENG_API void            DGL_Scalef(float x, float y, float z);

DENG_API void            DGL_Begin(dglprimtype_t type);
DENG_API void            DGL_End(void);
DENG_API boolean         DGL_NewList(DGLuint list, int mode);
DENG_API DGLuint         DGL_EndList(void);
DENG_API void            DGL_CallList(DGLuint list);
DENG_API void            DGL_DeleteLists(DGLuint list, int range);

DENG_API void            DGL_SetMaterial(struct material_s* mat);
DENG_API void            DGL_SetNoMaterial(void);
DENG_API void            DGL_SetPatch(lumpnum_t lump, int wrapS, int wrapT);
DENG_API void            DGL_SetPSprite(struct material_s* mat);
DENG_API void            DGL_SetTranslatedSprite(struct material_s* mat, int tclass, int tmap);
DENG_API void            DGL_SetRawImage(lumpnum_t lump, boolean part2, int wrapS, int wrapT);

DENG_API void            DGL_BlendOp(int op);
DENG_API void            DGL_BlendFunc(int param1, int param2);
DENG_API void            DGL_BlendMode(blendmode_t mode);

DENG_API void            DGL_Color3ub(DGLubyte r, DGLubyte g, DGLubyte b);
DENG_API void            DGL_Color3ubv(const DGLubyte* vec);
DENG_API void            DGL_Color4ub(DGLubyte r, DGLubyte g, DGLubyte b, DGLubyte a);
DENG_API void            DGL_Color4ubv(const DGLubyte* vec);
DENG_API void            DGL_Color3f(float r, float g, float b);
DENG_API void            DGL_Color3fv(const float* vec);
DENG_API void            DGL_Color4f(float r, float g, float b, float a);
DENG_API void            DGL_Color4fv(const float* vec);

DENG_API void            DGL_TexCoord2f(byte target, float s, float t);
DENG_API void            DGL_TexCoord2fv(byte target, float* vec);

DENG_API void            DGL_Vertex2f(float x, float y);
DENG_API void            DGL_Vertex2fv(const float* vec);
DENG_API void            DGL_Vertex3f(float x, float y, float z);
DENG_API void            DGL_Vertex3fv(const float* vec);
DENG_API void            DGL_Vertices2ftv(int num, const dgl_ft2vertex_t* vec);
DENG_API void            DGL_Vertices3ftv(int num, const dgl_ft3vertex_t* vec);
DENG_API void            DGL_Vertices3fctv(int num, const dgl_fct3vertex_t* vec);

DENG_API void            DGL_DrawLine(float x1, float y1, float x2, float y2,
                                        float r, float g, float b, float a);
DENG_API void            DGL_DrawRect(float x, float y, float w, float h, float r,
                                        float g, float b, float a);
DENG_API void            DGL_DrawRectTiled(float x, float y, float w, float h, int tw, int th);
DENG_API void            DGL_DrawCutRectTiled(float x, float y, float w, float h, int tw,
                                                int th, int txoff, int tyoff, float cx,
                                                float cy, float cw, float ch);
/**
 * \todo The following routines should not be necessary once materials can
 * be created dynamically.
 */
DENG_API int             DGL_Bind(DGLuint texture);
DENG_API void            DGL_DeleteTextures(int num, const DGLuint* names);
DENG_API void            DGL_EnableTexUnit(byte id);
DENG_API void            DGL_DisableTexUnit(byte id);

#endif
