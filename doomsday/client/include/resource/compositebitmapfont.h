/** @file compositebitmapfont.h  Composite bitmap font.
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

#ifndef CLIENT_RESOURCE_COMPOSITEBITMAPFONT_H
#define CLIENT_RESOURCE_COMPOSITEBITMAPFONT_H

#include "abstractfont.h"
#include "def_main.h"
#include "Texture"
#include <de/Rectangle>
#include <de/Vector>

/**
 * Composite bitmap font.
 *
 * @ingroup resource
 */
class CompositeBitmapFont : public AbstractFont
{
public:
    // Data for a character.
    struct bitmapcompositefont_char_t
    {
        de::Rectanglei geometry;
        patchid_t patch;
        de::Texture::Variant *tex;
        uint8_t border;
    };

public:
    CompositeBitmapFont(fontid_t bindId);

    static CompositeBitmapFont *fromDef(fontid_t bindId, ded_compositefont_t *def);

    ded_compositefont_t *definition() const;
    void setDefinition(ded_compositefont_t *newDef);

    /**
     * Update the font according to the supplied definition. To be called after
     * an engine update/reset.
     *
     * @param def  Definition to update using.
     *
     * @todo Should observe engine reset.
     */
    void rebuildFromDef(ded_compositefont_t *def);

    patchid_t charPatch(uchar ch);
    void charSetPatch(uchar ch, char const *encodedPatchName);

    de::Texture::Variant *charTexture(uchar ch);

    uint8_t charBorder(uchar chr);

    void charCoords(uchar chr, de::Vector2i coords[4]);

    void glInit();
    void glDeinit();

    de::Rectanglei const &charGeometry(uchar ch);
    int charWidth(uchar ch);
    int charHeight(uchar ch);

private:
    DENG2_PRIVATE(d)
};

#endif // CLIENT_RESOURCE_COMPOSITEBITMAPFONT_H
