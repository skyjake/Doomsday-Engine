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

// Data for a character.
typedef struct {
    RectRaw geometry;
    Point2Raw coords[4];
} bitmapfont_char_t;

typedef struct bitmapfont_s {
    /// Base font.
    font_t _base;

    /// Absolute file path to the archived version of this font (if any).
    ddstring_t _filePath;

    /// GL-texture name.
    DGLuint _tex;

    /// Size of the texture in pixels.
    Size2Raw _texSize;

    /// Character map.
    bitmapfont_char_t _chars[MAX_CHARS];
} bitmapfont_t;

font_t *BitmapFont_New(fontid_t bindId);
void BitmapFont_Delete(font_t *font);

void BitmapFont_Prepare(font_t *font);

void BitmapFont_SetFilePath(font_t *font, char const *filePath);

/// @return  GL-texture name.
DGLuint BitmapFont_GLTextureName(font_t const *font);
Size2Raw const *BitmapFont_TextureSize(font_t const *font);
int BitmapFont_TextureHeight(font_t const *font);
int BitmapFont_TextureWidth(font_t const *font);

void BitmapFont_DeleteGLTexture(font_t *font);

RectRaw const *BitmapFont_CharGeometry(font_t *font, unsigned char chr);
int BitmapFont_CharWidth(font_t *font, unsigned char ch);
int BitmapFont_CharHeight(font_t *font, unsigned char ch);

void BitmapFont_CharCoords(font_t *font, unsigned char ch, Point2Raw coords[4]);

// Data for a character.
typedef struct {
    RectRaw geometry;
    patchid_t patch;
    de::Texture::Variant *tex;
    uint8_t border;
} bitmapcompositefont_char_t;

typedef struct bitmapcompositefont_s {
    /// Base font.
    font_t _base;

    /// Definition used to construct this else @c NULL if not applicable.
    struct ded_compositefont_s *_def;

    /// Character map.
    bitmapcompositefont_char_t _chars[MAX_CHARS];
} bitmapcompositefont_t;

font_t *BitmapCompositeFont_New(fontid_t bindId);
void BitmapCompositeFont_Delete(font_t *font);

void BitmapCompositeFont_Prepare(font_t *font);

RectRaw const *BitmapCompositeFont_CharGeometry(font_t *font, unsigned char chr);
int BitmapCompositeFont_CharWidth(font_t *font, unsigned char ch);
int BitmapCompositeFont_CharHeight(font_t *font, unsigned char ch);

struct ded_compositefont_s *BitmapCompositeFont_Definition(font_t const *font);
void BitmapCompositeFont_SetDefinition(font_t *font, struct ded_compositefont_s *def);

patchid_t BitmapCompositeFont_CharPatch(font_t *font, unsigned char ch);
void BitmapCompositeFont_CharSetPatch(font_t *font, unsigned char ch, char const *patchName);

de::Texture::Variant *BitmapCompositeFont_CharTexture(font_t *font, unsigned char ch);
void BitmapCompositeFont_ReleaseTextures(font_t *font);

uint8_t BitmapCompositeFont_CharBorder(font_t *font, unsigned char chr);

void BitmapCompositeFont_CharCoords(font_t *font, unsigned char chr, Point2Raw coords[4]);

#endif /* LIBDENG_BITMAPFONT_H */
