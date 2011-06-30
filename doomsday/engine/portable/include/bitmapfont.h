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

#include "dd_types.h"

// Data for a character.
typedef struct {
    int x, y; /// Upper left corner.
    int w, h; /// Dimensions.

    patchid_t patch;
    DGLuint tex;
    DGLuint dlist;
} bitmapfont_char_t;

/**
 * @defgroup bitmapFontFlags  Bitmap Font Flags.
 */
/*@{*/
#define BFF_IS_MONOCHROME       0x1 /// Font texture is monochrome and can be colored.
#define BFF_HAS_EMBEDDEDSHADOW  0x2 /// Font texture has an embedded shadow.
#define BFF_IS_DIRTY            0x4 /// Font requires a full update.
/*@}*/

/**
 * Bitmap Font.
 * @ingroup refresh
 */
#define MAX_CHARS               256 // Normal 256 ANSI characters.
typedef struct bitmapfont_s {
    /// Identifier associated with this font.
    fontid_t _id;

    /// @see bitmapFontFlags.
    int _flags;

    /// Symbolic name associated with this font.
    ddstring_t _name;

    /// Absolute file path to the archived version of this font (if any).
    ddstring_t _filePath;

    /// Font metrics.
    int _leading;
    int _ascent;
    int _descent;

    /// dj: Do fonts have margins? Is this a pixel border in the composited
    /// character map texture (perhaps per-glyph)?
    int _marginWidth, _marginHeight;

    /// Character map.
    bitmapfont_char_t _chars[MAX_CHARS];

    /// GL-texture name.
    DGLuint _tex;

    /// Width and height of the texture in pixels.
    int _texWidth, _texHeight;
} bitmapfont_t;

bitmapfont_t* BitmapFont_Construct(fontid_t id, const char* name,
    const char* filePath);
void BitmapFont_Destruct(bitmapfont_t* font);

void BitmapFont_DeleteGLDisplayLists(bitmapfont_t* font);
void BitmapFont_DeleteGLTextures(bitmapfont_t* font);

/**
 * Query the visible dimensions of a character in this font.
 */
void BitmapFont_CharDimensions(bitmapfont_t* font, int* width,
    int* height, unsigned char ch);
int BitmapFont_CharHeight(bitmapfont_t* font, unsigned char ch);
int BitmapFont_CharWidth(bitmapfont_t* font, unsigned char ch);

/**
 * Query the texture coordinates of a character in this font.
 */
void BitmapFont_CharCoords(bitmapfont_t* font, int* s0, int* s1,
    int* t0, int* t1, unsigned char ch);

patchid_t BitmapFont_CharPatch(bitmapfont_t* font, unsigned char ch);
void BitmapFont_CharSetPatch(bitmapfont_t* font, unsigned char ch, const char* patchName);

/// @return  GL-texture name.
DGLuint BitmapFont_GLTextureName(const bitmapfont_t* font);

int BitmapFont_TextureHeight(const bitmapfont_t* font);
int BitmapFont_TextureWidth(const bitmapfont_t* font);

/**
 * Accessor methods.
 */

/// @return  @see bitmapFontFlags
int BitmapFont_Flags(const bitmapfont_t* font);

/// @return  Identifier associated with this font.
fontid_t BitmapFont_Id(const bitmapfont_t* font);

/// @return  Symbolic name associated with this font.
const ddstring_t* BitmapFont_Name(const bitmapfont_t* font);

int BitmapFont_Ascent(bitmapfont_t* font);
int BitmapFont_Descent(bitmapfont_t* font);
int BitmapFont_Leading(bitmapfont_t* font);

#endif /* LIBDENG_BITMAPFONT_H */
