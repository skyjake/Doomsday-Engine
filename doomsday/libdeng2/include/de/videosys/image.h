/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2004-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef LIBDENG2_IMAGE_H
#define LIBDENG2_IMAGE_H

#include "../Block"
#include "../Vector"

namespace de
{
    /**
     * Holds an array of image pixel data.
     *
     * @ingroup video
     */ 
    class LIBDENG2_API Image : public Block
    {
    public:
        /// The data buffer passed to the set() method contains the wrong amount of data.
        /// @ingroup errors
        DEFINE_ERROR(DataError);
        
        /// An image filtering operation is done on inapproprite data. @ingroup errors
        DEFINE_ERROR(FilterError);
        
        typedef Vector2ui Dimensions;
        
        /// Pixel format of the image.
        enum Format {
            RGB,
            RGBA
        };

    public:        
        Image();
        
        /** 
         * Defines the image.
         *
         * @param format    Pixel format of the image data.
         * @param dims      Width and height of the image in pixels.
         * @param data      Data buffer containing the pixels of the image.
         * @param dataSize  Number of bytes in the data buffer.
         * @param linePitchBytes  Bytes per each line.
         */
        void set(Format format, const Dimensions& dims, const Byte* data, Size dataSize,
            duint linePitchBytes = 0);
        
        Format format() const;
        
        /// Width of the image.
        duint width() const;
        
        /// Height of the image.
        duint height() const;

        /// Dimensions of the image as a vector.
        Dimensions dimensions() const;

        /// Number of bytes per pixel.
        duint bytesPerPixel() const;
        
        /// Converts an RGB image to RGBA so that the original RGB luminocity 
        /// becomes the alpha value, and the RGB is replaced with white.
        void luminocityToAlpha();
    
    private:
        Format format_;
        Dimensions dims_;
    };
}

#endif /* LIBDENG2_IMAGE_H */
