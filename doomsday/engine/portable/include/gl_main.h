/**\file gl_main.h
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2011 Daniel Swanson <danij@dengine.net>
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
 * Graphics Subsystem.
 */

#ifndef LIBDENG_GRAPHICS_H
#define LIBDENG_GRAPHICS_H

#include "r_main.h"

// This should be tweaked a bit.
#define DEFAULT_FOG_START       0
#define DEFAULT_FOG_END         2100
#define DEFAULT_FOG_DENSITY     0.0001f
#define DEFAULT_FOG_COLOR_RED   138.0f/255
#define DEFAULT_FOG_COLOR_GREEN 138.0f/255
#define DEFAULT_FOG_COLOR_BLUE  138.0f/255

typedef enum glfontstyle_e {
    GLFS_NORMAL,
    GLFS_BOLD,
    GLFS_LIGHT,
    NUM_GLFS
} glfontstyle_t;

extern int      numTexUnits;
extern boolean  envModAdd;
extern int      defResX, defResY, defBPP, defFullscreen;
extern int      viewph, viewpw, viewpx, viewpy;
extern int      r_framecounter;
extern float    vid_gamma, vid_bright, vid_contrast;
extern int      glFontFixed, glFontVariable[NUM_GLFS];
extern int      r_detail;

boolean         GL_IsInited(void);

void            GL_Register(void);
boolean         GL_EarlyInit(void);
void            GL_Init(void);
void            GL_Shutdown(void);
void            GL_TotalReset(void);
void            GL_TotalRestore(void);

void            GL_Init2DState(void);
void            GL_SwitchTo3DState(boolean push_state, viewport_t* port);
void            GL_Restore2DState(int step, viewport_t* port);
void            GL_ProjectionMatrix(void);
void            GL_InfinitePerspective(DGLdouble fovy, DGLdouble aspect, DGLdouble znear);
void            GL_DoUpdate(void);
void            GL_BlendMode(blendmode_t mode);

void            GL_InitRefresh(void);
void            GL_ShutdownRefresh(void);
void            GL_UseFog(int yes);
const char*     GL_ChooseFixedFont(void);
const char*     GL_ChooseVariableFont(glfontstyle_t style, int resX, int resY);
void            GL_LowRes(void);
void            GL_ActiveTexture(const DGLenum texture);
void            GL_ModulateTexture(int mode);
void            GL_SelectTexUnits(int count);
void            GL_SetTextureCompression(boolean on);
void            GL_SetVSync(boolean on);
void            GL_SetMultisample(boolean on);
void            GL_BlendOp(int op);
void            GL_SetGrayMipmap(int lev);
boolean         GL_NewList(DGLuint list, int mode);
DGLuint         GL_EndList(void);
void            GL_CallList(DGLuint list);
void            GL_DeleteLists(DGLuint list, int range);
void            GL_InitArrays(void);
void            GL_EnableArrays(int vertices, int colors, int coords);
void            GL_DisableArrays(int vertices, int colors, int coords);
void            GL_Arrays(void* vertices, void* colors, int numCoords,
                          void** coords, int lock);
void            GL_UnlockArrays(void);
void            GL_ArrayElement(int index);
void            GL_DrawElements(dglprimtype_t type, int count,
                                const uint* indices);
boolean         GL_Grab(int x, int y, int width, int height,
                        dgltexformat_t format, void* buffer);

/**
 * @param format  DGL texture format symbolic, one of:
 *      DGL_RGB
 *      DGL_RGBA
 *      DGL_COLOR_INDEX_8
 *      DGL_COLOR_INDEX_8_PLUS_A8
 *      DGL_LUMINANCE
 * @param palid  Id of the color palette to use with this texture. Only has
 *      meaning if the input format is one of:
 *      DGL_COLOR_INDEX_8
 *      DGL_COLOR_INDEX_8_PLUS_A8
 * @param width  Width of the texture, must be power of two.
 * @param height  Height of the texture, must be power of two.
 * @param genMips  If negative sets a specific mipmap level, e.g.:
 *      @c -1, means mipmap level 1.
 * @param data Ptr to the texture data.
 *
 * @return  @c true iff successful.
 */
