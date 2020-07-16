/** @file gl_main.h GL-Graphics Subsystem.
 * @ingroup gl
 *
 * @authors Copyright &copy; 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright &copy; 2006-2013 Daniel Swanson <danij@dengine.net>
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

#ifndef DE_GL_MAIN_H
#define DE_GL_MAIN_H

#ifndef __CLIENT__
#  error "gl only exists in the Client"
#endif

#ifndef __cplusplus
#  error "gl/gl_main.h requires C++"
#endif

#include "api_gl.h"
#include "sys_opengl.h"

#include "gl/gltextureunit.h"
#include "render/viewports.h"
#include "resource/clienttexture.h"
#include <de/matrix.h>

struct ColorRawf_s;
struct material_s;
struct texturevariant_s;

class ColorPalette;
namespace world { class Material; }

#define TEXQ_BEST               8
#define MINTEXWIDTH             8
#define MINTEXHEIGHT            8

DE_EXTERN_C float vid_gamma, vid_bright, vid_contrast;
DE_EXTERN_C int r_detail;

#ifdef _DEBUG
#  define DE_ASSERT_GL_CONTEXT_ACTIVE()  LIBGUI_ASSERT_GL_CONTEXT_ACTIVE()
#else
#  define DE_ASSERT_GL_CONTEXT_ACTIVE()
#endif

#define Sys_GLCheckError()  Sys_GLCheckErrorArgs(__FILE__, __LINE__)

#ifdef _DEBUG
#  define DE_ASSERT_GL_TEXTURE_ISBOUND(tex) { \
    GLint p; \
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &p); \
    Sys_GLCheckError(); \
    assert(p == (GLint)tex); \
}
#else
#  define DE_ASSERT_GL_TEXTURE_ISBOUND(tex)
#endif

void GL_AssertContextActive();

/// Register the console commands, variables, etc..., of this module.
void GL_Register();

/**
 * One-time initialization of GL and the renderer. This is done very early
 * on during engine startup and is supposed to be fast. All subsystems
 * cannot yet be initialized, such as the texture management, so any rendering
 * occuring before GL_Init() must be done with manually prepared textures.
 */
void GL_EarlyInit();

/**
 * Finishes GL initialization. This can be called once the virtual file
 * system has been fully loaded up, and the console variables have been
 * read from the config file.
 */
void GL_Init();

/**
 * Kills the graphics library for good.
 */
void GL_Shutdown();

/**
 * Returns @c true iff the graphics library is currently initialized
 * for basic drawing (using the OpenGL API directly).
 */
dd_bool GL_IsInited();

/**
 * Determines if the renderer is fully initialized (texture manager, deferring,
 * etc).
 */
dd_bool GL_IsFullyInited();

/**
 * GL is reset back to the state it was right after initialization.
 * Use GL_TotalRestore to bring back online.
 */
void GL_TotalReset();

/**
 * To be called after a GL_TotalReset to bring GL back online.
 */
void GL_TotalRestore();

/**
 * Initializes the renderer to 2D state.
 */
void GL_Init2DState();

void GL_ProjectionMatrix(bool useFixedFov = false /* psprites use fixed FOV */);

de::Rangef GL_DepthClipRange();

/**
 * The first selected unit is active after this call.
 */
void GL_SelectTexUnits(int count);

/**
 * Finish the frame being rendered. Applies the FPS limiter, if enabled.
 */
void GL_FinishFrame();

/**
 * Set the current GL blending mode.
 */
void GL_BlendMode(blendmode_t mode);

/**
 * Utility for translating to a GL texture filter identifier.
 */
GLenum GL_Filter(de::gfx::Filter f);

/**
 * Utility for translating to a GL texture wrapping identifier.
 */
GLenum GL_Wrap(de::gfx::Wrapping w);

/**
 * Initializes the graphics library for refresh. Also called at update.
 */
void GL_InitRefresh();

/**
 * To be called once at final shutdown.
 */
void GL_ShutdownRefresh();

/**
 * Enables or disables vsync. May cause the OpenGL surface to be recreated.
 *
 * @param on  @c true to enable vsync, @c false to disable.
 */
void GL_SetVSync(dd_bool on);

/**
 * Enables or disables multisampling when FSAA is available. You cannot enable
 * multisampling if FSAA has not been enabled in the Canvas. Never causes the GL surface
 * or pixel format to be modified; can be called at any time during the rendering of a
 * frame.
 *
 * @param on  @c true to enable multisampling, @c false to disable.
 */
//void GL_SetMultisample(dd_bool on);

/**
 * Reconfigure GL fog according to the setup defined in the specified @a mapInfo definition.
 */
void GL_SetupFogFromMapInfo(const de::Record *mapInfo);

//void GL_BlendOp(int op);

dd_bool GL_NewList(DGLuint list, int mode);

DGLuint GL_EndList();

void GL_CallList(DGLuint list);

void GL_DeleteLists(DGLuint list, int range);

void GL_SetMaterialUI2(world::Material *mat, de::gfx::Wrapping wrapS, de::gfx::Wrapping wrapT);
void GL_SetMaterialUI(world::Material *mat);

void GL_SetPSprite(world::Material *mat, int tclass, int tmap);

