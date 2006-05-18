/* DE1: $Id$
 * Copyright (C) 2003, 2004 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * along with this program; if not: http://www.opensource.org/
 */

/*
 * dglib.h: Doomsday Graphics Library Constants and Data Structures
 *
 * This header file is needed by all modules that use DGL.
 */

#ifndef __DOOMSDAY_GL_SHARED_H__
#define __DOOMSDAY_GL_SHARED_H__

#define DGL_VERSION_NUM     14

// Types.
typedef unsigned char DGLubyte;
typedef unsigned int DGLuint;

typedef struct gl_vertex_s {
    float           xyz[4];        // The fourth is padding.
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

enum {
    DGL_FALSE = 0x0000,
    DGL_TRUE = 0x0001,

    // Return codes
    DGL_ERROR = 0x0000,
    DGL_OK = 0x0001,
    DGL_UNSUPPORTED,

    DGL_MODE_WINDOW = 0x0000,
    DGL_MODE_FULLSCREEN = 0x0001,

    // Formats
    DGL_RGB = 0x1000,
    DGL_RGBA,
    DGL_COLOR_INDEX_8,
    DGL_COLOR_INDEX_8_PLUS_A8,
    DGL_LUMINANCE,
    DGL_R,
    DGL_G,
    DGL_B,
    DGL_A,
    DGL_DEPTH_COMPONENT,
    DGL_SINGLE_PIXELS,
    DGL_BLOCK,
    DGL_NORMAL_LIST,
    DGL_SKYMASK_LISTS,
    DGL_LIGHT_LISTS,
    DGL_NORMAL_DLIT_LIST,
    DGL_NORMAL_DETAIL_LIST,
    DGL_LUMINANCE_PLUS_A8,

    // Values
    DGL_VERSION = 0x2000,
    DGL_WINDOW_HANDLE,
    DGL_COLOR_BITS,
    DGL_MAX_TEXTURE_SIZE,
    DGL_SCISSOR_BOX,
    DGL_POLY_COUNT,
    DGL_TEXTURE_BINDING,
    DGL_MAX_TEXTURE_UNITS,
    DGL_ACTIVE_TEXTURE,

    // Primitives
    DGL_LINES = 0x3000,
    DGL_TRIANGLES,
    DGL_TRIANGLE_FAN,
    DGL_TRIANGLE_STRIP,
    DGL_QUADS,
    DGL_QUAD_STRIP,
    DGL_SEQUENCE,
    DGL_POINTS,

    // Matrices
    DGL_MODELVIEW = 0x4000,
    DGL_PROJECTION,
    DGL_TEXTURE,

    // Caps
    DGL_TEXTURING = 0x5000,
    DGL_BLENDING = 0x5001,
    DGL_DEPTH_TEST = 0x5002,
    DGL_ALPHA_TEST = 0x5003,
    DGL_SCISSOR_TEST = 0x5004,
    DGL_CULL_FACE = 0x5005,
    DGL_COLOR_WRITE = 0x5006,
    DGL_DEPTH_WRITE = 0x5007,
    DGL_FOG = 0x5008,
    DGL_PALETTED_TEXTURES = 0x5009,
    DGL_PALETTED_GENMIPS = 0x500B,
    DGL_MODULATE_ADD_COMBINE = 0x500C,
    DGL_MODULATE_TEXTURE = 0x500D,
    DGL_BLENDING_OP = 0x500E,
    DGL_WIREFRAME_MODE = 0x500F,
    DGL_TEXTURE_COMPRESSION = 0x5010,
    DGL_VSYNC = 0x5011,

    DGL_TEXTURE0 = 0x5F00,
    DGL_TEXTURE1 = 0x5F01,
    DGL_TEXTURE2,
    DGL_TEXTURE3,
    DGL_TEXTURE4,
    DGL_TEXTURE5,
    DGL_TEXTURE6,
    DGL_TEXTURE7,

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

    // Comparative functions
    DGL_ALWAYS = 0x7000,
    DGL_NEVER,
    DGL_LESS,
    DGL_EQUAL,
    DGL_LEQUAL,
    DGL_GREATER,
    DGL_GEQUAL,
    DGL_NOTEQUAL,

    // Miscellaneous
    DGL_MIN_FILTER = 0xF000,
    DGL_MAG_FILTER,
    DGL_NEAREST,
    DGL_LINEAR,
    DGL_NEAREST_MIPMAP_NEAREST,
    DGL_LINEAR_MIPMAP_NEAREST,
    DGL_NEAREST_MIPMAP_LINEAR,
    DGL_LINEAR_MIPMAP_LINEAR,
    DGL_WRAP_S,
    DGL_WRAP_T,
    DGL_CLAMP,
    DGL_REPEAT,
    DGL_FOG_MODE,
    DGL_FOG_DENSITY,
    DGL_FOG_START,
    DGL_FOG_END,
    DGL_FOG_COLOR,
    DGL_EXP,
    DGL_EXP2,
    DGL_WIDTH,
    DGL_HEIGHT,
    DGL_ENV_COLOR,
    DGL_ENV_ALPHA,
    DGL_GRAY_MIPMAP,
    DGL_CCW,
    DGL_CW,

    // Various bits
    DGL_COLOR_BUFFER_BIT = 0x00000001,
    DGL_DEPTH_BUFFER_BIT = 0x00000002,
    DGL_ALL_BITS = 0xFFFFFFFF
};

#endif
