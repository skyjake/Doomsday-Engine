/** @file colorpalette.h  Color palette resource.
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

#ifndef DENG_RESOURCE_COLORPALETTE_H
#define DENG_RESOURCE_COLORPALETTE_H

#include "dd_types.h"
#include <de/Error>
#include <de/Vector>

/**
 * Color Palette.
 *
 * @ingroup resource
 */
class ColorPalette
{
public:
    /// Base class for color-format-related errors. @ingroup errors
    DENG2_ERROR(ColorFormatError);

    /// Maximum number of bits per color component.
    static int const max_component_bits = 16;

public:
    /**
     * Construct a new empty color palette.
     */
    ColorPalette();

    /**
     * Constructs a new color palette using the specified color table.
     *
     * @param compOrder   Component order. Examples:
     * <pre> [0,1,2] == RGB
     * [2,1,0] == BGR
     * </pre>
     *
     * @param compBits    Number of bits per component [R,G,B].
     * @param colorData   Color triplets (at least @a numColors * 3).
     * @param colorCount  Number of color triplets.
     */
    ColorPalette(int const compOrder[3], uint8_t const compBits[3],
        uint8_t const *colorData, int colorCount);

    static void parseColorFormat(char const *fmt, int compOrder[3], uint8_t compSize[3]);

    /// @see color()
    inline de::Vector3ub operator [] (int colorIndex) const {
        return color(colorIndex);
    }

    /**
     * Returns the total number of colors in the palette.
     */
    int size() const;

    /**
     * Replace the entire color table.
     *
     * @param compOrder   Component order. Examples:
     * <pre> [0,1,2] == RGB
     * [2,1,0] == BGR
     * </pre>
     *
     * @param compBits    Number of bits per component [R,G,B].
     * @param colorData   Color triplets (at least @a numColors * 3).
     * @param colorCount  Number of color triplets.
     */
    void replaceColorTable(int const compOrder[3], uint8_t const compBits[3],
        uint8_t const *colorData, int colorCount);

    /**
     * Lookup a color in the palette by @a colorIndex. If the specified index is
     * out of valid [0..colorCount) range it will be clamped.
     *
     * @param colorIndex  Index of the color in the palette.
     *
     * @return  Associated R8G8B8 color triplet.
     *
     * @see colorf(), operator []
     */
    de::Vector3ub color(int colorIndex) const;

    /**
     * Same as @ref color() except the color is returned in [0..1] floating-point.
     */
    de::Vector3f colorf(int colorIndex) const;

    /**
     * Given an R8G8B8 color triplet return the closet matching color index.
     *
     * @param rgb  R8G8B8 color to be matched.
     *
     * @return  Closet matching color index or @c -1 if no colors in the palette.
     */
    int nearestIndex(de::Vector3ub const &rgb) const;

private:
    DENG2_PRIVATE(d)
};

#endif // DENG_RESOURCE_COLORPALETTE_H
