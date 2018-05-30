/** @file materiallib.cpp  Material library.
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

#include "gloom/render/materiallib.h"
#include "gloom/render/mapbuild.h"
#include "gloom/render/defs.h"
#include "gloom/render/databuffer.h"

#include <de/FileSystem>
#include <de/ImageFile>
#include <de/filesys/AssetObserver>
#include <array>

using namespace de;

namespace gloom {

DENG2_PIMPL(MaterialLib)
, DENG2_OBSERVES(filesys::AssetObserver, Availability)
{
    struct Properties {
        Flags   flags{Opaque};
        float   texelsPerMeter{100.f};
        float   aspectRatio{1.f};
        duint32 metricsFlags{0}; // copied to shader in texture metrics
    };
    struct Metrics {
        struct Texture {
            Vec4f uvRect;
            Vec4f texelSize;
        } texture[TextureMapCount];
    };
    using TexIds = std::array<Id, TextureMapCount>;

    filesys::AssetObserver        observer{"material\\..*"};
    QHash<String, Properties>     materials;
    mutable QHash<String, TexIds> loadedTextures; // name => atlas ID
    Ids                           materialIds;
    DataBuffer<Metrics>           textureMetrics{"uTextureMetrics", Image::RGBA_32f, gl::Static};

    Impl(Public *i) : Base(i)
    {
        observer.audienceForAvailability() += this;

/*        // All known materials.
        materials["world.stone"] = Properties{Opaque, 200.f, 0};
        materials["world.dirt"]  = Properties{Opaque, 200.f, 0};
        materials["world.grass"] = Properties{Opaque, 200.f, 0};
        materials["world.test"]  = Properties{Opaque, 200.f, 0};
        materials["world.test2"] = Properties{Opaque, 200.f, 0};
        materials["world.metal"] = Properties{Opaque, 200.f, 0};
        materials["world.water"] = Properties{Transparent, 100.f, 1};*/
    }

    void assetAvailabilityChanged(const String &identifier, filesys::AssetObserver::Event event) override
    {
        LOG_RES_MSG("Material asset \"%s\" is now %s")
                << identifier
                << (event == filesys::AssetObserver::Added? "available" :
                                                            "unavailable");

        const DotPath materialId = DotPath(identifier).beginningOmitted();

        if (event == filesys::AssetObserver::Added)
        {
            const auto &asset = App::asset(identifier);
            addMaterial(materialId, asset);
        }
        else
        {
            removeMaterial(materialId);
        }
    }

    void init(Context &)
    {
        // Load materials.
        /*for (auto i = materials.constBegin(); i != materials.constEnd(); ++i)
        {
            loadMaterial(i.key());
        }*/
        updateTextureMetrics();
    }

    void deinit()
    {
        while (!loadedTextures.isEmpty())
        {
            unloadTextures(loadedTextures.begin().key());
        }
        textureMetrics.clear();
    }

    void addMaterial(const DotPath &name, const Package::Asset &asset)
    {
        qDebug() << "Adding material:" << name;
        qDebug() << asset.accessedRecord().asText().toLatin1().constData();

        Properties props;
        props.aspectRatio    = asset.getf("aspectRatio", 1.f);
        props.texelsPerMeter = asset.getf("ppm", 100.f);

        materials.insert(name, props);

        /*
        static const char *suffix[TextureMapCount] = {
            ".diffuse", ".specgloss", ".emissive", ".normaldisp"
        };

        auto &ctx = self().context();
        TexIds ids {{Id::None, Id::None, Id::None, Id::None}};

        if (ctx.images->has(name + ".metallic"))
        {
            // Convert to specular/gloss.
            Image baseColor    = ctx.images->image(name + ".basecolor");
            Image invMetallic  = ctx.images->image(name + ".metallic").invertedColor(); // grayscale

            Image normal       = ctx.images->image(name + ".normal");
            Image gloss        = ctx.images->image(name + ".roughness").invertedColor(); // grayscale
            Image diffuse      = baseColor.multiplied(invMetallic);

            QImage defaultSpecular(QSize(invMetallic.width(), invMetallic.height()),
                                   QImage::Format_ARGB32);
            defaultSpecular.fill(QColor(56, 56, 56, 255));

            Image specGloss = invMetallic.mixed(baseColor, defaultSpecular).withAlpha(gloss);

            ids[Diffuse]            = ctx.atlas[Diffuse]->alloc(diffuse);
            ids[SpecularGloss]      = ctx.atlas[SpecularGloss]->alloc(specGloss);
            ids[NormalDisplacement] = ctx.atlas[NormalDisplacement]->alloc(normal);
        }

        for (int i = 0; i < TextureMapCount; ++i)
        {
            if (ctx.images->has(name + suffix[i]))
            {
                LOG_RES_MSG("Loading texture \"%s\"") << name + suffix[i];
                ids[i] = ctx.atlas[i]->alloc(ctx.images->image(name + suffix[i]));
            }
        }
        loadedTextures.insert(name, ids);
        */
    }

    void removeMaterial(const DotPath &materialId)
    {
        unloadTextures(materialId);
        materials.remove(materialId);
    }

    const Image getImage(const Package::Asset &asset, const String &key) const
    {
        return FS::locate<const ImageFile>(asset.absolutePath(key)).image();
    }

    void loadTextures(const String &materialId) const
    {
        static const char *texName[TextureMapCount] = {
            "diffuse", "specgloss", "emissive", "normal"
        };

        auto &      ctx   = self().context();
        const auto &asset = App::asset("material." + materialId);

        TexIds ids{{Id::None, Id::None, Id::None, Id::None}};

        if (asset.has("metallic"))
        {
            LOG_RES_MSG("Loading metallic/roughness textures of \"%s\"") << materialId;

            // Convert to specular/gloss.
            Image baseColor    = getImage(asset, "basecolor");
            Image invMetallic  = getImage(asset, "metallic").invertedColor(); // grayscale

            Image normal       = getImage(asset, "normal");
            Image gloss        = getImage(asset, "roughness").invertedColor(); // grayscale
            Image diffuse      = baseColor.multiplied(invMetallic);

            QImage defaultSpecular(QSize(invMetallic.width(), invMetallic.height()),
                                   QImage::Format_ARGB32);
            defaultSpecular.fill(QColor(56, 56, 56, 255));

            Image specGloss = invMetallic.mixed(baseColor, defaultSpecular).withAlpha(gloss);

            ids[Diffuse]            = ctx.atlas[Diffuse]->alloc(diffuse);
            ids[SpecularGloss]      = ctx.atlas[SpecularGloss]->alloc(specGloss);
            ids[NormalDisplacement] = ctx.atlas[NormalDisplacement]->alloc(normal);
        }

        for (int i = 0; i < TextureMapCount; ++i)
        {
            if (asset.has(texName[i]))
            {
                LOG_RES_MSG("Loading texture \"%s\"") << materialId.concatenateMember(texName[i]);
                ids[i] = ctx.atlas[i]->alloc(getImage(asset, texName[i]));
            }
        }

        loadedTextures.insert(materialId, ids);
    }

    void unloadTextures(const String &materialId) const
    {
        auto loaded = loadedTextures.find(materialId);
        if (loaded != loadedTextures.end())
        {
            const auto &texIds = loaded.value();
            for (int i = 0; i < TextureMapCount; ++i)
            {
                if (texIds[i])
                {
                    self().context().atlas[i]->release(texIds[i]);
                }
            }
            loadedTextures.erase(loaded);
        }
    }

    void updateTextureMetrics()
    {
        auto &ctx = self().context();

        textureMetrics.clear();
        materialIds.clear();
        materialIds.insert(String(), InvalidIndex);

        for (auto i = loadedTextures.begin(); i != loadedTextures.end(); ++i)
        {
            Metrics metrics{};

            // Load up metrics in an array.
            for (int j = 0; j < 4; ++j)
            {
                DENG2_ASSERT(materials.contains(i.key()));

                const auto &props = materials[i.key()];
                const Id    texId = i.value()[j];
                Rectanglei rect;
                Rectanglef rectf;
                if (texId)
                {
                    rect  = ctx.atlas[j]->imageRect(texId);
                    rectf = ctx.atlas[j]->imageRectf(texId);

                    float tmFlags;
                    std::memcpy(&tmFlags, &props.metricsFlags, sizeof(tmFlags));

                    qDebug() << i.key() << texId.asText()
                             << Vec3f(rect.width(), rect.height(), props.texelsPerMeter).asText();

                    metrics.texture[j] = Metrics::Texture{{rectf.xywh()},
                                                          {Vec4f(rect.width(),
                                                                 rect.height(),
                                                                 props.texelsPerMeter,
                                                                 tmFlags)}};
                }
            }

            const uint32_t matId = textureMetrics.append(metrics);
            materialIds.insert(i.key(), matId);
        }

        textureMetrics.update();
    }
};

