/** @file idtech1image.h
 *
 * @authors Copyright (c) 2016-2018 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#ifndef LIBDOOMSDAY_RESOURCE_IDTECH1IMAGE_H
#define LIBDOOMSDAY_RESOURCE_IDTECH1IMAGE_H

#include <de/ibytearray.h>
#include <de/vector.h>
#include "lumpcatalog.h"

namespace res {

using namespace de;

/**
 * Image that imports its content using Id Tech 1 graphics formats.
 */
class LIBDOOMSDAY_PUBLIC IdTech1Image
{
public:
    enum Format { Automatic, RawVGAScreen, Patch };
    typedef Vec2ui Size;

public:
    IdTech1Image();

    IdTech1Image(const Size &size, const Block &imagePixels, const IByteArray &palette);

    /**
     * Constructs a new Id Tech 1 image. The Image object gets initialized with the
     * RGBA_8888 contents of the image.
     *
     * @param data     Image data. Format detected automatically.
     * @param palette  RGB palette containing color triplets. The size of the palette
     *                 must be big enough to contain all the color indices used in the
     *                 image data.
     */
    IdTech1Image(const IByteArray &data, const IByteArray &palette, Format format = Automatic);

    IdTech1Image(const IdTech1Image &);
    IdTech1Image(IdTech1Image &&);

    IdTech1Image &operator=(const IdTech1Image &);
    IdTech1Image &operator=(IdTech1Image &&);

    Block &pixels();

    /**
     * Returns the image pixels as RGBA_8888.
     */
    Block pixels() const;

    /// Size in pixels.
    Size pixelSize() const;

    /**
     * Size of the image data as declared in its metadata. May not match the actual
     * image size.
     */
    Size nominalSize() const;

    Vec2i origin() const;

    void setOrigin(const Vec2i &origin);

private:
    DE_PRIVATE(d)
};

} // namespace res

#endif // LIBDOOMSDAY_RESOURCE_IDTECH1IMAGE_H
