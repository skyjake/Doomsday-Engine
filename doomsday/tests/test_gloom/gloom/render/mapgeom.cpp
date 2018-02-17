/** @file mapgeom.cpp
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

#include "mapgeom.h"
#include "mapbuild.h"
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
    MapBuild::PlaneMapper planeMapper;

    QHash<String, Id> loadedTextures; // name => atlas ID

    GLTexture metricsBuf; // array of metrics for use in shader
    struct Metrics {
        Vector4f uvRect;
        Vector4f texelSize;
    };
    QVector<Metrics> textureMetrics; // contents for metricsBuf

    GLTexture planeBuf;
    Vector2ui planeBufSize;
    struct PlaneData {
        float y;
    };
    QVector<PlaneData> planeData; // contents for planeBuf

    Drawable  mapDrawable;
    GLUniform uMvpMatrix        {"uMvpMatrix",      GLUniform::Mat4};
    GLUniform uTex              {"uTex",            GLUniform::Sampler2D};
    GLUniform uTextureMetrics   {"uTextureMetrics", GLUniform::Sampler2D};
    GLUniform uPlanes           {"uPlanes",         GLUniform::Sampler2D};
    GLUniform uTexelsPerMeter   {"uTexelsPerMeter", GLUniform::Float};

    Impl(Public *i) : Base(i)
    {}

    void clear()
    {
        mapDrawable.clear();
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
            const uint32_t   texId = textureMetrics.size();
            textureMetrics << Metrics{{rectf.xywh()}, {Vector4f(rect.width(), rect.height())}};
            textures.insert(i.key(), texId);
        }

        // Upload the data to a texture.
        metricsBuf.clear();
        metricsBuf.setAutoGenMips(false);
        metricsBuf.setFilter(gl::Nearest, gl::Nearest, gl::MipNone);
        const Image bufImg{
            Image::Size(2, textureMetrics.size()),
            Image::RGBA_32f,
            ByteRefArray(textureMetrics.constData(), sizeof(Metrics) * textureMetrics.size())};
        metricsBuf.setImage(bufImg);

        uTextureMetrics = metricsBuf;
    }

    void updatePlaneData()
    {
        planeBuf.setImage(
            Image{planeBufSize,
                  Image::R_32f,
                  ByteRefArray(planeData.constData(), sizeof(planeData[0]) * planeBufSize.area())});

        uPlanes = planeBuf;
    }

    void buildMap()
    {
        mapDrawable.clear();

        DENG2_ASSERT(map);

        MapBuild builder{*map, textures};
        auto *buf = builder.build();

        // Initialize the plane buffer.
        {
            planeMapper = builder.planeMapper();

            qDebug() << "PlaneMapper has" << planeMapper.size() << "planes";

            const int count = planeMapper.size();
            if (count)
            {
                planeBufSize.x = de::max(4u, uint(std::sqrt(count) + 0.5));
                planeBufSize.y = de::max(4u, uint((count + planeBufSize.x - 1) / planeBufSize.x));
                planeData.resize(planeBufSize.x * planeBufSize.y);
                planeData.fill(PlaneData{0});

                for (auto i = planeMapper.begin(), end = planeMapper.end(); i != end; ++i)
                {
                    const uint32_t index = i.value();
                    const int x = index % planeBufSize.x;
                    const int y = index / planeBufSize.x;

                    const float planeY = float(map->plane(i.key()).point.y);
                    planeData[x + y*planeBufSize.x].y = planeY;

                    //qDebug() << "Plane" << i.key() << i.value() << planeY;
                }

                planeBuf.setAutoGenMips(false);
                planeBuf.setFilter(gl::Nearest, gl::Nearest, gl::MipNone);

                updatePlaneData();
            }
        }

        mapDrawable.addBuffer(buf);

        GloomApp::shaders().build(mapDrawable.program(), "gloom.surface")
                << uMvpMatrix << uTex << uTextureMetrics << uTexelsPerMeter << uPlanes;
    }

    void glInit()
    {
        //uColor = Vector4f(1, 1, 1, 1);

        uTexelsPerMeter = 200;

        // Load some textures.
        {

    //        height.loadGrayscale(images.image("world.heightmap"));
    //        height.setMapSize(mapSize, heightRange);
    //        heightMap = atlas->alloc(height.toImage());
    //        normalMap = atlas->alloc(height.makeNormalMap());

            loadTexture("world.stone");
            loadTexture("world.dirt");
            loadTexture("world.grass");
            loadTexture("world.test");
            loadTexture("world.test2");

//            textures.insert("world.stone", atlas->alloc(images.image("world.stone")));
    //        Id grass = atlas->alloc(images.image("world.grass"));
//            textures.insert("world.dirt", atlas->alloc(images.image("world.dirt")));
        //        land.setMaterial(HeightField::Stone, atlas->imageRectf(stone));
        //        land.setMaterial(HeightField::Grass, atlas->imageRectf(grass));
        //        land.setMaterial(HeightField::Dirt,  atlas->imageRectf(dirt));

            updateTextureMetrics();
        }

        buildMap();
    }

    void glDeinit()
    {
        for (const Id &texId : loadedTextures)
        {
            atlas->release(texId);
        }
        loadedTextures.clear();
        textureMetrics.clear();
        metricsBuf.clear();
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
}

void MapRender::advanceTime(const TimeSpan &elapsed)
{
    d->currentTime += elapsed;

    // Generate test data.
    {
        const auto bw = d->planeBufSize.x;

        for (auto i = d->planeMapper.begin(), end = d->planeMapper.end(); i != end; ++i)
        {
            const uint32_t index = i.value();
            const int x = index % bw;
            const int y = index / bw;

            const float planeY =
                float(d->map->plane(i.key()).point.y) + float(std::sin(index + d->currentTime * .1));

            d->planeData[x + y*bw].y = planeY;
        }
    }
}

void MapRender::render(const ICamera &camera)
{
    d->updatePlaneData();

    d->uMvpMatrix = camera.cameraModelViewProjection();
    d->mapDrawable.draw();
}

} // namespace gloom
