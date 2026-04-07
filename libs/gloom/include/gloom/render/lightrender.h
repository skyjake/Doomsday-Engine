/** @file lightrender.h
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

#ifndef GLOOM_LIGHTRENDER_H
#define GLOOM_LIGHTRENDER_H

#include "gloom/render/render.h"

#include <de/drawable.h>

namespace gloom {

using namespace de;

class Light;

/**
 * Renders light source shadow maps and the deferred shading pass.
 */
class LIBGLOOM_PUBLIC LightRender : public Render
{
public:
    LightRender();

    void glInit(Context &) override;
    void glDeinit() override;
    void render() override;
    void advanceTime(TimeSpan) override;

    void bindLighting(GLProgram &);
    void renderLighting();

    typedef std::function<void (const Light &)> RenderFunc;
    void setShadowRenderCallback(RenderFunc callback);

    void createLights();

    GLUniform &uShadowMap();
    GLUniform &uShadowSize();
    GLState &  shadowState();
    GLUniform &uLightDir();
    GLUniform &uViewSpaceLightDir();

    const ICamera *testCamera() const;

private:
    DE_PRIVATE(d)
};

} // namespace gloom

#endif // GLOOM_LIGHTRENDER_H
