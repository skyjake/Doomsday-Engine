/** @file maprender.cpp
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

#include "gloom/render/maprender.h"
#include "gloom/render/mapbuild.h"
#include "gloom/render/materiallib.h"
#include "gloom/render/databuffer.h"
#include "gloom/render/entityrender.h"
#include "gloom/render/lightrender.h"
#include "gloom/render/light.h"
#include "gloom/render/icamera.h"
#include "gloom/render/gbuffer.h"

#include <de/Drawable>
#include <de/GLUniform>

#include <array>

using namespace de;

namespace gloom {

DE_PIMPL(MapRender)
{
    MaterialLib       matLib;
    MapBuild::Buffers builtMap;
    MapBuild::Mapper  planeMapper;
    MapBuild::Mapper  texOffsetMapper;

    struct PlaneMoveData {
        float target;
        float initial;
        float startTime;
        float speed;
    };

    struct TexOffsetData {
        Vec2f offset;
        Vec2f speed;
    };

    DataBuffer<PlaneMoveData> planes    {"uPlanes",     Image::RGBA_32f};
    DataBuffer<TexOffsetData> texOffsets{"uTexOffsets", Image::RGBA_32f};

    Drawable  surfaces;
    GLProgram dirShadowProgram;
    GLProgram omniShadowProgram;

    GLProgram            transparentProgram;
    GLState              transparentState;
    GLBuffer::DrawRanges visibleTransparents;
    GLFramebuffer        opaqueFrame;
    GLTexture            opaqueFrameTex;
    GLUniform            uRefractedFrame{"uRefractedFrame", GLUniform::Sampler2D};

    EntityRender ents;
    LightRender  lights;

    Impl(Public *i) : Base(i)
    {}

    void clear()
    {
        surfaces.clear();
    }

    void buildMap()
    {
        auto &context = self().context();
        const auto *map = context.map;

        surfaces.clear();

        DE_ASSERT(map);

        MapBuild builder{*map, matLib};
        builtMap = builder.build();

        planeMapper     = builder.planeMapper();
        texOffsetMapper = builder.texOffsetMapper();

        texOffsets.init(texOffsetMapper.size());
        planes    .init(planeMapper.size());

        // Initialize the plane buffer.
        {
            debug("PlaneMapper has %zu planes", planeMapper.size());

            for (const auto &i : planeMapper)
            {
                PlaneMoveData mov{};
                mov.target = mov.initial = float(map->plane(i.first).point.y);
                planes.setData(i.second, mov);
            }
        }

        surfaces.addBuffer(builtMap.geom[MapBuild::OpaqueGeometry]);

        context.shaders->build(surfaces.program(), "gloom.surface.material")
            << planes.var
            << texOffsets.var
            << matLib.uTextureMetrics();

        context.shaders->build(dirShadowProgram, "gloom.surface.shadow.dir")
            << planes.var
            << texOffsets.var
            << context.uLightMatrix
            << context.uInverseLightMatrix
            << context.lights->uLightDir()
            << context.lights->uShadowSize();

        context.shaders->build(omniShadowProgram, "gloom.surface.shadow.omni")
            << planes.var
            << texOffsets.var
            << context.uLightOrigin
            << context.uLightFarPlane
            << context.uLightCubeMatrices;

        context.shaders->build(transparentProgram, "gloom.surface.transparent")
            << planes.var
            << texOffsets.var
            << matLib.uTextureMetrics()
            << uRefractedFrame
            << context.gbuffer->uViewportSize()
            << context.gbuffer->uGBufferDepth();
        context.lights->bindLighting(transparentProgram);

        transparentState
                .setBlend(false)
                .setDepthTest(true)
                .setDepthWrite(true);

        for (auto *prog :
             {&surfaces.program(), &dirShadowProgram, &omniShadowProgram, &transparentProgram})
        {
            context.bindCamera(*prog).bindMaterials(*prog);
        }
    }

    void glInit()
    {
        matLib.glInit(self().context());
        ents  .glInit(self().context());
        lights.glInit(self().context());

        buildMap();
        ents.createEntities();
        lights.createLights();

        opaqueFrameTex.setAutoGenMips(false);
        opaqueFrameTex.setFilter(gfx::Linear, gfx::Linear, gfx::MipNone);
        opaqueFrameTex.setWrap(gfx::RepeatMirrored, gfx::RepeatMirrored);
        opaqueFrameTex.setUndefinedImage(GLTexture::Size(128, 128), Image::RGB_16f);
        opaqueFrame.configure(GLFramebuffer::Color0, opaqueFrameTex);
        uRefractedFrame = opaqueFrameTex;
    }

    void glDeinit()
    {
        opaqueFrame.configure();

        ents  .glDeinit();
        lights.glDeinit();
        matLib.glDeinit();

        planes.clear();
        texOffsets.clear();
        clear();
    }
};

MapRender::MapRender()
    : d(new Impl(this))
{}

void MapRender::glInit(Context &context)
{
    Render::glInit(context);
    d->glInit();
}

void MapRender::glDeinit()
{
    d->glDeinit();
    Render::glDeinit();
}

void MapRender::rebuild()
{
    d->buildMap();
    d->ents.createEntities();
    d->lights.createLights();

    // Update initial plane heights.
    {
        const double metersPerUnit = context().map->metersPerUnit().y;
        for (const auto &i : d->planeMapper)
        {
            const double planeY = context().map->plane(i.first).point.y;
            Impl::PlaneMoveData mov{};
            mov.target = mov.initial = float(planeY * metersPerUnit);
            d->planes.setData(i.second, mov);
        }
    }
}

LightRender &MapRender::lights()
{
    return d->lights;
}

MaterialLib &MapRender::materialLibrary()
{
    return d->matLib;
}

void MapRender::setPlaneY(ID planeId, double destY, double srcY, double startTime, double speed)
{
    DE_ASSERT(d->planeMapper.contains(planeId));
    const double metersPerUnit = context().map->metersPerUnit().y;
    Impl::PlaneMoveData mov;
    mov.target    = float(destY * metersPerUnit);
    mov.initial   = float(srcY * metersPerUnit);
    mov.speed     = abs(float(speed * metersPerUnit)) * asNumber<float>(sign(mov.target - mov.initial));
    mov.startTime = float(startTime);
    d->planes.setData(d->planeMapper[planeId], mov);
}

void MapRender::advanceTime(TimeSpan elapsed)
{
    DE_ASSERT(isInitialized());

    d->lights.advanceTime(elapsed);

    // Testing: Scroll offsets.
#if 0
    {
        for (auto i = d->texOffsetMapper.begin(), end = d->texOffsetMapper.end(); i != end; ++i)
        {
            d->texOffsets.setData(
                i.value(),
                Impl::TexOffsetData{
                    {0.f, 0.f}, {std::sin(i.value() + now * .25f), i.value() % 2 ? 0.1f : -.1f}});
        }
    }
#endif

    // Sort transparencies back-to-front. (TODO: PVS?)
    if (context().view.camera)
    {
        d->visibleTransparents.clear();

//        const Vec3d eye = context().view.camera->cameraPosition();

        d->visibleTransparents = d->builtMap.transparentRanges;
    }

    d->texOffsets.update();
    d->planes.update();
}

void MapRender::render()
{
    d->surfaces.draw();
    d->ents.render();

    d->lights.setShadowRenderCallback([this](const Light &light)
    {
        d->surfaces.setProgram(light.type() == Light::Directional? d->dirShadowProgram
                                                                 : d->omniShadowProgram);
        d->surfaces.setState(context().lights->shadowState());
        d->surfaces.draw();
        d->surfaces.setProgram(d->surfaces.program());
        d->surfaces.unsetState();

        d->ents.renderShadows(light);
    });

    d->lights.render();
}

void MapRender::renderTransparent()
{
    auto &ctx = context();
    const Vec2ui frameSize = ctx.framebuf->size();

    // Make a copy of the frame containing all the opaque surfaces, to be used for refracted
    // light.
    {
        if (d->opaqueFrameTex.size() != frameSize)
        {
            d->opaqueFrameTex.setUndefinedImage(frameSize, Image::RGB_16f);
        }
        ctx.framebuf->blit(d->opaqueFrame, GLFramebuffer::Color0, gfx::Nearest);
    }

    d->transparentState.setTarget(*ctx.framebuf).setViewport(frameSize);
    d->transparentState.apply();

    d->transparentProgram.beginUse();
    d->builtMap.geom[MapBuild::TransparentGeometry]->draw(&d->visibleTransparents);
    d->transparentProgram.endUse();

    GLState::current().apply();
}

} // namespace gloom
