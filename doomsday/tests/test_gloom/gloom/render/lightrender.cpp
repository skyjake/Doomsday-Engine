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
    RenderFunc                        callback;
    GLState                           state;
    std::unique_ptr<Light>            skyLight;
    QHash<ID, std::shared_ptr<Light>> lights;
    GLProgram                         surfaceProgram;
    GLProgram                         entityProgram;

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

        auto &ctx = self().context();
        ctx.shaders->build(surfaceProgram, "gloom.shadow.surface") << ctx.uLightMatrix;
        ctx.shaders->build(entityProgram,  "gloom.shadow.entity")  << ctx.uLightMatrix;
    }

    void glDeinit()
    {
        skyLight.reset();
    }
};

LightRender::LightRender()
    : d(new Impl(this))
{}

void LightRender::glInit(Context &context)
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
    //auto &sg = *d->shadowGeometry;

    //sg.setProgram(d->program);
    //sg.setState(d->state);

    for (auto *light : {d->skyLight.get()})
    {
        light->framebuf().clear(GLFramebuffer::Depth);

        d->state.setTarget(light->framebuf())
                .setViewport(Rectangleui::fromSize(light->framebuf().size()));

        context().uLightMatrix = light->lightMatrix();

        d->callback(*light);
//        sg.draw();
    }

    //sg.unsetState();
    //sg.setProgram(d->shadowGeometry->program());
}

void LightRender::setShadowRenderCallback(RenderFunc callback)
{
    d->callback = callback;
}

//void gloom::LightRender::setShadowGeometry(Drawable &sg)
//{
//    d->shadowGeometry = &sg;
//}

void LightRender::createLights()
{

}

GLTexture &LightRender::shadowMap()
{
    return d->skyLight->shadowMap();
}

GLProgram &LightRender::surfaceProgram()
{
    return d->surfaceProgram;
}

GLProgram &LightRender::entityProgram()
{
    return d->entityProgram;
}

GLState &LightRender::shadowState()
{
    return d->state;
}

} // namespace gloom
