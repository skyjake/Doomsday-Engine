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

#include <array>

using namespace de;

namespace gloom {

DENG2_PIMPL(MaterialLib)
{
    struct Properties
    {
        Flags flags;
    };
    QHash<String, Properties> materials;

    using TexIds = std::array<Id, TextureMapCount>;

    QHash<String, TexIds> loadedTextures; // name => atlas ID
    Ids                   materialIds;

    struct Metrics {
        struct Texture {
            Vec4f uvRect;
            Vec4f texelSize;
        } texture[TextureMapCount];
    };
    DataBuffer<Metrics> textureMetrics{"uTextureMetrics", Image::RGBA_32f, gl::Static};

    Impl(Public *i) : Base(i)
    {
        // All known materials.
        materials["world.stone"] = Properties{Opaque};
        materials["world.dirt"]  = Properties{Opaque};
        materials["world.grass"] = Properties{Opaque};
        materials["world.test"]  = Properties{Opaque};
        materials["world.test2"] = Properties{Opaque};
        materials["world.water"] = Properties{Transparent};
    }

    void init(Context &)
    {
        // Load materials.
        for (String name : materials.keys())
        {
            loadMaterial(name);
        }
        updateTextureMetrics();
    }

    void deinit()
    {
        for (const TexIds &texIds : loadedTextures)
        {
            for (int t = 0; t < TextureMapCount; ++t)
            {
                if (texIds[t])
                {
                    self().context().atlas[t]->release(texIds[t]);
                }
            }
        }
        loadedTextures.clear();
        textureMetrics.clear();
    }

    void loadMaterial(const String &name)
    {
        static const char *suffix[TextureMapCount] = {
            ".diffuse", ".specgloss", ".emissive", ".normaldisp"
        };

        auto &ctx = self().context();
        TexIds ids {{Id::None, Id::None, Id::None, Id::None}};

        for (int i = 0; i < TextureMapCount; ++i)
        {
            if (ctx.images->has(name + suffix[i]))
            {
                LOG_RES_MSG("Loading texture \"%s\"") << name + suffix[i];
                ids[i] = ctx.atlas[i]->alloc(ctx.images->image(name + suffix[i]));
            }
        }
        loadedTextures.insert(name, ids);
    }

    void updateTextureMetrics()
    {
        const float texelsPerMeter = 200.f;
        auto &ctx = self().context();

        textureMetrics.clear();
        materialIds.clear();
        materialIds.insert(String(), INVALID_INDEX);

        for (auto i = loadedTextures.begin(); i != loadedTextures.end(); ++i)
        {
            Metrics metrics{};

            // Load up metrics in an array.
            for (int j = 0; j < 4; ++j)
            {
                const Id texId = i.value()[j];
                Rectanglei rect;
                Rectanglef rectf;
                if (texId)
                {
                    rect  = ctx.atlas[j]->imageRect(texId);
                    rectf = ctx.atlas[j]->imageRectf(texId);

                    metrics.texture[j] = Metrics::Texture
                    {
                        {rectf.xywh()}, {Vec4f(rect.width(), rect.height(), texelsPerMeter)}
                    };
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
