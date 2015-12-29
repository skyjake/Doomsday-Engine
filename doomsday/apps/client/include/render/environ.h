/** @file environ.h  Environment rendering.
 *
 * @authors Copyright (c) 2015 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef DENG_CLIENT_RENDER_ENVIRON_H
#define DENG_CLIENT_RENDER_ENVIRON_H

#include <de/GLTexture>

class BspLeaf;

namespace render {

/**
 * Environmental rendering and texture maps.
 */
class Environment
{
public:
    Environment();

    de::GLTexture const &defaultReflection() const;

    /**
     * Determines the reflection cube map suitable for an object at a particular
     * position in the current map.
     *
     * @param sector  Sector.
     *
     * @return Reflection cube map.
     */
    de::GLTexture const &reflectionInBspLeaf(BspLeaf const *bspLeaf) const;

private:
    DENG2_PRIVATE(d)
};

} // namespace render

#endif // DENG_CLIENT_RENDER_ENVIRON_H
