/** @file idtech1image.h
 *
 * @authors Copyright (c) 2016 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef DENG_CLIENT_RESOURCE_IDTECH1IMAGE_H
#define DENG_CLIENT_RESOURCE_IDTECH1IMAGE_H

#include <de/Image>

/**
 * Image that imports its content using Id Tech 1 graphics formats.
 */
class IdTech1Image : public de::Image
{
public:
    enum Format {
        Automatic,
        RawVGAScreen,
        Patch
    };

public:
    /**
     * Constructs a new Id Tech 1 image. The Image object gets initialized with the
     * RGBA_8888 contents of the image.
     *
     * @param data     Image data. Format detected automatically.
     * @param palette  RGB palette containing color triplets. The size of the palette
     *                 must be big enough to contain all the color indices used in the
     *                 image data.
     */
    IdTech1Image(de::IByteArray const &data, de::IByteArray const &palette,
                 Format format = Automatic);

    de::Vector2i origin() const;

    /**
     * Size of the image data as declared in its metadata. May not match the actual
     * image size.
     */
    Size nominalSize() const;

private:
    DENG2_PRIVATE(d)
};

#endif // DENG_CLIENT_RESOURCE_IDTECH1IMAGE_H
