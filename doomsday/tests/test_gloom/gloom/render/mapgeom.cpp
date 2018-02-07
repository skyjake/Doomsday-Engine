#include "mapgeom.h"
#include "mapbuild.h"
#include "../icamera.h"
#include "../../src/gloomapp.h"

#include <de/Drawable>
#include <de/GLUniform>

using namespace de;

namespace gloom {

DENG2_PIMPL(MapGeom)
{
    const Map *   map   = nullptr;
    AtlasTexture *atlas = nullptr;
    MapBuild::TextureIds textures;

    GLTexture metricsBuf;
    QHash<String, Id> loadedTextures;
    struct Metrics {
        Vector4f uvRect;
        Vector4f texelSize;
    };
    QVector<Metrics> textureMetrics;

    Drawable  mapDrawable;
    GLUniform uMvpMatrix        {"uMvpMatrix",      GLUniform::Mat4};
    GLUniform uTex              {"uTex",            GLUniform::Sampler2D};
    GLUniform uTextureMetrics   {"uTextureMetrics", GLUniform::Sampler2D};
    //GLUniform uColor    {"uColor",     GLUniform::Vec4};

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
        const Image bufData(
            Image::Size(2, textureMetrics.size()),
            Image::RGBA_32f,
            ByteRefArray(textureMetrics.constData(), sizeof(Metrics) * textureMetrics.size()));
        metricsBuf.setImage(bufData);

        uTextureMetrics = metricsBuf;
    }

    void glInit()
    {
        //uColor = Vector4f(1, 1, 1, 1);

        // Load some textures.
        {

    //        height.loadGrayscale(images.image("world.heightmap"));
    //        height.setMapSize(mapSize, heightRange);
    //        heightMap = atlas->alloc(height.toImage());
    //        normalMap = atlas->alloc(height.makeNormalMap());

            loadTexture("world.stone");
            loadTexture("world.dirt");

//            textures.insert("world.stone", atlas->alloc(images.image("world.stone")));
    //        Id grass = atlas->alloc(images.image("world.grass"));
//            textures.insert("world.dirt", atlas->alloc(images.image("world.dirt")));
        //        land.setMaterial(HeightField::Stone, atlas->imageRectf(stone));
        //        land.setMaterial(HeightField::Grass, atlas->imageRectf(grass));
        //        land.setMaterial(HeightField::Dirt,  atlas->imageRectf(dirt));

            updateTextureMetrics();
        }

        DENG2_ASSERT(map);
        auto *buf = MapBuild(*map, textures).build();
        mapDrawable.addBuffer(buf);

        GloomApp::shaders().build(mapDrawable.program(), "gloom.surface")
                << uMvpMatrix << uTex << uTextureMetrics;
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

MapGeom::MapGeom()
    : d(new Impl(this))
{}

void MapGeom::setAtlas(AtlasTexture &atlas)
{
    d->atlas = &atlas;
    d->uTex  = atlas;
}

void MapGeom::setMap(const Map &map)
{
    d->clear();
    d->map = &map;
}

void MapGeom::glInit()
{
    d->glInit();
}

void MapGeom::glDeinit()
{
    d->glDeinit();
}

void MapGeom::render(const ICamera &camera)
{
    d->uMvpMatrix = camera.cameraModelViewProjection();
    d->mapDrawable.draw();
}

} // namespace gloom
