/** @file shadow.h  Shadow map.
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

#ifndef GLOOM_RENDER_SHADOW_H
#define GLOOM_RENDER_SHADOW_H

#include "gloom/render/light.h"

#include <de/glframebuffer.h>
#include <de/gltexture.h>

namespace gloom {

class Shadow
{
public:
    Shadow(Light::Type lightType);

    void setLightType(Light::Type lightType);

    Light::Type        lightType() const;
    de::GLTexture &    shadowMap() const;
    de::GLFramebuffer &framebuf();

private:
    DE_PRIVATE(d)
};

} // namespace gloom

#endif // GLOOM_RENDER_SHADOW_H
