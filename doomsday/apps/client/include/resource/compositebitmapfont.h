/** @file compositebitmapfont.h  Composite bitmap font.
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

#ifndef CLIENT_RESOURCE_COMPOSITEBITMAPFONT_H
#define CLIENT_RESOURCE_COMPOSITEBITMAPFONT_H

#include "abstractfont.h"
#include "resource/clienttexture.h"
#include <de/rectangle.h>
#include <de/string.h>
#include <de/vector.h>

struct ded_compositefont_s;

/**
 * Composite bitmap font.
 *
 * @ingroup resource
 */
class CompositeBitmapFont : public AbstractFont
{
public:
    struct Glyph
    {
        de::Rectanglei geometry;
        patchid_t patch;
        TextureVariant *tex;
        uint8_t border;
        bool haveSourceImage;
    };

public:
    CompositeBitmapFont(de::FontManifest &manifest);

    static CompositeBitmapFont *fromDef(de::FontManifest &manifest, const struct ded_compositefont_s &def);

    struct ded_compositefont_s *definition() const;
    void setDefinition(struct ded_compositefont_s *newDef);

    /**
     * Update the font according to the supplied definition. To be called after
     * an engine update/reset.
     *
     * @param def  Definition to update using.
     *
     * @todo Should observe engine reset.
     */
    void rebuildFromDef(const struct ded_compositefont_s &def);

    int ascent() const override;
    int descent() const override;
    int lineSpacing() const override;

    void glInit() const override;
    void glDeinit() const override;

    const de::Rectanglei &glyphPosCoords(de::dbyte ch) const override;
    const de::Rectanglei &glyphTexCoords(de::dbyte ch) const override;

    patchid_t       glyphPatch(de::dbyte ch) const;
    void            glyphSetPatch(de::dbyte ch, de::String encodedPatchName);
    TextureVariant *glyphTexture(de::dbyte ch) const;
    uint            glyphTextureBorder(de::dbyte ch) const;

private:
    DE_PRIVATE(d)
};

#endif // CLIENT_RESOURCE_COMPOSITEBITMAPFONT_H
