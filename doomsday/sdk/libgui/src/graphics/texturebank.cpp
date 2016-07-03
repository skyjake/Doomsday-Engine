/** @file texturebank.cpp
 *
 * @authors Copyright (c) 2014 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/TextureBank"

#include <QHash>

namespace de {

DENG2_PIMPL_NOREF(TextureBank::ImageSource)
{
    DotPath sourcePath;
};

TextureBank::ImageSource::ImageSource(DotPath const &sourcePath) : d(new Impl)
{
    d->sourcePath = sourcePath;
}

DotPath const &TextureBank::ImageSource::sourcePath() const
{
    return d->sourcePath;
}

DENG2_PIMPL(TextureBank)
{
    struct TextureData : public IData
    {
        Impl *d;
        Id id { Id::None };

        TextureData(Image const &image, Impl *owner) : d(owner)
        {
            id = d->atlas->alloc(image);

            /// @todo Reduce size if doesn't fit? Can be expanded when requested for use.
        }

        ~TextureData()
        {
            d->pathForAtlasId.remove(id);
            d->atlas->release(id);
        }
    };

    IAtlas *atlas { nullptr };
    QHash<Id::Type, String> pathForAtlasId; // reverse lookup

    Impl(Public *i) : Base(i) {}

    ~Impl()
    {
        // Get rid of items before the reverse lookup hash is destroyed.
        self.clear();
    }
};

TextureBank::TextureBank() : Bank("TextureBank"), d(new Impl(this))
{}

void TextureBank::setAtlas(IAtlas *atlas)
{
    d->atlas = atlas;
}

IAtlas *TextureBank::atlas()
{
    return d->atlas;
}

Id const &TextureBank::texture(DotPath const &id)
{
    return data(id).as<Impl::TextureData>().id;
}

Path TextureBank::sourcePathForAtlasId(Id const &id) const
{
    auto found = d->pathForAtlasId.constFind(id);
    if (found != d->pathForAtlasId.constEnd())
    {
        return found.value();
    }
    return "";
}

Bank::IData *TextureBank::loadFromSource(ISource &source)
{
    auto *data = new Impl::TextureData(source.as<ImageSource>().load(), d);
    d->pathForAtlasId.insert(data->id, source.as<ImageSource>().sourcePath());
    return data;
}

} // namespace de
