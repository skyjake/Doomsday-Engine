/** @file idtech1util.h
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

#ifndef IDTECH1UTIL_H
#define IDTECH1UTIL_H

#include "../libdoomsday.h"
#include <de/string.h>
#include <de/vector.h>

namespace res {

using namespace de;

namespace wad {

LIBDOOMSDAY_PUBLIC String nameString(const char *name, dsize maxLen = 8);

} // namespace wad

struct LIBDOOMSDAY_PUBLIC Image8
{
    Vec2i size;
    Block pixels;

    Image8(const Vec2i &size);
    Image8(const Vec2i &size, const Block &px);

    inline int layerSize() const
    {
        return size.x * size.y;
    }

    inline const duint8 *row(int y) const
    {
        return pixels.data() + size.x * y;
    }

    inline duint8 *row(int y)
    {
        return pixels.data() + size.x * y;
    }

    void blit(const Vec2i &pos, const Image8 &img);
};

} // namespace res

#endif // IDTECH1UTIL_H
