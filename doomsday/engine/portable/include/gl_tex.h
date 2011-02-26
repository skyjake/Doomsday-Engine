/**\file gl_tex.h
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
 * Texture Manipulation Algorithms.
 */

#ifndef LIBDENG_TEXTURES_H
#define LIBDENG_TEXTURES_H

struct image_s;

boolean GL_OptimalSize(int width, int height, int* optWidth, int* optHeight,
    boolean noStretch, boolean isMipMapped);

/**
 * in/out format:
 * 1 = palette indices
 * 2 = palette indices followed by alpha values
 * 3 = RGB
 * 4 = RGBA
 */
void GL_ConvertBuffer(int width, int height, int informat, int outformat,
    const uint8_t* src, uint8_t* dst, colorpaletteid_t pal, boolean gamma);

uint8_t* GL_ScaleBuffer32(const uint8_t* src, int width, int height, int comps,
    int outWidth, int outHeight);

void            GL_DownMipmap32(byte* in, int width, int height, int comps);
void            GL_ConvertToAlpha(struct image_s *image, boolean makeWhite);
void            GL_ConvertToLuminance(struct image_s *image);
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

boolean         ImageHasAlpha(struct image_s *image);

/**
 * @param width  Logical width of the image in pixels.
 * @param height  Logical height of the image in pixels.
 * @param flags  @see imageConversionFlags.
 */
int GL_ChooseSmartFilter(int width, int height, int flags);

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

#endif /* LIBDENG_TEXTURES_H */
