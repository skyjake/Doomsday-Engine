/** @file idtech1image.cpp
 *
 * @authors Copyright (c) 2016-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "doomsday/res/idtech1image.h"
#include "doomsday/res/patch.h"
#include "doomsday/game.h"

using namespace de;

namespace res {

DE_PIMPL_NOREF(IdTech1Image)
{
    Block pixels; // RGBA_8888
    Size  pixelSize;
    Size  nominalSize;
    Vec2i origin;
};

IdTech1Image::IdTech1Image()
    : d(new Impl)
{}

IdTech1Image::IdTech1Image(const Size &size, const Block &imagePixels, const IByteArray &palette)
    : d(new Impl)
{
    const auto layerSize = size.x * size.y;

    if (imagePixels.size() >= 2 * layerSize)
    {
        d->pixels = Block(imagePixels, 0, layerSize)
                        .mapAsIndices(3, palette, Block(imagePixels, layerSize, layerSize));
    }
    else
    {
        d->pixels = imagePixels.mapAsIndices(3, palette, {{0, 0, 0, 255}});
    }

    d->pixelSize   = size;
    d->nominalSize = size;
}

IdTech1Image::IdTech1Image(const IByteArray &data, const IByteArray &palette, Format format)
    : d(new Impl)
{
    Size const rawSize{320, 200};

    if (format == Automatic)
    {
        // Try to guess which format the data uses.
        if (data.size() == rawSize.x * rawSize.y)
        {
            format = RawVGAScreen;
        }
        else
        {
            format = Patch;
        }
    }

    if (format == RawVGAScreen)
    {
        d->nominalSize = rawSize;
        d->pixelSize   = rawSize;
        d->pixels      = Block(data).mapAsIndices(3, palette, {{0, 0, 0, 255}});
    }
    else
    {
        Patch::Metadata meta;
        const auto      patchData = Patch::load(data, &meta);

        d->pixelSize   = meta.dimensions;
        d->nominalSize = meta.logicalDimensions;
        d->origin      = meta.origin;

        const auto layerSize = patchData.size() / 2;
        d->pixels = Block(patchData, 0, layerSize)
                        .mapAsIndices(3, palette, Block(patchData, layerSize, layerSize));
    }
}

IdTech1Image::IdTech1Image(const IdTech1Image &other)
    : d(new Impl(*other.d))
{}

IdTech1Image::IdTech1Image(IdTech1Image &&moved)
    : d(std::move(moved.d))
{}

IdTech1Image &IdTech1Image::operator=(const IdTech1Image &other)
{
    *d = *other.d;
    return *this;
}

IdTech1Image &IdTech1Image::operator=(IdTech1Image &&moved)
{
    std::swap(d, moved.d);
    return *this;
}

Block &IdTech1Image::pixels()
{
    return d->pixels;
}

IdTech1Image::Size IdTech1Image::pixelSize() const
{
    return d->pixelSize;
}

IdTech1Image::Size IdTech1Image::nominalSize() const
{
    return d->nominalSize;
}

Vec2i IdTech1Image::origin() const
{
    return d->origin;
}

void IdTech1Image::setOrigin(const Vec2i &origin)
{
    d->origin = origin;
}

Block IdTech1Image::pixels() const
{
    return d->pixels;
}

} // namespace res
