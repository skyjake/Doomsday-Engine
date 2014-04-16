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

namespace de {

DENG2_PIMPL_NOREF(TextureBank::ImageSource)
{
    DotPath id;
};

TextureBank::ImageSource::ImageSource(DotPath const &id) : d(new Instance)
{
    d->id = id;
}

DotPath const &TextureBank::ImageSource::id() const
{
    return d->id;
}

DENG2_PIMPL_NOREF(TextureBank)
{
    struct TextureData : public IData
    {
        AtlasTexture *atlas;
        Id id;

        TextureData(Image const &image, AtlasTexture *atlasTex) : atlas(atlasTex)
        {
            id = atlas->alloc(image);

            /// @todo Reduce size if doesn't fit? Can be expanded when requested for use.
        }

        ~TextureData()
        {
            atlas->release(id);
        }
    };

    AtlasTexture *atlas;

    Instance() : atlas(0) {}
};

TextureBank::TextureBank() : d(new Instance)
{}

void TextureBank::setAtlas(AtlasTexture &atlas)
{
    d->atlas = &atlas;
}

Id const &TextureBank::texture(DotPath const &id)
{
    return data(id).as<Instance::TextureData>().id;
}

Bank::IData *TextureBank::loadFromSource(ISource &source)
{
    DENG2_ASSERT(d->atlas != 0);
    return new Instance::TextureData(source.as<ImageSource>().load(), d->atlas);
}

} // namespace de
