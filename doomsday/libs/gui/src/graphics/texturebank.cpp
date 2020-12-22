/** @file texturebank.cpp
 *
 * @authors Copyright (c) 2014-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/texturebank.h"

#include <de/hash.h>

namespace de {

DE_PIMPL_NOREF(TextureBank::ImageSource)
{
    DotPath sourcePath;
    int atlasId = 0;
};

TextureBank::ImageSource::ImageSource(const DotPath &sourcePath) : d(new Impl)
{
    d->sourcePath = sourcePath;
}

TextureBank::ImageSource::ImageSource(int atlasId, const DotPath &sourcePath) : d(new Impl)
{
    d->sourcePath = sourcePath;
    d->atlasId    = atlasId;
}

const DotPath &TextureBank::ImageSource::sourcePath() const
{
    return d->sourcePath;
}

int TextureBank::ImageSource::atlasId() const
{
    return d->atlasId;
}

DE_PIMPL(TextureBank)
{
    struct TextureData : public IData
    {
        Impl *d;
        AtlasId atlasId;
        Id _id { Id::None };
        std::unique_ptr<Image> pendingImage;

        TextureData(AtlasId atlasId, const Image &image, Impl *owner) : d(owner), atlasId(atlasId)
        {
            if (image)
            {
                if (d->atlases.contains(atlasId))
                {
                    _id = d->atlases[atlasId]->alloc(image);
                }
                else
                {
                    pendingImage.reset(new Image(image));
                }
            }

            /// @todo Reduce size if doesn't fit? Can be expanded when requested for use.
        }

        ~TextureData()
        {
            if (_id)
            {
                d->pathForAtlasId.remove(_id);
                d->atlases[atlasId]->release(_id);
            }
        }

        const Id &id()
        {
            if (pendingImage && d->atlases.contains(atlasId))
            {
                _id = d->atlases[atlasId]->alloc(*pendingImage);
                pendingImage.reset();
            }
            return _id;
        }
    };

    Hash<int, IAtlas *>                  atlases;
    Hash<Id, std::pair<AtlasId, String>> pathForAtlasId; // reverse lookup

    Impl(Public *i) : Base(i) {}

    ~Impl()
    {
        // Get rid of items before the reverse lookup hash is destroyed.
        self().clear();
    }
};

TextureBank::TextureBank(const char *nameForLog, const Flags &flags)
    : Bank(nameForLog, flags), d(new Impl(this))
{}

void TextureBank::setAtlas(IAtlas *atlas)
{
    setAtlas(0, atlas);
}

void TextureBank::setAtlas(AtlasId atlasId, IAtlas *atlas)
{
    d->atlases.insert(atlasId, atlas);
}

IAtlas *TextureBank::atlas(AtlasId atlasId)
{
    auto found = d->atlases.find(atlasId);
    return found != d->atlases.end()? found->second : nullptr;
}

TextureBank::Allocation TextureBank::texture(const DotPath &id)
{
    auto &item = data(id).as<Impl::TextureData>();
    return Allocation{item.id(), item.atlasId};
}

Path TextureBank::sourcePathForAtlasId(const Id &id) const
{
    auto found = d->pathForAtlasId.find(id);
    if (found != d->pathForAtlasId.end())
    {
        return found->second.second;
    }
    return {};
}

Bank::IData *TextureBank::loadFromSource(ISource &source)
{
    auto &src = source.as<ImageSource>();
    auto *data = new Impl::TextureData(src.atlasId(), src.load(), d);
    if (const auto &texId = data->id())
    {
        d->pathForAtlasId.insert(texId, std::make_pair(src.atlasId(), src.sourcePath().toString()));
    }
    return data;
}

} // namespace de
