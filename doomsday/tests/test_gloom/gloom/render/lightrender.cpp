/** @file lightrender.cpp
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

#include "gloom/render/lightrender.h"
#include "gloom/render/light.h"

using namespace de;

namespace gloom {

DENG2_PIMPL(LightRender)
{
    Drawable *                        shadowGeometry = nullptr;
    GLState                           state;
    GLProgram                         program;
    std::unique_ptr<Light>            skyLight;
    QHash<ID, std::shared_ptr<Light>> lights;
    GLUniform                         uLightMatrix{"uLightMatrix", GLUniform::Mat4};

    Impl(Public *i) : Base(i)
    {}

    void glInit()
    {
        state.setBlend(false);
        state.setDepthTest(true);
        state.setDepthWrite(true);
        state.setColorMask(gl::WriteNone);
        state.setCull(gl::Front);

        skyLight.reset(new Light);

        self().context().shaders->build(program, "gloom.shadow") << uLightMatrix;
    }

    void glDeinit()
    {
        skyLight.reset();
    }
};

LightRender::LightRender()
    : d(new Impl(this))
{}

void LightRender::glInit(const Context &context)
{
    Render::glInit(context);
    d->glInit();
}

void LightRender::glDeinit()
{
    d->glDeinit();
    Render::glDeinit();
}

void LightRender::render()
{
    auto &sg = *d->shadowGeometry;

    sg.setProgram(d->program);
    sg.setState(d->state);

    for (auto *light : {d->skyLight.get()})
    {
        d->state.setTarget(light->framebuf())
                .setViewport(Rectangleui::fromSize(light->framebuf().size()));
        d->uLightMatrix = light->lightMatrix();
        sg.draw();
    }

    sg.unsetState();
    sg.setProgram(d->shadowGeometry->program());
}

void gloom::LightRender::setShadowGeometry(Drawable &sg)
{
    d->shadowGeometry = &sg;
}

void LightRender::createLights()
{

}

} // namespace gloom
