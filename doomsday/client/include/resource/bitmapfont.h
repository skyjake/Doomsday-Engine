/** @file bitmapfont.h
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

#ifndef LIBDENG_BITMAPFONT_H
#define LIBDENG_BITMAPFONT_H

#include "font.h"
#include "Texture"
#include <de/point.h>
#include <de/rect.h>
#include <de/size.h>
#include <de/str.h>

class bitmapfont_t : public font_t
{
public:
    // Data for a character.
    struct bitmapfont_char_t
    {
        RectRaw geometry;
        Point2Raw coords[4];
    };

    /// Absolute file path to the archived version of this font (if any).
    ddstring_t _filePath;

    /// GL-texture name.
    DGLuint _tex;

    /// Size of the texture in pixels.
    Size2Raw _texSize;

    /// Character map.
    bitmapfont_char_t _chars[MAX_CHARS];

public:
    bitmapfont_t(fontid_t bindId);
    ~bitmapfont_t();

    static bitmapfont_t *fromFile(fontid_t bindId, char const *resourcePath);

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
};

class bitmapcompositefont_t : public font_t
{
public:
    // Data for a character.
    struct bitmapcompositefont_char_t
    {
        RectRaw geometry;
        patchid_t patch;
        de::Texture::Variant *tex;
        uint8_t border;
    };

    /// Definition used to construct this else @c NULL if not applicable.
    struct ded_compositefont_s *_def;

    /// Character map.
    bitmapcompositefont_char_t _chars[MAX_CHARS];

public:
    bitmapcompositefont_t(fontid_t bindId);
    ~bitmapcompositefont_t();

    static bitmapcompositefont_t *fromDef(fontid_t bindId, ded_compositefont_t *def);

    struct ded_compositefont_s *definition() const;
    void setDefinition(struct ded_compositefont_s *def);

    /**
     * Update the font according to the supplied definition. To be called after
     * an engine update/reset.
     *
     * @param def  Definition to update using.
     *
     * @todo Should observe engine reset.
     */
    void rebuildFromDef(ded_compositefont_t *def);

    patchid_t charPatch(unsigned char ch);
    void charSetPatch(unsigned char ch, char const *encodedPatchName);

    de::Texture::Variant *charTexture(unsigned char ch);

    uint8_t charBorder(unsigned char chr);

    void charCoords(unsigned char chr, Point2Raw coords[4]);

    void glInit();
    void glDeinit();

    RectRaw const *charGeometry(unsigned char ch);
    int charWidth(unsigned char ch);
    int charHeight(unsigned char ch);
};

#endif /* LIBDENG_BITMAPFONT_H */