void GL_SetRawImage(lumpnum_t lumpNum, de::gfx::Wrapping wrapS, de::gfx::Wrapping wrapT);

/**
 * Bind this texture to the currently active texture unit.
 * The bind process may result in modification of the GL texture state
 * according to the specification used to define this variant.
 *
 * @param tex  Texture::Variant object which represents the GL texture to be bound.
 */
void GL_BindTexture(ClientTexture::Variant *tex);

void GL_BindTextureUnmanaged(GLuint            texname,
                             de::gfx::Wrapping wrapS = de::gfx::Repeat,
                             de::gfx::Wrapping wrapT = de::gfx::Repeat,
                             de::gfx::Filter         = de::gfx::Linear);

/**
 * Bind the associated texture and apply the texture unit configuration to
 * the @em active GL texture unit. If no texture is associated then nothing
 * will happen.
 */
void GL_Bind(const de::GLTextureUnit &glTU);

/**
 * Bind the associated texture and apply the texture unit configuration to
 * the specified GL texture @a unit, which, is made active during this call.
 * If no texture is associated then nothing will happen.
 */
void GL_BindTo(const de::GLTextureUnit &glTU, int unit);

void GL_SetNoTexture();

/**
 * Given a logical anisotropic filtering level return an appropriate multiplier
 * according to the current GL state and user configuration.
 */
int GL_GetTexAnisoMul(int level);

/**
 * How many mipmap levels are needed for a texture of the given dimensions?
 *
 * @param width  Width of the texture in pixels.
 * @param height  Height of the texture in pixels.
 * @return  Number of mipmap levels required.
 */
int GL_NumMipmapLevels(int width, int height);

/**
 * Determine the optimal size for a texture. Usually the dimensions are scaled
 * upwards to the next power of two.
 *
 * @param noStretch  If @c true, the stretching can be skipped.
 * @param isMipMapped  If @c true, we will require mipmaps (this has an effect
 *     on the optimal size).
 */
dd_bool GL_OptimalTextureSize(int     width,
                              int     height,
                              dd_bool noStretch,
                              dd_bool isMipMapped,
                              int *   optWidth,
                              int *   optHeight);

/**
 * @param width  Width of the image in pixels.
 * @param height  Height of the image in pixels.
 * @param flags  @ref imageConversionFlags.
 */
int GL_ChooseSmartFilter(int width, int height, int flags);

GLuint GL_NewTextureWithParams(dgltexformat_t format,
                               int            width,
                               int            height,
                               const uint8_t *pixels,
                               int            flags);

GLuint GL_NewTextureWithParams(dgltexformat_t format,
                               int            width,
                               int            height,
                               const uint8_t *pixels,
                               int            flags,
                               int            grayMipmap,
                               GLenum         minFilter,
                               GLenum         magFilter,
                               int            anisoFilter,
                               GLenum         wrapS,
                               GLenum         wrapT);

/**
 * in/out format:
 * 1 = palette indices
 * 2 = palette indices followed by alpha values
 * 3 = RGB
 * 4 = RGBA
 */
uint8_t *GL_ConvertBuffer(const uint8_t *src, int width, int height,
    int informat, colorpaletteid_t paletteId, int outformat);

/**
 * @param method  Unique identifier of the smart filtering method to apply.
 * @param src  Source image to be filtered.
 * @param width  Width of the source image in pixels.
 * @param height  Height of the source image in pixels.
 * @param flags  @ref imageConversionFlags.
 * @param outWidth  Width of resultant image in pixels.
 * @param outHeight  Height of resultant image in pixels.
 *
 * @return  Newly allocated version of the source image if filtered else @c == @a src.
 */
uint8_t *GL_SmartFilter(int method, const uint8_t *src, int width, int height,
    int flags, int *outWidth, int *outHeight);

/**
 * Calculates the properties of a dynamic light that the given sprite frame
 * casts. Crop a boundary around the image to remove excess alpha'd pixels
 * from adversely affecting the calculation.
 * Handles pixel sizes; 1 (==2), 3 and 4.
 */
void GL_CalcLuminance(const uint8_t *buffer, int width, int height, int comps,
    colorpaletteid_t paletteId, float *brightX, float *brightY,
    struct ColorRawf_s *color, float *lumSize);

// DGL internal API ---------------------------------------------------------------------

void      DGL_Shutdown();
unsigned int    DGL_BatchMaxSize();
void      DGL_BeginFrame();
void            DGL_Flush();
void      DGL_AssertNotInPrimitive(void);
de::Mat4f DGL_Matrix(DGLenum matrixMode);
void      DGL_CurrentColor(DGLubyte *rgba);
void      DGL_CurrentColor(float *rgba);
void      DGL_ModulateTexture(int mode);
void      DGL_SetModulationColor(const de::Vec4f &modColor);
de::Vec4f DGL_ModulationColor();
void      DGL_FogParams(de::GLUniform &fogRange, de::GLUniform &fogColor);
void            DGL_DepthFunc(DGLenum depthFunc);
void            DGL_CullFace(DGLenum cull);

// Console commands ---------------------------------------------------------------------

//D_CMD(UpdateGammaRamp);

#endif // DE_GL_MAIN_H
