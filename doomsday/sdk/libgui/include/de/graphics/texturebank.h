/** @file texturebank.h  Bank for images stored in a texture atlas.
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

#ifndef LIBGUI_TEXTUREBANK_H
#define LIBGUI_TEXTUREBANK_H

#include <de/Bank>
#include "../AtlasTexture"

namespace de {

/**
 * Bank that stores images on a texture atlas for use in GL drawing.
 *
 * The data item sources in the bank must be derived from TextureBank::ImageSource.
 *
 * @ingroup gl
 */
class LIBGUI_PUBLIC TextureBank : public Bank
{
public:
    /**
     * Base classs for entries in the bank. When requested, provides the Image data
     * of the specified item.
     */
    class LIBGUI_PUBLIC ImageSource : public ISource
    {
    public:
        ImageSource(DotPath const &id = "");
        DotPath const &id() const;

        virtual Image load() const = 0;

    private:
        DENG2_PRIVATE(d)
    };

public:
    TextureBank();

    /**
     * Sets the atlas where the images are to be allocated from.
     *
     * @param atlas  Texture atlas.
     */
    void setAtlas(AtlasTexture &atlas);

    Id const &texture(DotPath const &id);

protected:
    IData *loadFromSource(ISource &source);

private:
    DENG2_PRIVATE(d)
};

} // namespace de

#endif // LIBGUI_TEXTUREBANK_H
