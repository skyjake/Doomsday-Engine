/** @file api_gl.h Doomsday graphics library.
 * @ingroup gl
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2007-2013 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2006 Jamie Jones <jamie_jones_au@yahoo.com.au>
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
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#ifndef DOOMSDAY_GL_H
#define DOOMSDAY_GL_H

#include <de/legacy/rect.h>
#include <doomsday/api_map.h>
#include "dd_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup gl Graphics Library
 * @ingroup render
 */
///@{

enum {
    // Values
    DGL_ACTIVE_TEXTURE = 1,

    DGL_CURRENT_COLOR_R,
    DGL_CURRENT_COLOR_G,
    DGL_CURRENT_COLOR_B,
    DGL_CURRENT_COLOR_A,
    DGL_CURRENT_COLOR_RGBA,

    DGL_FOG_MODE,
    DGL_FOG_START,
    DGL_FOG_END,
    DGL_FOG_DENSITY,
    DGL_FOG_COLOR,

    DGL_LINE_WIDTH,
    DGL_POINT_SIZE,
    DGL_ALPHA_LIMIT,

    // Matrices
    DGL_MODELVIEW = 0x4000,
    DGL_PROJECTION,
    DGL_TEXTURE,

    // Caps
    DGL_TEXTURE_2D = 0x5000,
    DGL_SCISSOR_TEST,
    DGL_FOG,
    DGL_MODULATE_TEXTURE,
    DGL_LINE_SMOOTH,
    DGL_POINT_SMOOTH,
    DGL_BLEND,
    DGL_DEPTH_TEST,
    DGL_DEPTH_WRITE,
    DGL_ALPHA_TEST,

    DGL_TEXTURE0 = 0x5100,
    DGL_TEXTURE1,

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
    DGL_ADD,
    DGL_SUBTRACT,
    DGL_REVERSE_SUBTRACT,

    // Comparison functions
    DGL_NEVER = 0x7000,
    DGL_ALWAYS,
    DGL_EQUAL,
    DGL_NOT_EQUAL,
    DGL_LESS,
    DGL_GREATER,
    DGL_LEQUAL,
    DGL_GEQUAL,

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
    DGL_EXP,
    DGL_EXP2,
    DGL_NONE,
    DGL_BACK,
    DGL_FRONT,
    DGL_FLUSH_BACKTRACE,
};

// Types.
typedef unsigned char DGLubyte;
typedef int           DGLint;
typedef unsigned int  DGLuint;
typedef int           DGLsizei;
typedef double        DGLdouble;
typedef unsigned int  DGLenum;

/// Texture formats.
typedef enum dgltexformat_e {
    DGL_RGB,
    DGL_RGBA,
    DGL_COLOR_INDEX_8,
    DGL_COLOR_INDEX_8_PLUS_A8,
    DGL_LUMINANCE,
    DGL_LUMINANCE_PLUS_A8
} dgltexformat_t;

/// Primitive types.
typedef enum dglprimtype_e {
    DGL_NO_PRIMITIVE,
    DGL_LINES,
    DGL_LINE_STRIP,
    DGL_LINE_LOOP,
    DGL_TRIANGLES,
    DGL_TRIANGLE_FAN,
    DGL_TRIANGLE_STRIP,
    DGL_QUADS,
    DGL_POINTS,
} dglprimtype_t;

#define DDNUM_BLENDMODES    9

/// Blending modes.
typedef enum blendmode_e {
    BM_FIRST = -1,
    BM_ZEROALPHA = BM_FIRST,
    BM_NORMAL,
    BM_ADD,
    BM_DARK,
    BM_SUBTRACT,
    BM_REVERSE_SUBTRACT,
    BM_MUL,
    BM_INVERSE,
    BM_INVERSE_MUL,
    BM_ALPHA_SUBTRACT,
    BM_LAST = BM_ALPHA_SUBTRACT
} blendmode_t;

#define VALID_BLENDMODE(val) ((int)(val) >= BM_FIRST && (int)(val) <= BM_LAST)
#define NUM_BLENDMODES       (10)

