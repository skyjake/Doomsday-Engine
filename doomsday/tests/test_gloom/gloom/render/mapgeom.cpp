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

template <typename Type>
struct DataBuffer
{
    GLUniform var;
    GLTexture buf;
    Vector2ui size;
    QVector<Type> data;
    Image::Format format;
    uint texelsPerElement; // number of pixels needed to represent one element
    uint maxWidth;

    DataBuffer(const char *uName, Image::Format format, uint texelsPerElement = 1, uint maxWidth = 0)
        : var{uName, GLUniform::Sampler2D}
        , format(format)
        , texelsPerElement(texelsPerElement)
        , maxWidth(maxWidth)
    {
        buf.setAutoGenMips(false);
        buf.setFilter(gl::Nearest, gl::Nearest, gl::MipNone);
        var = buf;
    }

    void init(int count)
    {
        size.x = de::max(4u, uint(std::sqrt(count) + 0.5));
        if (maxWidth) size.x = de::min(maxWidth, size.x);
        size.y = de::max(4u, uint((count + size.x - 1) / size.x));
        data.resize(size.area());
        data.fill(Type{});
    }

    void clear()
    {
        buf.clear();
        data.clear();
        size = Vector2ui();
    }

    void setData(uint index, const Type &value)
    {
        const int x = index % size.x;
        const int y = index / size.x;
        data[x + y*size.x] = value;
    }

    uint32_t append(const Type &value)
    {
        DENG2_ASSERT(maxWidth > 0);
        size.x = maxWidth;
        size.y++;
        const uint32_t oldSize = data.size();
        data << value;
        return oldSize;
    }

    void update()
    {
        buf.setImage(Image{Image::Size(size.x * texelsPerElement, size.y),
                           format,
                           ByteRefArray(data.constData(), sizeof(data[0]) * size.area())});
    }
};

DENG2_PIMPL(MapRender)
{
    const Map *map = nullptr;
    double     currentTime;

    AtlasTexture *       atlas = nullptr;
    MapBuild::TextureIds textures;
    MapBuild::PlaneMapper planeMapper;

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

    Drawable  mapDrawable;
    GLUniform uMvpMatrix        {"uMvpMatrix",      GLUniform::Mat4};
    GLUniform uTex              {"uTex",            GLUniform::Sampler2D};
    GLUniform uCurrentTime      {"uCurrentTime",    GLUniform::Float};
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
            const uint32_t   texId = textureMetrics.append(
                Metrics{{rectf.xywh()}, {Vector4f(rect.width(), rect.height())}});
            textures.insert(i.key(), texId);
        }

        textureMetrics.update();
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
                planes.init(count);
                for (auto i = planeMapper.begin(), end = planeMapper.end(); i != end; ++i)
                {
                    planes.setData(i.value(), float(map->plane(i.key()).point.y));
                }
                planes.update();
            }
        }

        mapDrawable.addBuffer(buf);

        GloomApp::shaders().build(mapDrawable.program(), "gloom.surface")
            << uMvpMatrix << uCurrentTime << uTexelsPerMeter << uTex << textureMetrics.var
            << planes.var << texOffsets.var;
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
}

void MapRender::advanceTime(const TimeSpan &elapsed)
{
    d->currentTime += elapsed;

    // Generate test data.
    {
        for (auto i = d->planeMapper.begin(), end = d->planeMapper.end(); i != end; ++i)
        {
            const float planeY = float(d->map->plane(i.key()).point.y) +
                                 float(std::sin(i.value() + d->currentTime * .1));

            d->planes.setData(i.value(), planeY);
        }

        // Scrolling.

    }
}

void MapRender::render(const ICamera &camera)
{
    d->planes    .update();
    d->texOffsets.update();

    d->uMvpMatrix = camera.cameraModelViewProjection();
    d->mapDrawable.draw();
}

} // namespace gloom
