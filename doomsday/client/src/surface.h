/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2009, 2010 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDENG2_SURFACE_H
#define LIBDENG2_SURFACE_H

#include "../Vector"

namespace de
{
    class Image;
    
    /**
     * Graphics surface. The video subsystem will define its own drawing surfaces
     * based on this.
     *
     * @ingroup video
     */
    class LIBDENG2_API Surface
    {
    public:
        typedef Vector2ui Size;
        
        /// Conversion of the drawing surface to an image failed. @ingroup errors
        DEFINE_ERROR(ImageError);
        
    public:
        Surface(const Size& size);
        
        virtual ~Surface();

        /**
         * Returns the size of the drawing surface.
         */
        const Size& size() const { return _size; }
        
        /**
         * Sets the size of the drawing surface.
         */
        virtual void setSize(const Size& size);

        /**
         * Determines the color depth of the surface.
         *
         * @return  Bits per pixel.
         */
        virtual duint colorDepth() const = 0;

        /**
         * Captures the contents of the drawing surface and stores them into an image.
         *
         * @return  Captured image. Caller gets ownership.
         */ 
        Image* toImage() const;
        
    private:
        Size _size;
    };
}

#endif /* LIBDENG2_SURFACE_H */
