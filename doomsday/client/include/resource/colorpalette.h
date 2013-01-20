/**
 * @file colorpalette.h Color Palette.
 *
 * @author Copyright &copy; 2009-2013 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG_RESOURCE_COLORPALETTE_H
#define LIBDENG_RESOURCE_COLORPALETTE_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup colorPaletteFlags  Color Palette Flags
 * @ingroup flags
 */
///@{
#define CPF_UPDATE_18TO8        0x1 /// The 18To8 LUT needs updating.
///@}

#define COLORPALETTE_MAX_COMPONENT_BITS (16) // Max bits per component.

/**
 * Color Palette.
 *
 * @ingroup resource
 */
typedef struct colorpalette_s {
    /// @see colorPaletteFlags
    uint8_t _flags;

    /// R8G8B88 color triplets [_num * 3].
    uint8_t *_colorData;
    int _colorCount;

    /// Nearest color lookup table.
    int *_18To8LUT;
} colorpalette_t;

colorpalette_t* ColorPalette_New(void);

/**
 * Constructs a new color palette.
 *
 * @param compOrder  Component order. Examples:
 * <pre> [0,1,2] == RGB
 * [2,1,0] == BGR</pre>
 * @param compBits  Number of bits per component [R,G,B].
 * @param colorData  Color triplets (at least @a numColors * 3).
 * @param colorCount  Number of color triplets.
 */
colorpalette_t *ColorPalette_NewWithColorTable(int const compOrder[3],
    uint8_t const compBits[3], uint8_t const *colorData, int colorCount);

void ColorPalette_Delete(colorpalette_t *pal);

/// @return  Number of colors in the palette.
ushort ColorPalette_Size(colorpalette_t *pal);

/**
 * Replace the entire color table.
 *
 * @param pal  Color palette.
 * @param compOrder  Component order. Examples:
 * <pre> [0,1,2] == RGB
 * [2,1,0] == BGR</pre>
 * @param compBits  Number of bits per component [R,G,B].
 * @param colorData  Color triplets (at least @a numColors * 3).
 * @param colorCount  Number of color triplets.
 */
void ColorPalette_ReplaceColorTable(colorpalette_t *pal, int const compOrder[3],
    uint8_t const compBits[3], uint8_t const *colorData, int colorCount);

/**
 * Lookup a color in the palette.
 *
 * @note If the specified color index is out of range it will be clamped to
 * a valid value before use.
 *
 * @param pal  Color palette.
 * @param colorIdx  Index of the color to lookup.
 * @param rgb  Associated R8G8B8 color triplet is written here.
 */
void ColorPalette_Color(colorpalette_t const *pal, int colorIdx, uint8_t rgb[3]);

/**
 * Given an R8G8B8 color triplet return the closet matching color index.
 *
 * @param pal  Color palette.
 * @param rgb  R8G8B8 color to be matched.
 *
 * @return  Closet matching color index or @c -1 if no colors in the palette.
 */
int ColorPalette_NearestIndexv(colorpalette_t *pal, uint8_t const rgb[3]);
int ColorPalette_NearestIndex(colorpalette_t *pal, uint8_t red, uint8_t green, uint8_t blue);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* LIBDENG_RESOURCE_COLORPALETTE_H */
