/** @file colorpalettes.h
 *
 * @authors Copyright © 2013-2015 Daniel Swanson <danij@dengine.net>
 * @authors Copyright (c) 2016-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDOOMSDAY_RESOURCE_COLORPALETTES_H
#define LIBDOOMSDAY_RESOURCE_COLORPALETTES_H

#include "colorpalette.h"

namespace res {

class LIBDOOMSDAY_PUBLIC ColorPalettes
{
public:
    /// Notified when a new palette is added to the collection.
    DE_AUDIENCE(Addition, void colorPaletteAdded(ColorPalette &))

public:
    ColorPalettes();

    /**
     * Returns the total number of color palettes.
     */
    int colorPaletteCount() const;

    /**
     * Destroys all the color palettes.
     */
    void clearAllColorPalettes();

    /**
     * Returns the ColorPalette associated with unique @a id.
     */
    ColorPalette &colorPalette(const de::Id &id) const;

    /**
     * Returns the symbolic name of the specified color @a palette. A zero-length
     * string is returned if no name is associated.
     */
    de::String colorPaletteName(ColorPalette &palette) const;

    /**
     * Returns @c true iff a ColorPalette with the specified @a name is present.
     */
    bool hasColorPalette(const de::String& name) const;

    /**
     * Returns the ColorPalette associated with @a name.
     *
     * @see hasColorPalette()
     */
    ColorPalette &colorPalette(const de::String& name) const;

    /**
     * @param newPalette  Color palette to add. Ownership of the palette is given
     *                    to the resource system.
     * @param name        Symbolic name of the color palette.
     */
    void addColorPalette(ColorPalette &newPalette, const de::String &name = de::String());

    /**
     * Returns the unique identifier of the current default color palette.
     */
    de::Id defaultColorPalette() const;

    /**
     * Change the default color palette.
     *
     * @param newDefaultPalette  The color palette to make default.
     */
    void setDefaultColorPalette(ColorPalette *newDefaultPalette);

private:
    DE_PRIVATE(d)
};

} // namespace res

#endif // LIBDOOMSDAY_RESOURCE_COLORPALETTES_H
