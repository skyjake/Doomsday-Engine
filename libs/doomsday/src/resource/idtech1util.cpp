/** @file idtech1util.cpp
 *
 * @authors Copyright (c) 2018 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "doomsday/res/idtech1util.h"

#include <de/cstring.h>

namespace res {

using namespace de;

Image8::Image8(const Vec2i &size)
    : size(size)
    , pixels(size.x * size.y * 2)
{
    pixels.fill(0);
}

Image8::Image8(const Vec2i &size, const Block &px)
    : size(size)
    , pixels(px)
{}

void Image8::blit(const Vec2i &pos, const Image8 &img)
{
    // Determine which part of each row will be blitted.
    int start = 0;
    int len   = img.size.x;

    int dx1 = pos.x;
    int dx2 = pos.x + len;

    // Clip the beginning and the end.
    if (dx1 < 0)
    {
        start = -dx1;
        len -= start;
        dx1 = 0;
    }
    if (dx2 > size.x)
    {
        len -= dx2 - size.x;
        dx2 = size.x;
    }
    if (len <= 0) return;

    const int srcLayerSize  = img.layerSize();
    const int destLayerSize = layerSize();

    for (int sy = 0; sy < img.size.y; ++sy)
    {
        const int dy = pos.y + sy;

        if (dy < 0 || dy >= size.y) continue;

        const duint8 *src    = img.row(sy) + start;
        const duint8 *srcEnd = src + len;
        duint8 *      dest   = row(dy) + dx1;

        for (; src < srcEnd; ++src, ++dest)
        {
            if (src[srcLayerSize])
            {
                *dest               = *src;
                dest[destLayerSize] = 255;
            }
        }
    }
}

String wad::nameString(const char *name, dsize maxLen)
{
    dsize len = 0;
    while (len < maxLen && name[len]) { len++; }
    return CString(name, name + len).upper();
}

} // namespace res
