/** @file idtech1texturelib.cpp  Collection of textures.
 *
 * @authors Copyright (c) 2018 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#include "doomsday/resource/idtech1texturelib.h"
#include "doomsday/resource/patch.h"

#include <de/DataArray>
#include <de/ByteOrder>
#include <de/ByteSubArray>

using namespace de;

namespace res {
namespace wad {

struct Name {
    char name[8];
};

struct Patch {
    dint16  originX;
    dint16  originY;
    duint16 patch;
    dint16  stepdir;
    dint16  colormap;
};

struct TexturesHeader {
    duint count;
    int   offset[1]; // location of Texture
};

struct Texture {
    Name    name;
    int     masked;
    duint16 width;
    duint16 height;
    dint32  obsolete;
    duint16 patchCount;
    Patch   patches[1];
};

struct PatchIndex {
    duint count;
    Name  list[1];
};

static String fixedString(const char *name, dsize maxLen = 8)
{
    dsize len = 0;
    while (len < maxLen && name[len]) len++;
    return String(name, len).toUpper();
}

} // namespace wad

struct Image8
{
    Vec2i size;
    Block pixels;

    Image8(const Vec2i &size)
        : size(size)
        , pixels(size.x * size.y * 2)
    {
        pixels.fill(0);
    }

    Image8(const Vec2i &size, const Block &px)
        : size(size)
        , pixels(px)
    {}

    inline int layerSize() const
    {
        return size.x * size.y;
    }

    inline const duint8 *row(int y) const
    {
        return pixels.data() + size.x * y;
    }

    inline duint8 *row(int y)
    {
        return pixels.data() + size.x * y;
    }

    void blit(const Vec2i &pos, const Image8 &img)
    {
        // Determine which part of each row will be blitted.
        int start = 0;
        int len   = img.size.x;

        int dx1 = pos.x;
        int dx2 = pos.x + len;

        // Clip the beginning and the end.
        if (dx1 < 0)
        {
            start = -dx1;
            len -= start;
            dx1 = 0;
        }
        if (dx2 > size.x)
        {
            len -= dx2 - size.x;
            dx2 = size.x;
        }
        if (len <= 0) return;

        const int srcLayerSize  = img.layerSize();
        const int destLayerSize = layerSize();

        for (int sy = 0; sy < img.size.y; ++sy)
        {
            const int dy = pos.y + sy;

            if (dy < 0 || dy >= size.y) continue;

            const duint8 *src    = img.row(sy) + start;
            const duint8 *srcEnd = src + len;
            duint8 *      dest   = row(dy) + dx1;

            for (; src < srcEnd; ++src, ++dest)
            {
                if (src[srcLayerSize])
                {
                    *dest = *src;
                    dest[destLayerSize] = 255;
                }
            }
        }
    }
};

DENG2_PIMPL(IdTech1TextureLib)
{
    struct Patch {
        Vec2i                origin;
        LumpCatalog::LumpPos patchLump;
        //dint16               stepdir;
        //ByteRefArray         palette;

        Patch(const Vec2i &               origin    = {},
              const LumpCatalog::LumpPos &patchLump = {}/*,
              dint16                      stepdir   = 0,
              const ByteRefArray &        palette   = {}*/)
            : origin(origin)
            , patchLump(patchLump)
            //, stepdir(stepdir)
//            , palette(palette)
        {}
    };

    struct Texture {
        Vec2i              size;
        bool               masked;
        std::vector<Patch> patches;
    };

    const LumpCatalog &    catalog;
    Block                  palette;
    Block                  pnames;
    QHash<String, Texture> textures;

    Impl(Public *i, const LumpCatalog &catalog) : Base(i), catalog(catalog)
    {
        init();
    }

    DataArray<wad::Name> patchIndex() const
    {
        return {ByteSubArray(pnames, 4, pnames.size() - 4)};
    }

    /**
     * Read all the texture patch data and look up the patches in the lump catalog.
     */
    void init()
    {
        //const int COLORMAP_SIZE = 3 * 256;

        palette = catalog.read("PLAYPAL");
        pnames  = catalog.read("PNAMES");

        const auto *patchNames = reinterpret_cast<const wad::PatchIndex *>(pnames.data());

        const auto tex1 = catalog.findAll("TEXTURE1");
        const auto tex2 = catalog.findAll("TEXTURE2");

        QList<LumpCatalog::LumpPos> texturesPos = {tex2.front(), tex1.front()};
        texturesPos += tex2.mid(1);
        texturesPos += tex1.mid(1);

        foreach (const auto &pos, texturesPos)
        {
            const Block lumpData = catalog.read(pos);
            const auto *header   = reinterpret_cast<const wad::TexturesHeader *>(lumpData.data());

            for (duint i = 0; i < fromLittleEndian(header->count); ++i)
            {
                const auto *texture = reinterpret_cast<const wad::Texture *>(
                    lumpData.data() + fromLittleEndian(header->offset[i]));

                Texture tex;
                tex.size   = {fromLittleEndian(texture->width), fromLittleEndian(texture->height)};
                tex.masked = fromLittleEndian(texture->masked) != 0;
                for (duint16 p = 0; p < fromLittleEndian(texture->patchCount); ++p)
                {
                    const auto *patch = &texture->patches[p];
                    qDebug() << "Looking for"
                             << wad::fixedString(patchNames->list[fromLittleEndian(patch->patch)].name);
                    tex.patches.emplace_back(
                        Vec2i{fromLittleEndian(patch->originX), fromLittleEndian(patch->originY)},
                        catalog.find(wad::fixedString(patchNames->list[fromLittleEndian(patch->patch)].name))
                        /*fromLittleEndian(patch->stepdir),
                        ByteRefArray(palette.data() + COLORMAP_SIZE * fromLittleEndian(patch->colormap),
                                     COLORMAP_SIZE)*/);
                    DENG2_ASSERT(tex.patches[p].patchLump.first);
                }
                textures.insert(wad::fixedString(texture->name.name), tex);
            }
        }
    }

    IdTech1Image compose(const String &textureName) const
    {
        const auto found = textures.constFind(textureName);
        if (found == textures.constEnd())
        {
            return IdTech1Image();
        }
        const auto &texture = *found;

        // Blit all the patches into the image.
        Image8 image{texture.size};
        for (const auto &p : texture.patches)
        {
            res::Patch::Metadata meta;
            const auto patchImage = res::Patch::load(catalog.read(p.patchLump), &meta);
            const Vec2i patchSize = meta.dimensions.toVec2i();

            // TODO: patch origin should be taken into account?

            image.blit(p.origin, Image8{patchSize, patchImage});
        }
        return IdTech1Image(image.size.toVec2ui(), image.pixels, palette);
    }
};

IdTech1TextureLib::IdTech1TextureLib(const LumpCatalog &catalog)
    : d(new Impl(this, catalog))
{}

IdTech1Image IdTech1TextureLib::textureImage(const String &name) const
{
    return d->compose(name);
}

} // namespace res
