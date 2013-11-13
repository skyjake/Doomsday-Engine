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
#include "gl/gl_main.h"
#include <de/Rectangle>
#include <de/String>
#include <de/Vector>

/**
 * Bitmap font.
 *
 * @ingroup resource
 */
class BitmapFont : public AbstractFont
{
public:
    BitmapFont(fontid_t bindId);

    static BitmapFont *fromFile(fontid_t bindId, de::String resourcePath);

    void rebuildFromFile(de::String resourcePath);
    void setFilePath(de::String resourcePath);

    /// @return  GL-texture name.
    GLuint textureGLName() const;
    de::Vector2i const &textureDimensions() const;

    void glInit();
    void glDeinit();

    de::Rectanglei const &glyphPosCoords(uchar ch);
    de::Rectanglei const &glyphTexCoords(uchar ch);

private:
    DENG2_PRIVATE(d)
};

#endif // CLIENT_RESOURCE_BITMAPFONT_H
