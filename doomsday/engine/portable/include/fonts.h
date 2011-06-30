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

#include "bitmapfont.h"

struct ded_compositefont_s;

/// Register the console commands, variables, etc..., of this module.
void Fonts_Register(void);

/**
 * Initialize the font renderer.
 * @return  @c 0 iff there are no errors.
 */
int Fonts_Init(void);
void Fonts_Shutdown(void);

/**
 * Mark all fonts as requiring a full update. Called during engine/renderer reset.
 */
void Fonts_Update(void);

/**
 * Load the specified font as a "system font".
 */
fontid_t Fonts_LoadSystemFont(const char* name, const char* path);

fontid_t Fonts_CreateFromDef(struct ded_compositefont_s* def);

int Fonts_ToIndex(fontid_t id);

/// @return  Ptr to the font associated with the specified id.
bitmapfont_t* Fonts_FontForId(fontid_t id);
bitmapfont_t* Fonts_FontForIndex(int index);

/**
 * Find the associated font for @a name.
 *
 * @param name  Name of the font to lookup.
 * @return  Unique id of the found font.
 */
fontid_t Fonts_IdForName(const char* name);

void Fonts_DestroyFont(fontid_t id);

ddstring_t** Fonts_CollectNames(int* count);

#endif /* LIBDENG_FONTS_H */
