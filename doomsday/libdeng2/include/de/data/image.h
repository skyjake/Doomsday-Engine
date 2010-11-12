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

#include "de/deng.h"
#include <QImage>

namespace de
{
    /**
     * Image-related operations.
     *
     * @ingroup data
     */ 
    class LIBDENG2_API Image
    {
    public:
        /// An image filtering operation is done on inapproprite data. @ingroup errors
        DEFINE_ERROR(FilterError);
                      
    public:
        /**
         * Converts an RGB image to RGBA so that the original RGB luminocity
         * becomes the alpha value, and the RGB is replaced with white.
         *
         * @param image  Input image.
         *
         * @return  Converted image.
         */
        static QImage luminocityToAlpha(const QImage& image);
    };
}

#endif /* LIBDENG2_IMAGE_H */
