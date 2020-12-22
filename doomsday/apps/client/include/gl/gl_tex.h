/** @file gl_tex.h  Image manipulation and evaluation algorithms.
 *
 * @ingroup gl
 *
 * @todo Belongs in the resource domain -- no ties to GL or related components.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
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

#ifndef DE_GL_IMAGE_MANIPULATION_H
#define DE_GL_IMAGE_MANIPULATION_H

#include <doomsday/color.h>
#include <doomsday/res/colorpalette.h>

typedef struct colorpalette_analysis_s {
    colorpaletteid_t paletteId;
} colorpalette_analysis_t;

typedef struct pointlight_analysis_s {
    float originX, originY, brightMul;
    ColorRawf color;
} pointlight_analysis_t;

typedef struct averagecolor_analysis_s {
    ColorRawf color;
} averagecolor_analysis_t;

typedef struct averagealpha_analysis_s {
    float alpha; ///< Result of the average.
    float coverage; ///< Fraction representing the ratio of alpha to non-alpha pixels.
} averagealpha_analysis_t;

/**
 * @param pixels    Luminance image to be enhanced.
 * @param width     Width of the image in pixels.
 * @param height    Height of the image in pixels.
 * @param hasAlpha  If @c true, @a pixels is assumed to contain luminance plus alpha data
 *                  (totaling 2 * @a width * @a height bytes).
 */
void AmplifyLuma(uint8_t* pixels, int width, int height, dd_bool hasAlpha);

/**
 * Take the input buffer and convert to color keyed. A new buffer may be
 * needed if the input buffer has three color components.
 * Color keying is done for both (0,255,255) and (255,0,255).
 *
 * @return  If the in buffer wasn't large enough will return a ptr to the
 *      newly allocated buffer which must be freed with free(), else @a buf.
 */
uint8_t* ApplyColorKeying(uint8_t* pixels, int width, int height, int pixelSize);

#if 0 // dj: Doesn't make sense, "darkness" applied to an alpha channel?
/**
 * Sets the RGB color of transparent pixels along the image's non-transparent
 * areas to black. When the image is then drawn with magnification,
 * RGB interpolation along the edges will go to (0, 0, 0) while the alpha
 * value is interpolated to zero. The end result is that the edges are
 * highlighted against the background -- i.e., this is a cheap way to make
 * the edges of small sprites stand out more clearly. The effect was originally
 * used by jDoom on font characters and other such graphics to make them appear
 * less blurry.
 *
 * @param pixels  RGBA data (in/out).
 * @param width   Width of the image in pixels.
 * @param height  Height of the image in pixels.
 */
void BlackOutlines(uint8_t* pixels, int width, int height);
#endif

/**
 * Spread the color of non-masked pixels outwards into the masked area.
 * This addresses the "black outlines" produced by texture filtering due to
 * sampling the default (black) color.
 *
 * @param pixels  Paletted pixel data (in/out). The size of this array
 *                is expected to be 2 * @a width * @a height bytes
 *                (the first layer is for the color indices and the second
 *                layer for mask values).
 * @param width   Width of the image in pixels.
 * @param height  Height of the image in pixels.
 */
void ColorOutlinesIdx(uint8_t* pixels, int width, int height);

void ColorOutlinesRGBA(uint8_t *buffer, int width, int height);

/**
 * @param pixels     RGB(a) image to be desaturated (in/out).
 * @param width      Width of the image in pixels.
 * @param height     Height of the image in pixels.
 * @param pixelSize  Size of each pixel. Handles 3 and 4.
 */
void Desaturate(uint8_t* pixels, int width, int height, int pixelSize);

/**
 * @note Does not conform to any standard technique and adjustments
 * are applied symmetrically for all color components.
 *
 * @param pixels     RGB(a) image to be enhanced (in/out).
 * @param width      Width of the image in pixels.
 * @param height     Height of the image in pixels.
 * @param pixelSize  Size of each pixel. Handles 3 and 4.
 */
void EnhanceContrast(uint8_t* pixels, int width, int height, int pixelSize);

/**
 * Equalize the specified luminance map such that the minimum and maximum
 * brightness covers the whole [0...255] range.
 *
 * @par Algorithm
 * Calculates shift deltas for bright and dark-side pixels by
 * averaging the luminosity of all pixels in the original image.
 *
 * @param pixels  Luminance image to equalize.
 * @param width   Width of the image in pixels.
 * @param height  Height of the image in pixels.
 * @param rBaMul  Calculated balance multiplier is written here.
 * @param rHiMul  Calculated multiplier is written here.
 * @param rLoMul  Calculated multiplier is written here.
 */
void EqualizeLuma(uint8_t* pixels, int width, int height,
                  float* rBaMul, float* rHiMul, float* rLoMul);

/**
 * @param pixels     RGB(a) image to evaluate.
 * @param width      Width of the image in pixels.
 * @param height     Height of the image in pixels.
 * @param pixelSize  Size of a pixel in bytes.
 * @param color      Determined average color written here.
 */
void FindAverageColor(const uint8_t* pixels, int width, int height,
                      int pixelSize, ColorRawf* color);

