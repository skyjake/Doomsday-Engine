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

#include "doomsday/res/idtech1texturelib.h"
#include "doomsday/res/idtech1util.h"
#include "doomsday/res/patch.h"

#include <de/byteorder.h>
#include <de/bytesubarray.h>
#include <de/keymap.h>

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

struct TextureIndex {
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

} // namespace wad

DE_PIMPL(IdTech1TextureLib)
{
    struct Patch {
        Vec2i                origin;
        LumpCatalog::LumpPos patchLump;

        Patch(const Vec2i &               origin    = {},
              const LumpCatalog::LumpPos &patchLump = {nullptr, 0})
            : origin(origin)
            , patchLump(patchLump)
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
    KeyMap<String, Texture, String::InsensitiveLessThan> textures;

    Impl(Public *i, const LumpCatalog &catalog) : Base(i), catalog(catalog)
    {
        init();
    }

    /**
     * Read all the texture patch data and look up the patches in the lump catalog.
     */
    void init()
    {
        palette = catalog.read("PLAYPAL");
        pnames  = catalog.read("PNAMES");

        const auto *patchNames = reinterpret_cast<const wad::PatchIndex *>(pnames.data());

        const auto tex1 = catalog.findAll("TEXTURE1");
        const auto tex2 = catalog.findAll("TEXTURE2");

        List<LumpCatalog::LumpPos> texturesPos = {tex2.front(), tex1.front()};
        texturesPos << tex2.mid(1);
        texturesPos << tex1.mid(1);

        for (const auto &pos : texturesPos)
        {
            const Block lumpData = catalog.read(pos);
            const auto *header   = reinterpret_cast<const wad::TextureIndex *>(lumpData.data());

            for (duint i = 0; i < fromLittleEndian(header->count); ++i)
            {
                const auto *texture = reinterpret_cast<const wad::Texture *>(
                    lumpData.data() + fromLittleEndian(header->offset[i]));

                const String textureName{wad::nameString(texture->name.name)};

                if (!textures.contains(textureName))
                {
                    Texture tex;
                    tex.size   = {fromLittleEndian(texture->width), fromLittleEndian(texture->height)};
                    tex.masked = fromLittleEndian(texture->masked) != 0;
                    for (duint16 p = 0; p < fromLittleEndian(texture->patchCount); ++p)
                    {
                        const auto *patch = &texture->patches[p];
                        tex.patches.emplace_back(
                            Vec2i{fromLittleEndian(patch->originX), fromLittleEndian(patch->originY)},
                            catalog.find(wad::nameString(patchNames->list[fromLittleEndian(patch->patch)].name))
                            /*fromLittleEndian(patch->stepdir),
                            ByteRefArray(palette.data() + COLORMAP_SIZE * fromLittleEndian(patch->colormap),
                                         COLORMAP_SIZE)*/);
                        DE_ASSERT(tex.patches[p].patchLump.first);
                    }
                    textures.insert(textureName, tex);
                }
            }
        }
    }

    IdTech1Image composeTexture(const String &textureName) const
    {
        const auto found = textures.find(textureName);
        if (found == textures.end())
        {
            return IdTech1Image();
        }
        const auto &texture = found->second;

        // Blit all the patches into the image.
        Image8 image{texture.size};
        for (const auto &p : texture.patches)
        {
            res::Patch::Metadata meta;
            const auto patchImage = res::Patch::load(catalog.read(p.patchLump), &meta);
            const Vec2i patchSize = meta.dimensions.toVec2i();

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
    return d->composeTexture(name);
}

} // namespace res
