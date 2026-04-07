/** @file store.h  Store.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2015 Daniel Swanson <danij@dengine.net>
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
#ifndef DE_CLIENT_RENDER_STORE_H
#define DE_CLIENT_RENDER_STORE_H

#include <de/vector.h>

/**
 * Geometry backing store (arrays).
 * @todo Replace with GLBuffer -ds
 */
struct Store
{
    de::Vec3f *posCoords    = nullptr;
    de::Vec4ub *colorCoords = nullptr;
    de::Vec2f *texCoords[2];
    de::Vec2f *modCoords    = nullptr;

    Store();
    ~Store();

    void rewind();

    void clear();

    de::duint allocateVertices(de::duint count);

private:
    de::duint _vertCount = 0;
    de::duint _vertMax   = 0;
};

#endif // DE_CLIENT_RENDER_STORE_H
