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
#include "gloom/render/databuffer.h"
#include "gloom/render/entityrender.h"
#include "gloom/render/lightrender.h"
#include "gloom/render/light.h"
#include "gloom/render/icamera.h"

#include <de/Drawable>
#include <de/GLUniform>

using namespace de;

namespace gloom {

DENG2_PIMPL(MapRender)
{
    MapBuild::TextureIds textures;
    MapBuild::Mapper     planeMapper;
    MapBuild::Mapper     texOffsetMapper;

    QHash<String, Id> loadedTextures; // name => atlas ID

    struct Metrics {
        Vec4f uvRect;
        Vec4f texelSize;
    };
    struct TexOffsetData {
        Vec2f offset;
        Vec2f speed;
    };

    DataBuffer<Metrics>       textureMetrics{"uTextureMetrics", Image::RGBA_32f, 2, 1};
    DataBuffer<float>         planes        {"uPlanes",         Image::R_32f};
    DataBuffer<TexOffsetData> texOffsets    {"uTexOffsets",     Image::RGBA_32f};

    GLUniform uTexelsPerMeter{"uTexelsPerMeter", GLUniform::Float};
    Drawable  surfaces;
    GLProgram dirShadowProgram;
    GLProgram omniShadowProgram;

    EntityRender ents;
    LightRender lights;

    Impl(Public *i) : Base(i)
    {}

    void clear()
    {
        surfaces.clear();
    }

    void loadTexture(const String &name)
    {
        const Id id = self().context().atlas->alloc(self().context().images->image(name));
        loadedTextures.insert(name, id);
    }

    void updateTextureMetrics()
    {
        textureMetrics.clear();
        textures.clear();

        for (auto i = loadedTextures.begin(); i != loadedTextures.end(); ++i)
        {
            // Load up metrics in an array.
            const Rectanglei rect  = self().context().atlas->imageRect(i.value());
            const Rectanglef rectf = self().context().atlas->imageRectf(i.value());
            const uint32_t   texId = textureMetrics.append(
                Metrics{{rectf.xywh()}, {Vec4f(rect.width(), rect.height())}});
            textures.insert(i.key(), texId);
        }

        textureMetrics.update();
    }

    void buildMap()
    {
        const auto &context = self().context();
        const auto *map = context.map;

        surfaces.clear();

        DENG2_ASSERT(map);

        MapBuild builder{*map, textures};
        auto *buf = builder.build();

        planeMapper     = builder.planeMapper();
        texOffsetMapper = builder.texOffsetMapper();

        texOffsets.init(texOffsetMapper.size());
        planes    .init(planeMapper.size());

        // Initialize the plane buffer.
        {
            qDebug() << "PlaneMapper has" << planeMapper.size() << "planes";

            for (auto i = planeMapper.begin(), end = planeMapper.end(); i != end; ++i)
            {
                planes.setData(i.value(), float(map->plane(i.key()).point.y));
            }
        }

        surfaces.addBuffer(buf);

        context.shaders->build(surfaces.program(), "gloom.surface")
            << planes.var
            << uTexelsPerMeter
            << textureMetrics.var
            << texOffsets.var;

        context.shaders->build(dirShadowProgram, "gloom.shadow.surface")
            << planes.var
            << context.uLightMatrix
            << context.lights->uLightDir();

        context.shaders->build(omniShadowProgram, "gloom.shadow_cube.surface")
            << planes.var
            << context.uLightOrigin
            << context.uLightFarPlane
            << context.uLightCubeMatrices;

        context.bindTo(surfaces.program());
        context.bindTo(dirShadowProgram);
        context.bindTo(omniShadowProgram);
    }

    void glInit()
    {
        ents  .glInit(self().context());
        lights.glInit(self().context());

        uTexelsPerMeter = 200;

        // Load some textures.
        for (const char *name :
             {"world.stone", "world.dirt", "world.grass", "world.test", "world.test2"})
        {
            loadTexture(name);
        }
        updateTextureMetrics();

        buildMap();
        ents.createEntities();
        lights.createLights();
    }

    void glDeinit()
    {
        ents  .glDeinit();
        lights.glDeinit();

        for (const Id &texId : loadedTextures)
        {
            self().context().atlas->release(texId);
        }
        loadedTextures.clear();
        textureMetrics.clear();
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
}

LightRender &MapRender::lights()
{
    return d->lights;
}

void MapRender::advanceTime(TimeSpan)
{
    // Update plane heights.
    {
        for (auto i = d->planeMapper.begin(), end = d->planeMapper.end(); i != end; ++i)
        {
            const float planeY = float(context().map->plane(i.key()).point.y); // +
                                 //std::sin(i.value() + now * .1f);

            d->planes.setData(i.value(), planeY);
        }
    }

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

    d->texOffsets.update();
    d->planes.update();
}

void MapRender::render()
{
    d->surfaces.draw();
    d->ents.render();

    d->lights.setShadowRenderCallback([this](const Light &light) {
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

} // namespace gloom