MaterialLib::MaterialLib()
    : d(new Impl(this))
{}

void MaterialLib::glInit(Context &context)
{
    Render::glInit(context);
    d->init(context);
}

void MaterialLib::glDeinit()
{
    d->deinit();
    Render::glDeinit();
}

void MaterialLib::render()
{}

void MaterialLib::loadMaterials(const StringList &materials) const
{
    // Unload unnecessary materials.
    {
        QMutableHashIterator<String, Impl::TexIds> iter(d->loadedTextures);
        while (iter.hasNext())
        {
            iter.next();
            if (!materials.contains(iter.key()))
            {
                d->unloadTextures(iter.key());
            }
        }
    }

    // Load the requested new materials.
    for (const String &materialId : materials)
    {
        if (!d->loadedTextures.contains(materialId))
        {
            d->loadTextures(materialId);
        }
    }

    d->updateTextureMetrics();
}

const MaterialLib::Ids &MaterialLib::materials() const
{
    return d->materialIds;
}

bool MaterialLib::isTransparent(const String &matId) const
{
    auto found = d->materials.constFind(matId);
    if (found != d->materials.constEnd())
    {
        return (found.value().flags & Transparent) != 0;
    }
    return false;
}

GLUniform &MaterialLib::uTextureMetrics()
{
    return d->textureMetrics.var;
}

} // namespace glo0om
