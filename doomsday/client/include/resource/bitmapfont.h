/** @file bitmapfont.h  Bitmap font.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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
#include "Texture"
#include <de/point.h>
#include <de/rect.h>
#include <de/size.h>
#include <de/str.h>

/**
 * Bitmap font.
 *
 * @ingroup resource
 */
class BitmapFont : public AbstractFont
{
public:
    // Data for a character.
    struct bitmapfont_char_t
    {
        RectRaw geometry;
        Point2Raw coords[4];
    };

public:
    BitmapFont(fontid_t bindId);

    static BitmapFont *fromFile(fontid_t bindId, char const *resourcePath);

    void rebuildFromFile(char const *resourcePath);
    void setFilePath(char const *filePath);

    /// @return  GL-texture name.
    DGLuint textureGLName() const;
    Size2Raw const *textureSize() const;
    int textureHeight() const;
    int textureWidth() const;

    void charCoords(unsigned char ch, Point2Raw coords[4]);

    void glInit();
    void glDeinit();

    RectRaw const *charGeometry(unsigned char ch);
    int charWidth(unsigned char ch);
    int charHeight(unsigned char ch);

private:
    DENG2_PRIVATE(d)
};

#endif // CLIENT_RESOURCE_BITMAPFONT_H
