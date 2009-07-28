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

#include "de/Image"

using namespace de;

Image::Image() : Block()
{}

void Image::set(Format format, const Dimensions& dims, const Byte* data, Size dataSize, duint linePitchBytes)
{
    format_ = format;
    dims_ = dims;

    duint expectedSize = dims.x * dims.y * bytesPerPixel();
    if(expectedSize > dataSize)
    {
        dims_ = Dimensions();
        throw DataError("Image::set", "Not enough data provided");
    }

    if(!linePitchBytes)
    {
        // Lines have no extra space between between, so just directly copy 
        // the entire buffer.
        Block::set(0, data, expectedSize);
    }
    else
    {
        // Copy line by line, then.
        for(duint y = 0; y < dims.y; ++y)
        {
            Block::set(y * dims.x * bytesPerPixel(), &data[y * linePitchBytes], 
                dims.x * bytesPerPixel());
        }
    }
}

Image::Format Image::format() const
{
    return format_;
}

duint Image::width() const
{
    return dims_.x;
}

duint Image::height() const
{
    return dims_.y;
}

Image::Dimensions Image::dimensions() const
{
    return dims_;
}

duint Image::bytesPerPixel() const
{
    return (format_ == RGB? 3 : format_ == RGBA? 4 : 0);
}

void Image::luminocityToAlpha()
{
    if(format_ != RGB)
    {
        throw FilterError("Image::luminocityToAlpha", "Image must be in RGB format");
    }
    
    duint count = width() * height() * 4;
    Byte* filtered = new Byte[count];
    Byte* dest = filtered;
    const Byte* source = data();
    
    for(duint i = 0; i < count/4; ++i)
    {
        // White for RGB.
        *dest++ = 0xff;
        *dest++ = 0xff;
        *dest++ = 0xff;
        
        /// @todo This is just an average, not luminocity...
        *dest++ = (source[0] + source[1] + source[2]) / 3;
        source += 3;
    }

    set(Image::RGBA, dims_, filtered, count);
    delete filtered;
}
