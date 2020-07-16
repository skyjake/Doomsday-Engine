/** @file environ.h  Environment rendering.
 *
 * @authors Copyright (c) 2015-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef DE_CLIENT_RENDER_ENVIRON_H
#define DE_CLIENT_RENDER_ENVIRON_H

#include <de/gltexture.h>

namespace world { class Subsector; }

namespace render {

/**
 * Environmental rendering and texture maps.
 */
class Environment
{
public:
    Environment();

    void glDeinit();

    const de::GLTexture &defaultReflection() const;

    /**
     * Determines the reflection cube map suitable for an object whose origin lies inside
     * the given @a subsector.
     *
     * @return Reflection cube map.
     */
    const de::GLTexture &reflectionInSubsector(const world::Subsector *subsector) const;

private:
    DE_PRIVATE(d)
};

} // namespace render

#endif // DE_CLIENT_RENDER_ENVIRON_H
