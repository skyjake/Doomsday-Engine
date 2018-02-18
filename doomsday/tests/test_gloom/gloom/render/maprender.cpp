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

#include "maprender.h"
#include "mapbuild.h"
#include "databuffer.h"
#include "entityrender.h"
#include "../icamera.h"
#include "../../src/gloomapp.h"

#include <de/Drawable>
#include <de/GLUniform>

using namespace de;

namespace gloom {

DENG2_PIMPL(MapRender)
{
    const Map *map = nullptr;
    double     currentTime;

    AtlasTexture *       atlas = nullptr;
    MapBuild::TextureIds textures;
    MapBuild::Mapper     planeMapper;
    MapBuild::Mapper     texOffsetMapper;

    QHash<String, Id> loadedTextures; // name => atlas ID

    struct Metrics {
        Vector4f uvRect;
        Vector4f texelSize;
    };
    DataBuffer<Metrics> textureMetrics{"uTextureMetrics", Image::RGBA_32f, 2, 1};

    DataBuffer<float> planes{"uPlanes", Image::R_32f};

    struct TexOffsetData {
        Vector2f offset;
        Vector2f speed;
    };
    DataBuffer<TexOffsetData> texOffsets{"uTexOffsets", Image::RGBA_32f};

    GLUniform uMvpMatrix        {"uMvpMatrix",      GLUniform::Mat4};
    GLUniform uTex              {"uTex",            GLUniform::Sampler2D};
    GLUniform uCurrentTime      {"uCurrentTime",    GLUniform::Float};
    GLUniform uTexelsPerMeter   {"uTexelsPerMeter", GLUniform::Float};
    Drawable  drawable;

    EntityRender ents;

    Impl(Public *i) : Base(i)
    {}

    void clear()
    {
        drawable.clear();
    }

    void loadTexture(const String &name)
    {
        const Id id = atlas->alloc(GloomApp::images().image(name));
        loadedTextures.insert(name, id);
    }

    void updateTextureMetrics()
    {
        textureMetrics.clear();
        textures.clear();

        for (auto i = loadedTextures.begin(); i != loadedTextures.end(); ++i)
        {
            // Load up metrics in an array.
            const Rectanglei rect  = atlas->imageRect(i.value());
            const Rectanglef rectf = atlas->imageRectf(i.value());
            const uint32_t   texId = textureMetrics.append(
                Metrics{{rectf.xywh()}, {Vector4f(rect.width(), rect.height())}});
            textures.insert(i.key(), texId);
        }

        textureMetrics.update();
    }

    void buildMap()
    {
        drawable.clear();

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

        drawable.addBuffer(buf);

        GloomApp::shaders().build(drawable.program(), "gloom.surface")
            << uMvpMatrix << uCurrentTime << uTexelsPerMeter << uTex << textureMetrics.var
            << planes.var << texOffsets.var;
    }

    void glInit()
    {
        ents.setAtlas(atlas);
        ents.setMap(map);
        ents.glInit();

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
    }

    void glDeinit()
    {
        ents.glDeinit();
        for (const Id &texId : loadedTextures)
        {
            atlas->release(texId);
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

void MapRender::setAtlas(AtlasTexture &atlas)
{
    d->atlas = &atlas;
    d->uTex  = atlas;
}

void MapRender::setMap(const Map &map)
{
    d->clear();
    d->map = &map;
}

void MapRender::glInit()
{
    d->glInit();
}

void MapRender::glDeinit()
{
    d->glDeinit();
}

void MapRender::rebuild()
{
    d->buildMap();
    d->ents.createEntities();
}

void MapRender::advanceTime(const TimeSpan &elapsed)
{
    d->currentTime += elapsed;
    d->uCurrentTime = d->currentTime;

    const float now = float(d->currentTime);

    // Testing: move some planes.
    {
        for (auto i = d->planeMapper.begin(), end = d->planeMapper.end(); i != end; ++i)
        {
            const float planeY = float(d->map->plane(i.key()).point.y) +
                                 std::sin(i.value() + now * .1f);

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
}

void MapRender::render(const ICamera &camera)
{
    d->planes    .update();
    d->texOffsets.update();

    d->uMvpMatrix = camera.cameraModelViewProjection();
    d->drawable.draw();

    d->ents.render(camera);
}

} // namespace gloom
