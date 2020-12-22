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

#ifndef LIBDOOMSDAY_RESOURCE_COLORPALETTE_H
#define LIBDOOMSDAY_RESOURCE_COLORPALETTE_H

#include "../libdoomsday.h"
#include <de/error.h>
#include <de/id.h>
#include <de/observers.h>
#include <de/string.h>
#include <de/vector.h>
#include <de/list.h>

namespace res {

/**
 * Converts a sequence of bytes, given a color format descriptor, into a table
 * of colors (usable with ColorPalette).
 */
class LIBDOOMSDAY_PUBLIC ColorTableReader
{
public:
    /// Base class for color-format-related errors. @ingroup errors
    DE_ERROR(FormatError);

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
    static de::List<de::Vec3ub> read(de::String format, int colorCount, const de::dbyte *colorData);
};

/**
 * Color Palette.
 *
 * @ingroup resource
 */
class LIBDOOMSDAY_PUBLIC ColorPalette
{
public:
    /// An invalid translation id was specified. @ingroup errors
    DE_ERROR(InvalidTranslationIdError);

    /// Notified whenever the color table changes.
    DE_DEFINE_AUDIENCE(ColorTableChange, void colorPaletteColorTableChanged(ColorPalette &colorPalette))

    /// Palette index translation mapping table.
    typedef de::List<int> Translation;

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
    ColorPalette(de::List<de::Vec3ub> const &colors);

    /// @see color()
    inline de::Vec3ub operator [] (int colorIndex) const {
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
     * Lookup a color in the palette by @a colorIndex. If the specified index is
     * out of valid [0..colorCount) range it will be clamped.
     *
     * @param colorIndex  Index of the color in the palette.
     *
     * @return  Associated R8G8B8 color triplet.
     *
     * @see colorf(), operator []
     */
    de::Vec3ub color(int colorIndex) const;

    /**
     * Same as @ref color() except the color is returned in [0..1] floating-point.
     */
    de::Vec3f colorf(int colorIndex) const;

    /**
     * Replace the entire color table. The ColorTableChange audience is notified
     * whenever the color table changes.
     *
     * If the new color table has a different number of colors, then any existing
     * translation maps will be cleared automatically.
     *
     * @param colorTable  The replacement color table. A copy is made.
     */
    ColorPalette &replaceColorTable(de::List<de::Vec3ub> const &colorTable);

    /**
     * Given an R8G8B8 color triplet return the closet matching color index.
     *
     * @param rgb  R8G8B8 color to be matched.
     *
     * @return  Closet matching color index or @c -1 if no colors in the palette.
     */
    int nearestIndex(const de::Vec3ub &rgb) const;

    /**
     * Clear all translation maps.
     */
    void clearTranslations();

    /**
     * Lookup a translation map by it's unique @a id.
     *
     * @return  Pointer to the identified translation; otherwise @c 0.
     */
    const Translation *translation(de::String id) const;

    /**
     * Add/replace the identified translation map.
     *
     * @param id        Unique identifier of the translation.
     * @param mappings  Table of palette index mappings (a copy is made). It is
     *                  assumed that this table contains a mapping for each color
     *                  in the palette.
     *
     * @see colorCount()
     */
    void newTranslation(de::String id, const Translation &mappings);

private:
    DE_PRIVATE(d)
};

typedef ColorPalette::Translation ColorPaletteTranslation;

} // namespace res

#endif // LIBDOOMSDAY_RESOURCE_COLORPALETTE_H
