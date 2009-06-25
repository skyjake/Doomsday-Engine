/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2009 Daniel Swanson <danij@dengine.net>
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
 * gl_main.h: Graphics Subsystem
 */

#ifndef __DOOMSDAY_GRAPHICS_H__
#define __DOOMSDAY_GRAPHICS_H__

#include "con_decl.h"
#include "r_main.h"

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
extern char     hiTexPath[], hiTexPath2[];
extern int      UpdateState;
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
void            GL_Restore2DState(int step);
void            GL_ProjectionMatrix(void);
void            GL_RuntimeMode(void);
void            GL_DoUpdate(void);
void            GL_BlendMode(blendmode_t mode);

void            GL_InitRefresh(void);
void            GL_ShutdownRefresh(void);
//void            GL_InitVarFont(void);
//void            GL_ShutdownVarFont(void);
const char*     GL_ChooseFixedFont(void);
const char*     GL_ChooseVariableFont(glfontstyle_t style, int resX, int resY);
void            GL_LowRes(void);
void            GL_ActiveTexture(const GLenum texture);
void            GL_ModulateTexture(int mode);
void            GL_SelectTexUnits(int count);
void            GL_SetTextureCompression(boolean on);
void            GL_SetVSync(boolean on);
void            GL_SetMultisample(boolean on);
void            GL_EnableTexUnit(byte id);
void            GL_DisableTexUnit(byte id);
void            GL_BlendOp(int op);
void            GL_SetGrayMipmap(int lev);
boolean         GL_EnablePalTexExt(boolean enable);
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
boolean         GL_TexImage(dgltexformat_t format, DGLuint palid, int width,
                            int height, int genMips, void* data);
int             GL_GetTexAnisoMul(int level);

DGLuint         GL_CreateColorPalette(const int compOrder[3],
                                      const byte compSize[3],
                                      const byte* data, ushort num);
void            GL_DeleteColorPalettes(DGLsizei n, const DGLuint* palettes);

void            GL_GetColorPaletteRGB(DGLuint id, DGLubyte rgb[3],
                                      ushort idx);
boolean         GL_PalettizeImage(byte* out, int outformat, DGLuint palid,
                                  boolean gammaCorrect, const byte* in,
                                  int informat, int width, int height);
boolean         GL_QuantizeImageToPalette(byte* out, int outformat,
                                          DGLuint palid, const byte* in,
                                          int informat, int width,
                                          int height);
void            GL_DeSaturatePalettedImage(byte* buffer, DGLuint palid,
                                           int width, int height);

// Console commands.
D_CMD(UpdateGammaRamp);

#endif
