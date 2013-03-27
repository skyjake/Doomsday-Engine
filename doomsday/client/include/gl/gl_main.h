/** @file gl_main.h GL-Graphics Subsystem.
 * @ingroup gl
 *
 * @authors Copyright &copy; 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDENG_GL_MAIN_H
#define LIBDENG_GL_MAIN_H

#ifndef __CLIENT__
#  error "gl only exists in the Client"
#endif

#ifndef __cplusplus
#  error "gl/gl_main.h requires C++"
#endif

#include "api_gl.h"
#include "sys_opengl.h"

#include "render/r_main.h"
#include "Texture"

struct colorpalette_s;
struct ColorRawf_s;
struct material_s;
struct texturevariant_s;

class Material;

#define MAX_TEX_UNITS           2 // More aren't currently used.

DENG_EXTERN_C int numTexUnits;
DENG_EXTERN_C boolean  envModAdd;
DENG_EXTERN_C int defResX, defResY, defBPP, defFullscreen;
DENG_EXTERN_C int viewph, viewpw, viewpx, viewpy;
DENG_EXTERN_C float vid_gamma, vid_bright, vid_contrast;
DENG_EXTERN_C int r_detail;

#ifdef _DEBUG
#  define LIBDENG_ASSERT_GL_CONTEXT_ACTIVE()  {GL_AssertContextActive();}
#else
#  define LIBDENG_ASSERT_GL_CONTEXT_ACTIVE()
#endif

#ifdef _DEBUG
#  define LIBDENG_ASSERT_GL_TEXTURE_ISBOUND(tex) { \
    GLint p; \
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &p); \
    Sys_GLCheckError(); \
    assert(p == (GLint)tex); \
}
#else
#  define LIBDENG_ASSERT_GL_TEXTURE_ISBOUND(tex)
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
boolean GL_EarlyInit();

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
boolean GL_IsInited();

/**
 * Determines if the renderer is fully initialized (texture manager, deferring,
 * etc).
 */
boolean GL_IsFullyInited();

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

void GL_SwitchTo3DState(boolean push_state, viewport_t const *port, viewdata_t const *viewData);

void GL_Restore2DState(int step, viewport_t const *port, viewdata_t const *viewData);

void GL_ProjectionMatrix();

/**
 * Swaps buffers / blits the back buffer to the front.
 */
void GL_DoUpdate();

/**
 * Set the current GL blending mode.
 */
void GL_BlendMode(blendmode_t mode);

/**
 * Initializes the graphics library for refresh. Also called at update.
 */
void GL_InitRefresh();

/**
 * To be called once at final shutdown.
 */
void GL_ShutdownRefresh();

/**
 * Configure the GL state for the specified texture modulation mode.
 *
 * @param mode  Modulation mode ident.
 */
void GL_ModulateTexture(int mode);

/**
 * Enables or disables vsync. Changes the value of the vid-vsync variable.
 * May cause the OpenGL surface to be recreated.
 *
 * @param on  @c true to enable vsync, @c false to disable.
 */
void GL_SetVSync(boolean on);

/**
 * Enables or disables multisampling when FSAA is available (vid-fsaa 1). You
 * cannot enable multisampling if vid-fsaa is 0. Never causes the GL surface or
 * pixel format to be modified; can be called at any time during the rendering
 * of a frame.
 *
 * @param on  @c true to enable multisampling, @c false to disable.
 */
void GL_SetMultisample(boolean on);

void GL_BlendOp(int op);

boolean GL_NewList(DGLuint list, int mode);

DGLuint GL_EndList();

void GL_CallList(DGLuint list);

void GL_DeleteLists(DGLuint list, int range);

void GL_SetMaterialUI2(Material *mat, int wrapS, int wrapT);
void GL_SetMaterialUI(Material *mat);

void GL_SetPSprite(Material *mat, int tclass, int tmap);

void GL_SetRawImage(lumpnum_t lumpNum, int wrapS, int wrapT);

/**
 * Bind this texture to the currently active texture unit.
 * The bind process may result in modification of the GL texture state
 * according to the specification used to define this variant.
 *
 * @param tex  Texture::Variant object which represents the GL texture to be bound.
 */
void GL_BindTexture(de::Texture::Variant *tex);

void GL_BindTextureUnmanaged(DGLuint texname, int wrapS = GL_REPEAT, int wrapT = GL_REPEAT,
                             int magMode = GL_LINEAR);

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
 * @param width  Width of the image in pixels.
 * @param height  Height of the image in pixels.
 * @param flags  @ref imageConversionFlags.
 */
int GL_ChooseSmartFilter(int width, int height, int flags);

/**
 * in/out format:
 * 1 = palette indices
 * 2 = palette indices followed by alpha values
 * 3 = RGB
 * 4 = RGBA
 */
uint8_t *GL_ConvertBuffer(uint8_t const *src, int width, int height,
    int informat, struct colorpalette_s *palette, int outformat);

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
uint8_t *GL_SmartFilter(int method, uint8_t const *src, int width, int height,
    int flags, int *outWidth, int *outHeight);

/**
 * Calculates the properties of a dynamic light that the given sprite frame
 * casts. Crop a boundary around the image to remove excess alpha'd pixels
 * from adversely affecting the calculation.
 * Handles pixel sizes; 1 (==2), 3 and 4.
 */
void GL_CalcLuminance(uint8_t const *buffer, int width, int height, int comps,
    struct colorpalette_s *palette, float *brightX, float *brightY,
    struct ColorRawf_s *color, float *lumSize);

// Console commands.
D_CMD(UpdateGammaRamp);

#endif /* LIBDENG_GL_MAIN_H */
