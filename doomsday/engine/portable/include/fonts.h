/**\file fonts.h
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2011 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG_FONTS_H
#define LIBDENG_FONTS_H

#include "font.h"

struct ded_compositefont_s;

/// Register the console commands, variables, etc..., of this module.
void Fonts_Register(void);

/// Initialize this module.
void Fonts_Init(void);

/// Shutdown this module.
void Fonts_Shutdown(void);

/// Mark all fonts as requiring a full update. Called during engine/renderer reset.
void Fonts_Update(void);

/// @return  Number of known font bindings in all namespaces.
uint Fonts_Count(void);

/// @return  List of collected font names.
ddstring_t** Fonts_CollectNames(int* count);

/**
 * Font reference/handle translators:
 */

/// @return  Font associated with the specified unique name else @c NULL.
font_t* Fonts_ToFont(fontnum_t num);

/// @return  Unique name associated with the specified Font.
fontnum_t Fonts_ToIndex(font_t* font);

/// @return  Unique name associated with the found Font.
fontnum_t Fonts_IndexForUri(const dduri_t* uri);

/// @return  Unique name associated with the found Font.
fontnum_t Fonts_IndexForName(const char* path);

/// @return  Symbolic name associated with specified font.
const ddstring_t* Fonts_GetSymbolicName(font_t* font);

/// @return  New Uri composed for the specified font (release with Uri_Destruct).
dduri_t* Fonts_GetUri(font_t* font);

/// Load an external font from a local file.
font_t* Fonts_LoadExternal(const char* name, const char* filepath);

/// Create a bitmap composite font from @a def.
font_t* Fonts_CreateBitmapCompositeFromDef(struct ded_compositefont_s* def);

/**
 * Update the Font according to the supplied definition.
 * To be called after an engine update/reset.
 *
 * @parma font  Font to be updated.
 * @param def  font definition to update using.
 */
void Fonts_RebuildBitmapComposite(font_t* font, struct ded_compositefont_s* def);

int Fonts_Ascent(font_t* font);
int Fonts_Descent(font_t* font);
int Fonts_Leading(font_t* font);

/**
 * Query the visible dimensions of a character in this font.
 */
void Fonts_CharDimensions(font_t* font, int* width, int* height, unsigned char ch);
int Fonts_CharHeight(font_t* font, unsigned char ch);
int Fonts_CharWidth(font_t* font, unsigned char ch);

#endif /* LIBDENG_FONTS_H */