boolean GL_TexImage(dgltexformat_t format, DGLuint palid, int width, int height,
    int genMips, void* data);

/**
 * Given a logical anisotropic filtering level return an appropriate multiplier
 * according to the current GL state and user configuration.
 */
int GL_GetTexAnisoMul(int level);

/**
 * How many mipmap levels are needed for a texture of the given dimensions?
 *
 * @param width  Logical width of the texture in pixels.
 * @param height  Logical height of the texture in pixels.
 * @return  Number of mipmap levels required.
 */
int GL_NumMipmapLevels(int width, int height);

/**
 * @param compOrder  Component order. Examples:
 *                         [0,1,2] == RGB
 *                         [2,1,0] == BGR
 * @param compSize  Number of bits per component [R,G,B].
 */
DGLuint GL_CreateColorPalette(const int compOrder[3], const uint8_t compSize[3],
    const uint8_t* data, ushort num);

void GL_DeleteColorPalettes(DGLsizei n, const DGLuint* palettes);

void GL_GetColorPaletteRGB(DGLuint id, DGLubyte rgb[3], ushort idx);

boolean GL_PalettizeImage(uint8_t* out, int outformat, DGLuint palid,
    boolean gammaCorrect, const uint8_t* in, int informat, int width, int height);

boolean GL_QuantizeImageToPalette(uint8_t* out, int outformat, DGLuint palid,
    const uint8_t* in, int informat, int width, int height);

/**
 * Desaturates the texture in the dest buffer by averaging the colour then
 * looking up the nearest match in the palette. Increases the brightness
 * to maximum.
 */
void GL_DeSaturatePalettedImage(byte* buffer, DGLuint palid, int width, int height);

// Returns a pointer to a copy of the screen. The pointer must be
// deallocated by the caller.
unsigned char*  GL_GrabScreen(void);

/**
 * in/out format:
 * 1 = palette indices
 * 2 = palette indices followed by alpha values
 * 3 = RGB
 * 4 = RGBA
 */
uint8_t* GL_ConvertBuffer(const uint8_t* src, int width, int height,
    int comps, colorpaletteid_t pal, boolean gamma, int outformat);

/**
 * @param method  Unique identifier of the smart filtering method to apply.
 * @param src  Source image to be filtered.
 * @param width  Logical width of the source image in pixels.
 * @param height  Logical height of the source image in pixels.
 * @param flags  @see imageConversionFlags.
 * @param outWidth  Logical width of resultant image in pixels.
 * @param outHeight  Logical height of resultant image in pixels.
 *
 * @return  Newly allocated version of the source image if filtered else @c == @a src.
 */
uint8_t* GL_SmartFilter(int method, const uint8_t* src, int width, int height,
    int flags, int* outWidth, int* outHeight);

/**
 * Calculates the properties of a dynamic light that the given sprite frame
 * casts. Crop a boundary around the image to remove excess alpha'd pixels
 * from adversely affecting the calculation.
 * Handles pixel sizes; 1 (==2), 3 and 4.
 */
void GL_CalcLuminance(const uint8_t* buffer, int width, int height, int comps,
    colorpaletteid_t palid, float* brightX, float* brightY, float color[3],
    float* lumSize);

/**
 * @param width  Logical width of the image in pixels.
 * @param height  Logical height of the image in pixels.
 * @param flags  @see imageConversionFlags.
 */
int GL_ChooseSmartFilter(int width, int height, int flags);

/**
 * The given RGB color is scaled uniformly so that the highest component
 * becomes one.
 */
void amplify(float rgb[3]);

// Console commands.
D_CMD(UpdateGammaRamp);

#endif /* LIBDENG_GRAPHICS_H */
