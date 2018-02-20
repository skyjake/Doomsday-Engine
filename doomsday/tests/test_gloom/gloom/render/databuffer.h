/** @file databuffer.h
 *
 * @authors Copyright (c) 2018 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * LGPL: http://www.gnu.org/licenses/lgpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser
 * General Public License for more details. You should have received a copy of
 * the GNU Lesser General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#ifndef GLOOM_DATABUFFER_H
#define GLOOM_DATABUFFER_H

#include <de/GLTexture>
#include <de/GLUniform>
#include <de/Image>
#include <de/Vector>

namespace gloom {

template <typename Type>
struct DataBuffer
{
    int elementCount = 0;
    de::GLUniform var;
    de::GLTexture buf;
    de::Vec2ui size;
    QVector<Type> data;
    de::Image::Format format;
    uint texelsPerElement; // number of pixels needed to represent one element
    uint maxWidth;

    DataBuffer(const char *      uName,
               de::Image::Format format,
               uint              texelsPerElement = 1,
               uint              maxWidth         = 0)
        : var{uName, de::GLUniform::Sampler2D}
        , format(format)
        , texelsPerElement(texelsPerElement)
        , maxWidth(maxWidth)
    {
        buf.setAutoGenMips(false);
        buf.setFilter(de::gl::Nearest, de::gl::Nearest, de::gl::MipNone);
        var = buf;
    }

    void init(int count)
    {
        elementCount = count;
        size.x = de::max(4u, uint(std::sqrt(count) + 0.5));
        if (maxWidth) size.x = de::min(maxWidth, size.x);
        size.y = de::max(4u, uint((count + size.x - 1) / size.x));
        data.resize(size.area());
        data.fill(Type{});
    }

    void clear()
    {
        elementCount = 0;
        buf.clear();
        data.clear();
        size = de::Vec2ui();
    }

    void setData(uint index, const Type &value)
    {
        const int x = index % size.x;
        const int y = index / size.x;
        data[x + y*size.x] = value;
    }

    uint32_t append(const Type &value)
    {
        DENG2_ASSERT(maxWidth > 0);
        size.x = maxWidth;
        size.y++;
        elementCount++;
        const uint32_t oldSize = data.size();
        data << value;
        return oldSize;
    }

    void update()
    {
        buf.setImage(de::Image{de::Image::Size(size.x * texelsPerElement, size.y),
                               format,
                               de::ByteRefArray(data.constData(), sizeof(data[0]) * size.area())});
    }
};

} // namespace gloom

#endif // GLOOM_DATABUFFER_H
