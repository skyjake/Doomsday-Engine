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
 * gl_tex.h: Texture Manipulation Algorithms.
 */

#ifndef __DOOMSDAY_TEXTURES_H__
#define __DOOMSDAY_TEXTURES_H__

#include "gl_texmanager.h"

#define MINTEXWIDTH             8
#define MINTEXHEIGHT            8

boolean         GL_OptimalSize(int width, int height, int *optWidth,
                               int *optHeight, boolean noStretch,
                               boolean isMipMapped);
void            GL_ConvertBuffer(int width, int height, int informat,
                                 int outformat, byte *in, byte *out,
                                 colorpaletteid_t pal, boolean gamma);
void            GL_ScaleBuffer32(byte *in, int inWidth, int inHeight,
                                 byte *out, int outWidth, int outHeight,
                                 int comps);
void            GL_DownMipmap32(byte* in, int width, int height, int comps);
void            GL_ConvertToAlpha(image_t *image, boolean makeWhite);
void            GL_ConvertToLuminance(image_t *image);
void            GL_CalcLuminance(byte* buffer, int width, int height,
                                 int pixelsize, colorpaletteid_t palid,
                                 float* brightX, float* brightY,
                                 rgbcol_t* color, float* lumSize);
byte*           GL_ApplyColorKeying(byte* buf, unsigned int pixelSize,
                                    unsigned int width, unsigned int height);
int             GL_ValidTexHeight2(int width, int height);

void            pixBlt(byte* src, int srcWidth, int srcHeight, byte* dest,
                       int destWidth, int destHeight, int alpha, int srcRegX,
                       int srcRegY, int destRegX, int destRegY, int regWidth,
                       int regHeight);
void            averageColorIdx(rgbcol_t col, byte* data, int w, int h,
                                colorpaletteid_t palid, boolean hasAlpha);
void            averageColorRGB(rgbcol_t col, byte* data, int w, int h);
int             lineAverageColorIdx(rgbcol_t col, byte* data, int w, int h,
                                    int line, colorpaletteid_t palid,
                                    boolean hasAlpha);
int             lineAverageColorRGB(rgbcol_t col, byte* data, int w, int h,
                                    int line);
void            amplify(float* rgb);
void            ColorOutlines(byte* buffer, int width, int height);
int             DrawRealPatch(byte* buffer, int texwidth, int texheight,
                              const lumppatch_t* patch, int origx,
                              int origy, boolean maskZero,
                              boolean checkForAlpha);
boolean         ImageHasAlpha(image_t *image);
void            GL_TranslatePatch(lumppatch_t* patch, byte* transTable);

int             GL_PickSmartScaleMethod(int width, int height);
void            GL_SmartFilter(int method, byte* in, byte* out, int width, int height);
#endif
