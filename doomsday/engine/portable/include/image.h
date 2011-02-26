/**\file image.h
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2011 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG_IMAGE_H
#define LIBDENG_IMAGE_H

/**
 * @defgroup imageFlags Image Flags
 */
/*@{*/
#define IMGF_IS_MASKED          0x1
/*@}*/

/**
 * This structure is used with GL_LoadImage. When it is no longer needed
 * it must be discarded with GL_DestroyImage.
 */
typedef struct image_s {
    int width;
    int height;
    int pixelSize;
    int originalBits; /// Bits per pixel in the image file.
    int flags; /// @see imageFlags
    uint8_t* pixels;
} image_t;

/**
 * @defgroup imageConversionFlags Image Conversion Flags.
 */
/*@{*/
#define ICF_UPSCALE_SAMPLE_WRAPH    (0x1)
#define ICF_UPSCALE_SAMPLE_WRAPV    (0x2)
#define ICF_UPSCALE_SAMPLE_WRAP     (ICF_UPSCALE_SAMPLE_WRAPH|ICF_UPSCALE_SAMPLE_WRAPV)
/*@}*/

#endif /* LIBDENG_IMAGE_H */
