/**\file abstractresource.h
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2009-2012 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG_REFRESH_COLORPALETTE_H
#define LIBDENG_REFRESH_COLORPALETTE_H

/**
 * @defgroup colorPaletteFlags  Color Palette Flags
 * @{
 */
#define CPF_UPDATE_18TO8        0x1 /// The 18To8 LUT needs updating.
/**@}*/

#define COLORPALETTE_MAX_COMPONENT_BITS (16) // Max bits per component.

typedef struct colorpalette_s {
    /// @see colorPaletteFlags
    uint8_t _flags;

    /// R8G8B88 color triplets [_num * 3].
    uint8_t* _colorData;
    int _colorCount;

    /// Nearest color lookup table.
    int* _18To8LUT;
} colorpalette_t;

colorpalette_t* ColorPalette_New(void);

/**
 * @param compOrder  Component order. Examples:
 *      [0,1,2] == RGB
 *      [2,1,0] == BGR
 * @param compSize  Number of bits per component [R,G,B].
 * @param colorData  Color triplets (at least @a numColors * 3).
 * @param colorCount  Number of color triplets.
 */
colorpalette_t* ColorPalette_NewWithColorTable(const int compOrder[3],
    const uint8_t compBits[3], const uint8_t* colorData, int colorCount);

void ColorPalette_Delete(colorpalette_t* pal);

/// @return  Number of colors in the palette.
ushort ColorPalette_Size(colorpalette_t* pal);

/**
 * Replace the entire color table.
 *
 * @param compOrder  Component order. Examples:
 *      [0,1,2] == RGB
 *      [2,1,0] == BGR
 * @param compSize  Number of bits per component [R,G,B].
 * @param colorData  Color triplets (at least @a numColors * 3).
 * @param colorCount  Number of color triplets.
 */
void ColorPalette_ReplaceColorTable(colorpalette_t* pal, const int compOrder[3],
    const uint8_t compBits[3], const uint8_t* colorData, int colorCount);

/**
 * Lookup a color in the palette.
 *
 * \note If the specified color index is out of range it will be clamped to
 * a valid value before use.
 *
 * @param colorIdx  Index of the color to lookup.
 * @param rgb  Associated R8G8B8 color triplet is written here.
 */
void ColorPalette_Color(const colorpalette_t* pal, int colorIdx, uint8_t rgb[3]);

/**
 * Given an R8G8B8 color triplet return the closet matching color index.
 *
 * @param rgb  R8G8B8 color to be matched.
 * @return  Closet matching color index or @c -1 if no colors in the palette.
 */
int ColorPalette_NearestIndex(colorpalette_t* pal, uint8_t red, uint8_t green, uint8_t blue);
int ColorPalette_NearestIndexv(colorpalette_t* pal, const uint8_t rgb[3]);

#endif /* LIBDENG_REFRESH_COLORPALETTE_H */
