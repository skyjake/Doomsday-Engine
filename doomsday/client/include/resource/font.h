/** @file font.h
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

#ifndef LIBDENG_FONT_H
#define LIBDENG_FONT_H

#include "dd_types.h"

#ifdef __cplusplus
extern "C" {
#endif

// Font types.
typedef enum {
    FT_FIRST = 0,
    FT_BITMAP = FT_FIRST,
    FT_BITMAPCOMPOSITE,
    FT_LAST = FT_BITMAPCOMPOSITE
} fonttype_t;

#define VALID_FONTTYPE(v)       ((v) >= FT_FIRST && (v) <= FT_LAST)

/**
 * @defgroup fontFlags  Font Flags
 * @ingroup flags
 */
/*@{*/
#define FF_COLORIZE             0x1 /// Font can be colored.
#define FF_SHADOWED             0x2 /// Font has an embedded shadow.
/*@}*/

/**
 * Abstract Font base. To be used as the basis for all types of font.
 * @ingroup refresh
 */
#define MAX_CHARS               256 // Normal 256 ANSI characters.
typedef struct font_s {
    fonttype_t _type;

    /// @c true= Font requires a full update.
    boolean _isDirty;

    /// @ref fontFlags.
    int _flags;

    /// Unique identifier of the primary binding in the owning collection.
    fontid_t _primaryBind;

    /// Font metrics.
    int _leading;
    int _ascent;
    int _descent;

    Size2Raw _noCharSize;

    /// dj: Do fonts have margins? Is this a pixel border in the composited
    /// character map texture (perhaps per-glyph)?
    int _marginWidth, _marginHeight;
} font_t;

void Font_Init(font_t* font, fonttype_t type, fontid_t bindId);

fonttype_t Font_Type(const font_t* font);

fontid_t Font_PrimaryBind(const font_t* font);

void Font_SetPrimaryBind(font_t* font, fontid_t bindId);

/// @return  @ref fontFlags
int Font_Flags(const font_t* font);

int Font_Ascent(font_t* font);

int Font_Descent(font_t* font);

int Font_Leading(font_t* font);

boolean Font_IsPrepared(font_t* font);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* LIBDENG_FONT_H */
