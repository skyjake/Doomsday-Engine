/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2010 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2007-2010 Daniel Swanson <danij@dengine.net>
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

int             DGL_Enable(int cap);
void            DGL_Disable(int cap);

boolean         DGL_GetIntegerv(int name, int* vec);
int             DGL_GetInteger(int name);
boolean         DGL_SetInteger(int name, int value);
float           DGL_GetFloat(int name);
boolean         DGL_SetFloat(int name, float value);

void            DGL_Ortho(float left, float top, float right, float bottom, float znear, float zfar);
void            DGL_Scissor(int x, int y, int width, int height);

void            DGL_MatrixMode(int mode);
void            DGL_PushMatrix(void);
void            DGL_PopMatrix(void);
void            DGL_LoadIdentity(void);

void            DGL_Translatef(float x, float y, float z);
void            DGL_Rotatef(float angle, float x, float y, float z);
void            DGL_Scalef(float x, float y, float z);

void            DGL_Begin(dglprimtype_t type);
void            DGL_End(void);
boolean         DGL_NewList(DGLuint list, int mode);
DGLuint         DGL_EndList(void);
void            DGL_CallList(DGLuint list);
void            DGL_DeleteLists(DGLuint list, int range);

void            DGL_SetMaterial(struct material_s* mat);
void            DGL_SetNoMaterial(void);
void            DGL_SetPatch(patchid_t id, int wrapS, int wrapT);
void            DGL_SetPSprite(struct material_s* mat);
void            DGL_SetTranslatedSprite(struct material_s* mat, int tclass, int tmap);
void            DGL_SetRawImage(lumpnum_t lump, int wrapS, int wrapT);

void            DGL_BlendOp(int op);
void            DGL_BlendFunc(int param1, int param2);
void            DGL_BlendMode(blendmode_t mode);

void            DGL_Color3ub(DGLubyte r, DGLubyte g, DGLubyte b);
void            DGL_Color3ubv(const DGLubyte* vec);
void            DGL_Color4ub(DGLubyte r, DGLubyte g, DGLubyte b, DGLubyte a);
void            DGL_Color4ubv(const DGLubyte* vec);
void            DGL_Color3f(float r, float g, float b);
void            DGL_Color3fv(const float* vec);
void            DGL_Color4f(float r, float g, float b, float a);
void            DGL_Color4fv(const float* vec);

void            DGL_TexCoord2f(byte target, float s, float t);
void            DGL_TexCoord2fv(byte target, float* vec);

void            DGL_Vertex2f(float x, float y);
void            DGL_Vertex2fv(const float* vec);
void            DGL_Vertex3f(float x, float y, float z);
void            DGL_Vertex3fv(const float* vec);
void            DGL_Vertices2ftv(int num, const dgl_ft2vertex_t* vec);
void            DGL_Vertices3ftv(int num, const dgl_ft3vertex_t* vec);
void            DGL_Vertices3fctv(int num, const dgl_fct3vertex_t* vec);

void            DGL_DrawLine(float x1, float y1, float x2, float y2, float r, float g, float b, float a);
void            DGL_DrawRect(float x, float y, float w, float h, float r, float g, float b, float a);
void            DGL_DrawRectTiled(float x, float y, float w, float h, int tw, int th);
void            DGL_DrawCutRectTiled(float x, float y, float w, float h, int tw, int th, int txoff, int tyoff, float cx, float cy, float cw, float ch);

void            DGL_DrawPatch(patchid_t id, int x, int y);
void            DGL_DrawPatch_CS(patchid_t id, int x, int y);
void            DGL_DrawFuzzPatch(patchid_t id, int x, int y);
void            DGL_DrawAltFuzzPatch(patchid_t id, int x, int y);
void            DGL_DrawShadowedPatch(patchid_t id, int x, int y);

void            DGL_DrawRawScreen(lumpnum_t lump, float offx, float offy);
void            DGL_DrawRawScreen_CS(lumpnum_t lump, float offx, float offy, float scalex, float scaley);

/**
 * \todo The following routines should not be necessary once materials can
 * be created dynamically.
 */
int             DGL_Bind(DGLuint texture);
void            DGL_DeleteTextures(int num, const DGLuint* names);
void            DGL_EnableTexUnit(byte id);
void            DGL_DisableTexUnit(byte id);
#endif
