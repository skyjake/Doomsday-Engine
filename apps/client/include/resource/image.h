/** @file image.h  Image objects and related routines.
 *
 * @ingroup resource
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef DE_RESOURCE_IMAGE_H
#define DE_RESOURCE_IMAGE_H

#ifndef __CLIENT__
#  error "resource/image.h is for the client only"
#endif

#include "dd_share.h" // gfxmode_t
#include <doomsday/filesys/filehandle.h>
#include <de/legacy/size.h>
#include <de/image.h>
#include <de/string.h>
#include <de/vector.h>

/// @todo Should not depend on texture-level stuff here.
class TextureVariantSpec;
class ClientTexture;

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

struct image_t
{
    typedef de::Vec2ui Size;

    /// @ref imageFlags
    int flags = 0;

    /// Identifier of the color palette used/assumed or @c 0 if none (1-based).
    uint paletteId = 0;

    /// Size of the image in pixels.
    Size size;

    /// Bytes per pixel in the data buffer.
    int pixelSize = 0;

    /// Pixel color/palette (+alpha) data buffer.
    uint8_t *pixels = nullptr;
};

/**
 * Initializes the previously allocated @a image for use.
 * @param image  Image instance.
 */
void Image_Init(image_t &image);

void Image_InitFromImage(image_t &image, const de::Image &guiImage);

/**
 * Releases image pixel data, but does not delete @a image.
 * @param image  Image instance.
 */
void Image_ClearPixelData(image_t &image);

/**
 * Returns the size of the image in pixels.
 */
image_t::Size Image_Size(const image_t &image);

/**
 * Returns a textual description of the image.
 *
 * @return Human-friendly description of the image.
 */
de::String Image_Description(const image_t &image);

/**
 * Loads PCX, TGA and PNG images. The returned buffer must be freed
 * with M_Free. Color keying is done if "-ck." is found in the filename.
 * The allocated memory buffer always has enough space for 4-component
 * colors.
 */
uint8_t *Image_LoadFromFile(image_t &image, res::FileHandle &file);

bool Image_LoadFromFileWithFormat(image_t &image, const char *format, res::FileHandle &file);

bool Image_Save(const image_t &image, const char *filePath);

/// @return  @c true if the image pixel data contains alpha information.
bool Image_HasAlpha(const image_t &image);

/**
 * Converts the image by converting it to a luminance map and then moving
 * the resultant luminance data into the alpha channel. The color channel(s)
 * are then filled all-white.
 */
void Image_ConvertToAlpha(image_t &image, bool makeWhite = false);

/**
 * Converts the image data to grayscale luminance in-place.
 */
void Image_ConvertToLuminance(image_t &image, bool retainAlpha = true);

/// @todo Move into image_t
uint8_t *GL_LoadImage(image_t &image, const de::String& nativePath);

/// @todo Move into image_t
res::Source GL_LoadExtImage(image_t &image, const char *searchPath, gfxmode_t mode);

/// @todo Move into image_t
res::Source GL_LoadSourceImage(image_t &image, const ClientTexture &tex,
                               const TextureVariantSpec &spec);

#endif // DE_RESOURCE_IMAGE_H
