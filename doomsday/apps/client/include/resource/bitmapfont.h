/** @file bitmapfont.h  Bitmap font.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
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

#ifndef CLIENT_RESOURCE_BITMAPFONT_H
#define CLIENT_RESOURCE_BITMAPFONT_H

#include "abstractfont.h"
#include <de/rectangle.h>
#include <de/string.h>
#include <de/vector.h>

/**
 * Bitmap font.
 *
 * @ingroup resource
 */
class BitmapFont : public AbstractFont
{
public:
    BitmapFont(de::FontManifest &manifest);

    static BitmapFont *fromFile(de::FontManifest &manifest, de::String resourcePath);

    void setFilePath(de::String resourcePath);

    /// @return  GL-texture name.
    uint textureGLName() const;
    const de::Vec2i &textureDimensions() const;
    const de::Vec2ui &textureMargin() const;

    int ascent() const override;
    int descent() const override;
    int lineSpacing() const override;

    const de::Rectanglei &glyphPosCoords(de::dbyte ch) const override;
    const de::Rectanglei &glyphTexCoords(de::dbyte ch) const override;

    void glInit() const override;
    void glDeinit() const override;

private:
    DE_PRIVATE(d)
};

#endif // CLIENT_RESOURCE_BITMAPFONT_H
