/** @file texturebank.h  Bank for images stored in a texture atlas.
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

#ifndef LIBGUI_TEXTUREBANK_H
#define LIBGUI_TEXTUREBANK_H

#include <de/bank.h>
#include "de/atlas.h"

namespace de {

/**
 * Bank that stores images on one or more atlases.
 *
 * The data item sources in the bank must be derived from TextureBank::ImageSource.
 *
 * @ingroup gl
 */
class LIBGUI_PUBLIC TextureBank : public Bank
{
public:
    using AtlasId = int;

    /**
     * Base classs for entries in the bank. When requested, provides the Image data
     * of the specified item.
     */
    class LIBGUI_PUBLIC ImageSource : public ISource
    {
    public:
        ImageSource(const DotPath &sourcePath = "");
        ImageSource(AtlasId atlasId, const DotPath &sourcePath);

        AtlasId atlasId() const;
        const DotPath &sourcePath() const;

        virtual Image load() const = 0;

    private:
        DE_PRIVATE(d)
    };

    struct LIBGUI_PUBLIC Allocation
    {
        Id id;
        AtlasId atlas;

        operator const Id &() const { return id; }
    };

public:
    TextureBank(const char *nameForLog = "TextureBank",
                const Flags &flags = DefaultFlags);

    /**
     * Sets the atlas where the images are to be allocated from.
     *
     * @param atlas  Texture atlas. Ownership not taken.
     */
    void setAtlas(IAtlas *atlas);

    /**
     * Sets one of the atlases that will be used for allocating images.
     *
     * @param atlasId  Id of the atlas.
     *
     * @param atlas Texture atlas.
     */
    void setAtlas(AtlasId atlasId, IAtlas *atlas);

    IAtlas *atlas(AtlasId atlasId = 0);

    Allocation texture(const DotPath &id);

    /**
     * Returns the source path of an image that has been loaded into the atlas.
     *
     * @param id  Atlas allocation id.
     *
     * @return  ImageSource path.
     */
    Path sourcePathForAtlasId(const Id &id) const;

protected:
    IData *loadFromSource(ISource &source);

private:
    DE_PRIVATE(d)
};

} // namespace de

#endif // LIBGUI_TEXTUREBANK_H
