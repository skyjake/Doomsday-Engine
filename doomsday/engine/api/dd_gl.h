/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2008 Jaakko Keränen <jaakko.keranen@iki.fi>
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

enum {
    // Values
    DGL_COLOR_BITS,
    DGL_MAX_TEXTURE_SIZE,
    DGL_SCISSOR_BOX,
    DGL_POLY_COUNT,
    DGL_TEXTURE_BINDING,
    DGL_MAX_TEXTURE_UNITS,
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
    DGL_SCISSOR_TEST = 0x5004,
    DGL_FOG = 0x5008,
    DGL_PALETTED_TEXTURES = 0x5009,
    DGL_PALETTED_GENMIPS = 0x500B,
    DGL_MODULATE_ADD_COMBINE = 0x500C,
    DGL_MODULATE_TEXTURE = 0x500D,
    DGL_TEXTURE_COMPRESSION = 0x500F,
    DGL_TEXTURE_NON_POWER_OF_TWO = 0x5010,
    DGL_VSYNC = 0x5011,
    DGL_MULTISAMPLE = 0x5012,
    DGL_LINE_SMOOTH = 0x5013,
    DGL_POINT_SMOOTH = 0x5014,

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
    DGL_REPEAT,
    DGL_GRAY_MIPMAP,
    DGL_LINE_WIDTH,
    DGL_POINT_SIZE,

    // Various bits
    DGL_ALL_BITS = 0xFFFFFFFF
};

// Types.
typedef unsigned char DGLubyte;
typedef unsigned int DGLuint;

// Texture formats:
typedef enum gltexformat_e {
    DGL_RGB,
    DGL_RGBA,
    DGL_COLOR_INDEX_8,
    DGL_COLOR_INDEX_8_PLUS_A8,
    DGL_LUMINANCE,
    DGL_DEPTH_COMPONENT,
    DGL_LUMINANCE_PLUS_A8
} gltexformat_t;

typedef enum glprimtype_e {
    DGL_LINES,
    DGL_TRIANGLES,
    DGL_TRIANGLE_FAN,
    DGL_TRIANGLE_STRIP,
    DGL_QUADS,
    DGL_QUAD_STRIP,
    DGL_POINTS
} glprimtype_t;

typedef struct gl_vertex_s {
    float           xyz[4]; // The fourth is padding.
} gl_vertex_t;

typedef struct gl_texcoord_s {
    float           st[2];
} gl_texcoord_t;

typedef struct gl_color_s {
    byte            rgba[4];
} gl_color_t;

typedef struct {
    DGLubyte        rgb[3];
} gl_rgb_t;

typedef struct {
    DGLubyte        rgba[4];
} gl_rgba_t;

// A 2-vertex with texture coordinates, using floats
typedef struct {
    float           pos[2];
    float           tex[2];
} gl_ft2vertex_t;

// A 3-vertex with texture coordinates, using floats
typedef struct {
    float           pos[3];
    float           tex[2];
} gl_ft3vertex_t;

// A 3-vertex with texture coordinates and a color, using floats
typedef struct {
    float           pos[3];
    float           tex[2];
    float           color[4];
} gl_fct3vertex_t;

// A colored 3-vertex, using floats
typedef struct {
    float           pos[3];
    float           color[4];
} gl_fc3vertex_t;

boolean         DGL_GetIntegerv(int name, int *v);
int             DGL_GetInteger(int name);
boolean         DGL_SetInteger(int name, int value);
float           DGL_GetFloat(int name);
boolean         DGL_SetFloat(int name, float value);
void            DGL_BlendOp(int op);
void            DGL_Scissor(int x, int y, int width, int height);
int             DGL_Enable(int cap);
void            DGL_Disable(int cap);
void            DGL_EnableTexUnit(byte id);
void            DGL_DisableTexUnit(byte id);
void            DGL_BlendFunc(int param1, int param2);
void            DGL_Translatef(float x, float y, float z);
void            DGL_Rotatef(float angle, float x, float y, float z);
void            DGL_Scalef(float x, float y, float z);
void            DGL_Ortho(float left, float top, float right, float bottom, float znear,
                          float zfar);
void            DGL_MatrixMode(int mode);
void            DGL_PushMatrix(void);
void            DGL_PopMatrix(void);
void            DGL_LoadIdentity(void);
void            DGL_Begin(glprimtype_t type);
void            DGL_End(void);
boolean         DGL_NewList(DGLuint list, int mode);
DGLuint         DGL_EndList(void);
void            DGL_CallList(DGLuint list);
void            DGL_DeleteLists(DGLuint list, int range);
void            DGL_Color3ub(DGLubyte r, DGLubyte g, DGLubyte b);
void            DGL_Color3ubv(const DGLubyte *data);
void            DGL_Color4ub(DGLubyte r, DGLubyte g, DGLubyte b, DGLubyte a);
void            DGL_Color4ubv(const DGLubyte *data);
void            DGL_Color3f(float r, float g, float b);
void            DGL_Color3fv(const float *data);
void            DGL_Color4f(float r, float g, float b, float a);
void            DGL_Color4fv(const float *data);
void            DGL_TexCoord2f(byte target, float s, float t);
void            DGL_TexCoord2fv(byte target, float *data);
void            DGL_Vertex2f(float x, float y);
void            DGL_Vertex2fv(const float *data);
void            DGL_Vertex3f(float x, float y, float z);
void            DGL_Vertex3fv(const float *data);
void            DGL_Vertices2ftv(int num, const gl_ft2vertex_t *data);
void            DGL_Vertices3ftv(int num, const gl_ft3vertex_t *data);
void            DGL_Vertices3fctv(int num, const gl_fct3vertex_t *data);
void            DGL_DeleteTextures(int num, const DGLuint *names);
int             DGL_Bind(DGLuint texture);

#endif