/**
 * @param pixels    Index-color image to evaluate.
 * @param width     Width of the image in pixels.
 * @param height    Height of the image in pixels.
 * @param palette   Color palette to use.
 * @param hasAlpha  @c true == @a pixels includes alpha data.
 * @param color     Determined average color written here.
 */
void FindAverageColorIdx(const uint8_t *pixels, int width, int height,
    const res::ColorPalette &palette, dd_bool hasAlpha, ColorRawf *color);

/**
 * @param pixels     RGB(a) image to evaluate.
 * @param width      Width of the image in pixels.
 * @param height     Height of the image in pixels.
 * @param pixelSize  Size of a pixel in bytes.
 * @param line       Line to evaluate.
 * @param color      Determined average color written here.
 */
void FindAverageLineColor(const uint8_t *pixels, int width, int height,
    int pixelSize, int line, ColorRawf *color);

/**
 * @param pixels    Index-color image to evaluate.
 * @param width     Width of the image in pixels.
 * @param height    Height of the image in pixels.
 * @param line      Line to evaluate.
 * @param palette   Color palette to use.
 * @param hasAlpha  @c true == @a pixels includes alpha data.
 * @param color     Determined average color written here.
 */
void FindAverageLineColorIdx(const uint8_t *pixels, int width, int height,
    int line, const res::ColorPalette &palette, dd_bool hasAlpha, ColorRawf *color);

/**
 * @param pixels     RGB(a) image to evaluate.
 * @param width      Width of the image in pixels.
 * @param height     Height of the image in pixels.
 * @param pixelSize  Size of a pixel in bytes.
 * @param alpha      Determined average alpha written here.
 * @param coverage   Fraction representing the ratio of alpha to non-alpha pixels.
 */
void FindAverageAlpha(const uint8_t *pixels, int width, int height, int pixelSize,
    float *alpha, float *coverage);

/**
 * @param pixels    Index-color image to evaluate.
 * @param width     Width of the image in pixels.
 * @param height    Height of the image in pixels.
 * @param alpha     Determined average alpha written here.
 * @param coverage  Fraction representing the ratio of alpha to non-alpha pixels.
 */
void FindAverageAlphaIdx(const uint8_t *pixels, int width, int height, float *alpha,
    float *coverage);

/**
 * Calculates a clip region for the image that excludes alpha pixels.
 *
 * @par Algorithm
 * Cross spread from bottom > top, right > left (inside out).
 *
 * @param pixels     Image data to be processed.
 * @param width      Width of the image in pixels.
 * @param height     Height of the image in pixels.
 * @param pixelSize  Size of each pixel. Handles 1 (==2), 3 and 4.
 * @param region     Determined region written here.
 */
void FindClipRegionNonAlpha(const uint8_t* pixels, int width, int height,
    int pixelSize, int region[4]);

/**
 * @param pixels     RGB(a) image to be enhanced.
 * @param width      Width of the image in pixels.
 * @param height     Height of the image in pixels.
 * @param pixelSize  Size of each pixel. Handles 3 and 4.
 */
void SharpenPixels(uint8_t* pixels, int width, int height, int pixelSize);

uint8_t* GL_ScaleBuffer(const uint8_t* pixels, int width, int height,
    int pixelSize, int outWidth, int outHeight);

void* GL_ScaleBufferEx(const void* datain, int width, int height, int pixelSize,
    /*GLint typein,*/ int rowLength, int alignment, int skiprows, int skipPixels,
    int outWidth, int outHeight, /*GLint typeout,*/ int outRowLength, int outAlignment,
    int outSkipRows, int outSkipPixels);

uint8_t* GL_ScaleBufferNearest(const uint8_t* pixels, int width, int height,
    int pixelSize, int outWidth, int outHeight);

/**
 * Works within the given data, reducing the size of the picture to half
 * its original.
 *
 * @param pixels     RGB(A) pixel data to process (in/out).
 * @param width      Width of the final texture, must be power of two.
 * @param height     Height of the final texture, must be power of two.
 * @param pixelSize  Size of a pixel in bytes.
 */
void GL_DownMipmap32(uint8_t* pixels, int width, int height, int pixelSize);

/**
 * Works within the given data, reducing the size of the picture to half
 * its original.
 *
 * @param in        Pixel data to process (paletted, in/out).
 * @param fadedOut  Faded result image.
 * @param width     Width of the final texture, must be power of two.
 * @param height    Height of the final texture, must be power of two.
 * @param fade      Fade factor (0..1).
 */
void GL_DownMipmap8(uint8_t* in, uint8_t* fadedOut, int width, int height, float fade);

dd_bool GL_PalettizeImage(uint8_t *out, int outformat, const res::ColorPalette *palette,
    dd_bool gammaCorrect, const uint8_t *in, int informat, int width, int height);

dd_bool GL_QuantizeImageToPalette(uint8_t *out, int outformat,
    const res::ColorPalette *palette, const uint8_t *in, int informat, int width, int height);

/**
 * Desaturates the texture in the dest buffer by averaging the colour then
 * looking up the nearest match in the palette. Increases the brightness
 * to maximum.
 */
void GL_DeSaturatePalettedImage(uint8_t *buffer, const res::ColorPalette &palette,
    int width, int height);

#endif // DE_GL_IMAGE_MANIPULATION_H
