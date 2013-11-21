/** @file image.h  Image objects and related routines.
 *
 * @ingroup resource
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
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

#ifndef DENG_RESOURCE_IMAGE_H
#define DENG_RESOURCE_IMAGE_H

#include "dd_share.h" // gfxmode_t
#include "filehandle.h"
#include <de/String>
#include <de/Vector>
#include <de/size.h>

/// @todo Should not depend on texture-level stuff here.
struct texturevariantspecification_t;
namespace de {
class Texture;
}

namespace res
{
    enum Source {
        None,     ///< Not a valid source.
        Original, ///< An "original".
        External  ///< An "external" replacement.
    };
}

/**
 * @defgroup imageConversionFlags Image Conversion Flags
 * @ingroup flags
 */
/*@{*/
#define ICF_UPSCALE_SAMPLE_WRAPH    (0x1)
#define ICF_UPSCALE_SAMPLE_WRAPV    (0x2)
#define ICF_UPSCALE_SAMPLE_WRAP     (ICF_UPSCALE_SAMPLE_WRAPH|ICF_UPSCALE_SAMPLE_WRAPV)
/*@}*/

/**
 * @defgroup imageFlags Image Flags
 * @ingroup flags
 */
/*@{*/
#define IMGF_IS_MASKED              (0x1)
/*@}*/

typedef struct image_s
{
    /// @ref imageFlags
    int flags;

    /// Indentifier of the color palette used/assumed or @c 0 if none (1-based).
    uint paletteId;

    /// Size of the image in pixels.
    Size2Raw size;

    /// Bytes per pixel in the data buffer.
    int pixelSize;

    /// Pixel color/palette (+alpha) data buffer.
    uint8_t *pixels;
} image_t;

/**
 * Initializes the previously allocated @a image for use.
 * @param image  Image instance.
 */
void Image_Init(image_t* image);

/**
 * Releases image pixel data, but does not delete @a image.
 * @param image  Image instance.
 */
void Image_Destroy(image_t* image);

/**
 * Returns the dimensions of the image in pixels.
 */
de::Vector2i Image_Dimensions(image_t const *image);

/**
 * Returns a textual description of the image.
 *
 * @return Human-friendly description of the image.
 */
de::String Image_Description(image_t const *image);

/**
 * Loads PCX, TGA and PNG images. The returned buffer must be freed
 * with M_Free. Color keying is done if "-ck." is found in the filename.
 * The allocated memory buffer always has enough space for 4-component
 * colors.
 */
uint8_t *Image_LoadFromFile(image_t* image, FileHandle* file);

boolean Image_LoadFromFileWithFormat(image_t* img, const char* format, FileHandle* file);

boolean Image_Save(const image_t *image, const char* filePath);

/// @return  @c true if the image pixel data contains alpha information.
boolean Image_HasAlpha(const image_t* image);

/**
 * Converts the image by converting it to a luminance map and then moving
 * the resultant luminance data into the alpha channel. The color channel(s)
 * are then filled all-white.
 */
void Image_ConvertToAlpha(image_t* image, boolean makeWhite);

/**
 * Converts the image data to grayscale luminance in-place.
 */
void Image_ConvertToLuminance(image_t* image, boolean retainAlpha);

#ifdef __CLIENT__
/// @todo Move into image_t
uint8_t *GL_LoadImage(image_t &image, de::String nativePath);

/// @todo Move into image_t
res::Source GL_LoadExtImage(image_t &image, char const *searchPath, gfxmode_t mode);

/// @todo Move into image_t
res::Source GL_LoadSourceImage(image_t &image, de::Texture const &tex,
    texturevariantspecification_t const &spec);

#endif // __CLIENT__

#endif // DENG_RESOURCE_IMAGE_H
