/** @file shadow.cpp  Shadow map.
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

#include "gloom/render/shadow.h"

#include <de/glframebuffer.h>
#include <de/gltexture.h>

using namespace de;

namespace gloom {

DE_PIMPL_NOREF(Shadow)
{
    Light::Type   lightType;
    GLTexture     map;
    GLFramebuffer framebuf;

    void init()
    {
        map.setAutoGenMips(false);
        map.setFilter(gfx::Linear, gfx::Linear, gfx::MipNone);
        map.setComparisonMode(gfx::CompareRefToTexture, gfx::LessOrEqual);

        if (lightType == Light::Directional)
        {
            map.setWrap(gfx::ClampToBorder, gfx::ClampToBorder);
            map.setBorderColor(Vec4f(1));
            map.setUndefinedContent(
                GLTexture::Size(2048, 2048),
                GLPixelFormat(GL_DEPTH_COMPONENT16, GL_DEPTH_COMPONENT, GL_FLOAT));
        }
        else if (lightType == Light::Omni)
        {
            map.setWrap(gfx::ClampToEdge, gfx::ClampToEdge);
            for (int i = 0; i < 6; ++i)
            {
                map.setUndefinedContent(gfx::CubeFace(i), GLTexture::Size(512, 512),
                                                GLPixelFormat(GL_DEPTH_COMPONENT16,
                                                              GL_DEPTH_COMPONENT, GL_FLOAT));
            }
        }

        framebuf.configure(GLFramebuffer::Depth, map);
    }
};

Shadow::Shadow(Light::Type lightType)
    : d(new Impl)
{
    setLightType(lightType);
}

void Shadow::setLightType(Light::Type lightType)
{
    d->lightType = lightType;
    d->init();
}

Light::Type Shadow::lightType() const
{
    return d->lightType;
}

GLTexture &Shadow::shadowMap() const
{
    return d->map;
}

GLFramebuffer &Shadow::framebuf()
{
    return d->framebuf;
}

} // namespace gloom
