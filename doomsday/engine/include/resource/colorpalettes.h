/**
 * @file colorpalettes.h ColorPalette repository and related bookkeeping.
 *
 * @author Copyright &copy; 2009-2012 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG_RESOURCE_COLORPALETTES_H
#define LIBDENG_RESOURCE_COLORPALETTES_H

#include "dd_share.h" // For colorpaletteid_t
#include "resource/colorpalette.h"

// Maximum number of palette translation tables.
#define NUM_TRANSLATION_CLASSES         3
#define NUM_TRANSLATION_MAPS_PER_CLASS  7
#define NUM_TRANSLATION_TABLES          (NUM_TRANSLATION_CLASSES * NUM_TRANSLATION_MAPS_PER_CLASS)

DENG_EXTERN_C colorpaletteid_t defaultColorPalette;
DENG_EXTERN_C byte* translationTables;

#ifdef __cplusplus
extern "C" {
#endif

void R_InitTranslationTables(void);
void R_UpdateTranslationTables(void);

/// Prepare for color palette creation.
void R_InitColorPalettes(void);

/// To be called when any existing color palettes are no longer required.
void R_DestroyColorPalettes(void);

/// @return  Number of available color palettes.
int R_ColorPaletteCount(void);

/// @return  ColorPalette associated with unique @a id, else @c NULL.
colorpalette_t *R_ToColorPalette(colorpaletteid_t id);

/**
 * Given a color palette list index return the ColorPalette.
 * @return  ColorPalette if found else @c NULL
 */
colorpalette_t *R_GetColorPaletteByIndex(int paletteIdx);

/**
 * Add a new (named) color palette.
 * @note Part of the Doomsday public API.
 *
 * The idea with the two-teered implementation is to allow maximum flexibility.
 * Within the engine we can create new palettes and manipulate them directly
 * via the DGL interface. The underlying implementation is wrapped in a similar
 * way to the materials so that publically, there is a set of (eternal) names
 * and unique identifiers that survive game and GL resets.
 *
 * @param fmt  Format string describes the format of @p data.
 *      Expected form: "C#C#C"
 *      C = color component, one of R, G, B.
 *      # = bits per component.
 *
 * @param name  Unique name by which the palette will be known.
 * @param colorData  Color component triplets (at least @a colorCount * 3 values).
 * @param colorCount  Number of colors.
 *
 * @return  Color palette id.
 */
colorpaletteid_t R_CreateColorPalette(char const *fmt, char const *name,
    uint8_t const *colorData, int colorCount);

/**
 * Given a color palette id, look up the specified unique name.
 * @note Part of the Doomsday public API.
 *
 * @param id  Id of the color palette to locate.
 * @return  Pointer to the unique name associated with the specified id else @c NULL
 */
char const *R_GetColorPaletteNameForNum(colorpaletteid_t id);

/**
 * Given a color palette name, look up the associated identifier.
 * @note Part of the Doomsday public API.
 *
 * @param name Unique name of the palette to locate.
 * @return  Identifier of the palette associated with this name, else @c 0
 */
colorpaletteid_t R_GetColorPaletteNumForName(char const *name);

/**
 * Given a color palette index, calculate the equivalent RGB color.
 * @note Part of the Doomsday public API.
 *
 * @param id  Id of the ColorPalette to use.
 * @param colorIdx  ColorPalette color index.
 * @param rgb  Final color will be written back here.
 * @param correctGamma  @c true= the texture gamma ramp should be applied.
 */
void R_GetColorPaletteRGBubv(colorpaletteid_t id, int colorIdx, uint8_t rgb[3], boolean correctGamma);

/**
 * Given a color palette index, calculate the equivalent RGB color.
 * @note Part of the Doomsday public API.
 *
 * @param id  Id of the ColorPalette to use.
 * @param colorIdx  ColorPalette color index.
 * @param rgb  Final color will be written back here.
 * @param correctGamma  @c true= the texture gamma ramp should be applied.
 */
void R_GetColorPaletteRGBf(colorpaletteid_t id, int colorIdx, float rgb[3], boolean correctGamma);

/**
 * Change the default color palette.
 *
 * @param id  Id of the color palette to make default.
 *
 * @return  @c true iff successful, else @c NULL.
 */
boolean R_SetDefaultColorPalette(colorpaletteid_t id);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* LIBDENG_RESOURCE_COLORPALETTES_H */
