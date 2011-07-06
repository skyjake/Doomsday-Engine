/**\file bitmapfont.h
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2011 Daniel Swanson <danij@dengine.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

#ifndef LIBDENG_BITMAPFONT_H
#define LIBDENG_BITMAPFONT_H

#include "font.h"

// Data for a character.
typedef struct {
    int x, y; /// Upper left corner.
    int w, h; /// Dimensions.
    DGLuint dlist;
} bitmapfont_char_t;

typedef struct bitmapfont_s {
    /// Base font.
    font_t _base;

    /// Absolute file path to the archived version of this font (if any).
    ddstring_t _filePath;

    /// GL-texture name.
    DGLuint _tex;

    /// Width and height of the texture in pixels.
    int _texWidth, _texHeight;

    /// Character map.
    bitmapfont_char_t _chars[MAX_CHARS];
} bitmapfont_t;

font_t* BitmapFont_Construct(void);
void BitmapFont_Destruct(font_t* font);

void BitmapFont_Prepare(font_t* font);

void BitmapFont_SetFilePath(font_t* font, const char* filePath);

/// @return  GL-texture name.
DGLuint BitmapFont_GLTextureName(const font_t* font);
int BitmapFont_TextureHeight(const font_t* font);
int BitmapFont_TextureWidth(const font_t* font);

void BitmapFont_DeleteGLTexture(font_t* font);
void BitmapFont_DeleteGLDisplayLists(font_t* font);

int BitmapFont_CharWidth(font_t* font, unsigned char ch);
int BitmapFont_CharHeight(font_t* font, unsigned char ch);

/**
 * Query the texture coordinates of a character in this font.
 */
void BitmapFont_CharCoords(font_t* font, int* s0, int* s1, int* t0, int* t1, unsigned char ch);

// Data for a character.
typedef struct {
    int x, y; /// Upper left corner.
    int w, h; /// Dimensions.
    DGLuint dlist;
    patchid_t patch;
    DGLuint tex;
} bitmapcompositefont_char_t;

typedef struct bitmapcompositefont_s {
    /// Base font.
    font_t _base;

    /// Definition used to construct this else @c NULL if not applicable.
    struct ded_compositefont_s* _def;

    /// Character map.
    bitmapcompositefont_char_t _chars[MAX_CHARS];
} bitmapcompositefont_t;

font_t* BitmapCompositeFont_Construct(void);
void BitmapCompositeFont_Destruct(font_t* font);

void BitmapCompositeFont_Prepare(font_t* font);

int BitmapCompositeFont_CharWidth(font_t* font, unsigned char ch);
int BitmapCompositeFont_CharHeight(font_t* font, unsigned char ch);

struct ded_compositefont_s* BitmapCompositeFont_Definition(const font_t* font);
void BitmapCompositeFont_SetDefinition(font_t* font, struct ded_compositefont_s* def);

patchid_t BitmapCompositeFont_CharPatch(font_t* font, unsigned char ch);
void BitmapCompositeFont_CharSetPatch(font_t* font, unsigned char ch, const char* patchName);

void BitmapCompositeFont_DeleteGLTextures(font_t* font);
void BitmapCompositeFont_DeleteGLDisplayLists(font_t* font);

/**
 * Query the texture coordinates of a character in this font.
 */
void BitmapCompositeFont_CharCoords(font_t* font, int* s0, int* s1, int* t0, int* t1, unsigned char ch);

#endif /* LIBDENG_BITMAPFONT_H */