static inline const char *DGL_NameForBlendMode(blendmode_t mode)
{
    static const char *names[1 + NUM_BLENDMODES] = {
        /* invalid */               "(invalid)",
        /* BM_ZEROALPHA */          "zero_alpha",
        /* BM_NORMAL */             "normal",
        /* BM_ADD */                "add",
        /* BM_DARK */               "dark",
        /* BM_SUBTRACT */           "subtract",
        /* BM_REVERSE_SUBTRACT */   "reverse_subtract",
        /* BM_MUL */                "mul",
        /* BM_INVERSE */            "inverse",
        /* BM_INVERSE_MUL */        "inverse_mul",
        /* BM_ALPHA_SUBTRACT */     "alpha_subtract"
    };
    if (!VALID_BLENDMODE(mode)) return names[0];
    return names[2 + (int) mode];
}

typedef struct dgl_vertex_s {
    float           xyz[4]; ///< The fourth is padding.
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

/// 2-vertex with texture coordinates, using floats.
typedef struct {
    float           pos[2];
    float           tex[2];
} dgl_ft2vertex_t;

/// 3-vertex with texture coordinates, using floats.
typedef struct {
    float           pos[3];
    float           tex[2];
} dgl_ft3vertex_t;

/// 3-vertex with texture coordinates and a color, using floats.
typedef struct {
    float           pos[3];
    float           tex[2];
    float           color[4];
} dgl_fct3vertex_t;

/// Colored 3-vertex, using floats.
typedef struct {
    float           pos[3];
    float           color[4];
} dgl_fc3vertex_t;

/// @defgroup scaleModes  Scale Modes
/// @ingroup gl
/// @{
typedef enum {
    SCALEMODE_FIRST = 0,
    SCALEMODE_SMART_STRETCH = SCALEMODE_FIRST,
    SCALEMODE_NO_STRETCH, // Never.
    SCALEMODE_STRETCH, // Always.
    SCALEMODE_LAST = SCALEMODE_STRETCH,
    SCALEMODE_COUNT
} scalemode_t;
/// @}

/**
 * @defgroup borderedProjectionFlags  Bordered Projection Flags
 * @ingroup apiFlags
 * @{
 */
#define BPF_OVERDRAW_MASK   0x1
#define BPF_OVERDRAW_CLIP   0x2
///@}

typedef struct {
    int flags;
    scalemode_t scaleMode;
    int width, height;
    int availWidth, availHeight;
    dd_bool isPillarBoxed; /// @c false: align vertically instead.
    float scaleFactor;
} dgl_borderedprojectionstate_t;

DE_API_TYPEDEF(GL)
{
    de_api_t api;

    int (*Enable)(int cap);
    void (*Disable)(int cap);
    void (*PushState)(void);
    void (*PopState)(void);

    dd_bool (*GetIntegerv)(int name, int* vec);
    int (*GetInteger)(int name);
    dd_bool (*SetInteger)(int name, int value);
    dd_bool (*GetFloatv)(int name, float* vec);
    float (*GetFloat)(int name);
    dd_bool (*SetFloat)(int name, float value);

    void (*Ortho)(float left, float top, float right, float bottom, float znear, float zfar);

    /**
     * Change the current viewport scissor region.
     *
     * This function only sets the geometry. To enable the scissor use
     * DGL_Enable(DGL_SCISSOR_TEST).
     *
     * @param rect  Geometry of the new scissor region. Coordinates are
     *              in viewport space.
     */
    void (*SetScissor)(const RectRaw *rect);
    void (*SetScissor2)(int x, int y, int width, int height);

    void (*MatrixMode)(DGLenum mode);
    void (*PushMatrix)(void);
    void (*PopMatrix)(void);
    void (*LoadIdentity)(void);
    void (*LoadMatrix)(const float *matrix4x4);

    void (*Translatef)(float x, float y, float z);
    void (*Rotatef)(float angle, float x, float y, float z);
    void (*Scalef)(float x, float y, float z);

    void (*Begin)(dglprimtype_t type);
    void (*End)(void);

    void (*SetNoMaterial)(void);
    void (*SetMaterialUI)(world_Material *mat, DGLint wrapS, DGLint wrapT);
    void (*SetPatch)(patchid_t id, DGLint wrapS, DGLint wrapT);
    void (*SetPSprite)(world_Material *mat);
    void (*SetPSprite2)(world_Material *mat, int tclass, int tmap);
    void (*SetRawImage)(lumpnum_t lumpNum, DGLint wrapS, DGLint wrapT);

    void (*BlendOp)(int op);
    void (*BlendFunc)(int param1, int param2);
    void (*BlendMode)(blendmode_t mode);

    void (*Color3ub)(DGLubyte r, DGLubyte g, DGLubyte b);
    void (*Color3ubv)(const DGLubyte* vec);
    void (*Color4ub)(DGLubyte r, DGLubyte g, DGLubyte b, DGLubyte a);
    void (*Color4ubv)(const DGLubyte* vec);
    void (*Color3f)(float r, float g, float b);
    void (*Color3fv)(const float* vec);
    void (*Color4f)(float r, float g, float b, float a);
    void (*Color4fv)(const float* vec);

    void (*TexCoord2f)(byte target, float s, float t);
    void (*TexCoord2fv)(byte target, const float *vec);

    void (*Vertex2f)(float x, float y);
    void (*Vertex2fv)(const float* vec);
    void (*Vertex3f)(float x, float y, float z);
    void (*Vertex3fv)(const float* vec);
    void (*Vertices2ftv)(int num, const dgl_ft2vertex_t* vec);
    void (*Vertices3ftv)(int num, const dgl_ft3vertex_t* vec);
    void (*Vertices3fctv)(int num, const dgl_fct3vertex_t* vec);

    void (*DrawLine)(float x1, float y1, float x2, float y2, float r, float g, float b, float a);

    void (*DrawRect)(const RectRaw* rect);
    void (*DrawRect2)(int x, int y, int w, int h);

    void (*DrawRectf)(const RectRawf* rect);
    void (*DrawRectf2)(double x, double y, double w, double h);
    void (*DrawRectf2Color)(double x, double y, double w, double h, float r, float g, float b, float a);
    void (*DrawRectf2Tiled)(double x, double y, double w, double h, int tw, int th);

    void (*DrawCutRectfTiled)(const RectRawf* rect, int tw, int th, int txoff, int tyoff, const RectRawf* cutRect);
    void (*DrawCutRectf2Tiled)(double x, double y, double w, double h, int tw, int th, int txoff, int tyoff,
                               double cx, double cy, double cw, double ch);

    void (*DrawQuadOutline)(const Point2Raw* tl, const Point2Raw* tr, const Point2Raw* br,
                            const Point2Raw* bl, const float color[4]);
    void (*DrawQuad2Outline)(int tlX, int tlY, int trX, int trY, int brX, int brY, int blX, int blY,
                             const float color[4]);

    DGLuint (*NewTextureWithParams)(dgltexformat_t format, int width, int height,
                                    const uint8_t* pixels, int flags, int minFilter, int magFilter,
                                    int anisoFilter, int wrapS, int wrapT);

    /**
     * \todo The following routines should not be necessary once materials can
     * be created dynamically.
     */
    int (*Bind)(DGLuint texture);

    void (*DeleteTextures)(int num, const DGLuint* names);

    void (*Fogi)(DGLenum property, int value);
    void (*Fogf)(DGLenum property, float value);
    void (*Fogfv)(DGLenum property, const float *values);

    void (*UseFog)(int yes);

    void (*SetFilter)(dd_bool enable);
    void (*SetFilterColor)(float r, float g, float b, float a);
    void (*ConfigureBorderedProjection2)(dgl_borderedprojectionstate_t* bp, int flags, int width, int height, int availWidth, int availHeight, scalemode_t overrideMode, float stretchEpsilon);
    void (*ConfigureBorderedProjection)(dgl_borderedprojectionstate_t* bp, int flags, int width, int height, int availWidth, int availHeight, scalemode_t overrideMode);
    void (*BeginBorderedProjection)(dgl_borderedprojectionstate_t* bp);
    void (*EndBorderedProjection)(dgl_borderedprojectionstate_t* bp);

    /**
     * Disable the color filter and clear PostFX (for consoleplayer).
     */
    void (*ResetViewEffects)();
}
DE_API_T(GL);

#ifndef DE_NO_API_MACROS_GL
#define DGL_Enable          _api_GL.Enable
#define DGL_Disable         _api_GL.Disable
#define DGL_PushState       _api_GL.PushState
#define DGL_PopState        _api_GL.PopState
#define DGL_GetIntegerv     _api_GL.GetIntegerv
#define DGL_GetInteger      _api_GL.GetInteger
#define DGL_SetInteger      _api_GL.SetInteger
#define DGL_GetFloatv       _api_GL.GetFloatv
#define DGL_GetFloat        _api_GL.GetFloat
#define DGL_SetFloat        _api_GL.SetFloat
#define DGL_Ortho           _api_GL.Ortho
#define DGL_SetScissor      _api_GL.SetScissor
#define DGL_SetScissor2     _api_GL.SetScissor2
#define DGL_MatrixMode      _api_GL.MatrixMode
#define DGL_PushMatrix      _api_GL.PushMatrix
#define DGL_PopMatrix       _api_GL.PopMatrix
#define DGL_LoadIdentity    _api_GL.LoadIdentity
#define DGL_LoadMatrix      _api_GL.LoadMatrix
#define DGL_Translatef      _api_GL.Translatef
#define DGL_Rotatef         _api_GL.Rotatef
#define DGL_Scalef          _api_GL.Scalef
#define DGL_Begin           _api_GL.Begin
#define DGL_End             _api_GL.End
#define DGL_SetNoMaterial   _api_GL.SetNoMaterial
#define DGL_SetMaterialUI   _api_GL.SetMaterialUI
#define DGL_SetPatch        _api_GL.SetPatch
#define DGL_SetPSprite      _api_GL.SetPSprite
#define DGL_SetPSprite2     _api_GL.SetPSprite2
#define DGL_SetRawImage     _api_GL.SetRawImage
#define DGL_BlendOp         _api_GL.BlendOp
#define DGL_BlendFunc       _api_GL.BlendFunc
#define DGL_BlendMode       _api_GL.BlendMode
#define DGL_Color3ub        _api_GL.Color3ub
#define DGL_Color3ubv       _api_GL.Color3ubv
#define DGL_Color4ub        _api_GL.Color4ub
#define DGL_Color4ubv       _api_GL.Color4ubv
#define DGL_Color3f         _api_GL.Color3f
#define DGL_Color3fv        _api_GL.Color3fv
#define DGL_Color4f         _api_GL.Color4f
#define DGL_Color4fv        _api_GL.Color4fv
#define DGL_TexCoord2f      _api_GL.TexCoord2f
#define DGL_TexCoord2fv     _api_GL.TexCoord2fv
#define DGL_Vertex2f        _api_GL.Vertex2f
#define DGL_Vertex2fv       _api_GL.Vertex2fv
#define DGL_Vertex3f        _api_GL.Vertex3f
#define DGL_Vertex3fv       _api_GL.Vertex3fv
#define DGL_Vertices2ftv            _api_GL.Vertices2ftv
#define DGL_Vertices3ftv            _api_GL.Vertices3ftv
#define DGL_Vertices3fctv           _api_GL.Vertices3fctv
#define DGL_DrawLine                _api_GL.DrawLine
#define DGL_DrawRect                _api_GL.DrawRect
#define DGL_DrawRect2               _api_GL.DrawRect2
#define DGL_DrawRectf               _api_GL.DrawRectf
#define DGL_DrawRectf2              _api_GL.DrawRectf2
#define DGL_DrawRectf2Color         _api_GL.DrawRectf2Color
#define DGL_DrawRectf2Tiled         _api_GL.DrawRectf2Tiled
#define DGL_DrawCutRectfTiled       _api_GL.DrawCutRectfTiled
#define DGL_DrawCutRectf2Tiled      _api_GL.DrawCutRectf2Tiled
#define DGL_DrawQuadOutline         _api_GL.DrawQuadOutline
#define DGL_DrawQuad2Outline        _api_GL.DrawQuad2Outline
#define DGL_NewTextureWithParams    _api_GL.NewTextureWithParams
#define DGL_Bind                    _api_GL.Bind
#define DGL_DeleteTextures          _api_GL.DeleteTextures
#define DGL_Fogi            _api_GL.Fogi
#define DGL_Fogf            _api_GL.Fogf
#define DGL_Fogfv           _api_GL.Fogfv
#define GL_UseFog                   _api_GL.UseFog
#define GL_SetFilter                _api_GL.SetFilter
#define GL_SetFilterColor           _api_GL.SetFilterColor
#define GL_ConfigureBorderedProjection2 _api_GL.ConfigureBorderedProjection2
#define GL_ConfigureBorderedProjection  _api_GL.ConfigureBorderedProjection
#define GL_BeginBorderedProjection  _api_GL.BeginBorderedProjection
#define GL_EndBorderedProjection    _api_GL.EndBorderedProjection
#define GL_ResetViewEffects         _api_GL.ResetViewEffects
#endif

#if defined __DOOMSDAY__ && defined __CLIENT__
DE_USING_API(GL);
#endif

///@}

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* DOOMSDAY_GL_H */
