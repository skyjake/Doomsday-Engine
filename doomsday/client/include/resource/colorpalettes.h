/** @file colorpalettes.h  Color palette resource collection.
 *
 * @authors Copyright © 2009-2013 Daniel Swanson <danij@dengine.net>
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
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#ifndef DENG_RESOURCE_COLORPALETTES_H
#define DENG_RESOURCE_COLORPALETTES_H

#include "dd_share.h" // For colorpaletteid_t
#include "resource/colorpalette.h"

// Maximum number of palette translation tables.
#define NUM_TRANSLATION_CLASSES         3
#define NUM_TRANSLATION_MAPS_PER_CLASS  7
#define NUM_TRANSLATION_TABLES          (NUM_TRANSLATION_CLASSES * NUM_TRANSLATION_MAPS_PER_CLASS)

DENG_EXTERN_C colorpaletteid_t defaultColorPalette;
DENG_EXTERN_C byte *translationTables;

void R_InitTranslationTables();
void R_UpdateTranslationTables();

byte const *R_TranslationTable(int tclass, int tmap);

/// Prepare for color palette creation.
void R_InitColorPalettes();

/// To be called when any existing color palettes are no longer required.
void R_DestroyColorPalettes();

/// @return  Number of available color palettes.
int R_ColorPaletteCount();

/// @return  ColorPalette associated with unique @a id, else @c NULL.
ColorPalette *R_ToColorPalette(colorpaletteid_t id);

DENG_EXTERN_C colorpaletteid_t R_GetColorPaletteNumForName(char const *name);
DENG_EXTERN_C char const *R_GetColorPaletteNameForNum(colorpaletteid_t id);

/**
 * Given a color palette list index return the ColorPalette.
 * @return  ColorPalette if found else @c NULL
 */
ColorPalette *R_GetColorPaletteByIndex(int paletteIdx);

/**
 * Change the default color palette.
 *
 * @param id  Id of the color palette to make default.
 *
 * @return  @c true iff successful, else @c NULL.
 */
boolean R_SetDefaultColorPalette(colorpaletteid_t id);

#endif // DENG_RESOURCE_COLORPALETTES_H
