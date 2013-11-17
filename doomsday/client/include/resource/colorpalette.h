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

#include <de/libdeng2.h>
#include <de/Error>
#include <de/Id>
#include <de/Observers>
#include <de/Vector>
#ifdef __CLIENT__
#include <QColor>
#endif
#include <QVector>

/**
 * Converts a sequence of bytes, given a color format descriptor, into a table
 * of colors (usable with ColorPalette).
 */
class ColorTableReader
{
public:
    /// Base class for color-format-related errors. @ingroup errors
    DENG2_ERROR(FormatError);

public:
    /**
     * @param format  Textual color format description of the format of each
     * discreet color value in @a colorData.
     *
     * Expected form: "C#C#C"
     * - 'C'= color component identifier, one of [R, G, B]
     * - '#'= number of bits for the identified component.
     *
     * @param colorCount  Number of discreet colors in @a colorData.
     * @param colorData   Color data (at least @a colorCount * 3 values).
     */
    static QVector<de::Vector3ub> read(de::String format, int colorCount,
                                       de::dbyte const *colorData);
};

/**
 * Color Palette.
 *
 * @ingroup resource
 */
class ColorPalette
{
public:
    /// Notified whenever the color table changes.
    DENG2_DEFINE_AUDIENCE(ColorTableChange, void colorPaletteColorTableChanged(ColorPalette &colorPalette))

public:
    /**
     * Construct a new empty color palette.
     */
    ColorPalette();

    /**
     * Constructs a new color palette using the specified color table.
     *
     * @param colors  Color table to initialize from. A copy is made.
     */
    ColorPalette(QVector<de::Vector3ub> const &colors);

    /// @see color()
    inline de::Vector3ub operator [] (int colorIndex) const {
        return color(colorIndex);
    }

    /**
     * Returns the automatically generated, unique identifier of the color palette.
     */
    de::Id id() const;

    /**
     * Returns the total number of colors in the palette.
     */
    int colorCount() const;

    /**
     * Replace the entire color table. The ColorTableChange audience is notified
     * whenever the color table changes.
     *
     * @param colorTable  The replacement color table. A copy is made.
     */
    ColorPalette &loadColorTable(QVector<de::Vector3ub> const &colorTable);

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

#ifdef __CLIENT__
    /**
     * Same as @ref color() except the color is returned as a QColor instance.
     */
    inline QColor colorq(int colorIndex, int alpha = 255) const {
        de::Vector3ub rgb = color(colorIndex);
        return QColor(rgb.x, rgb.y, rgb.z, alpha);
    }
#endif

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
