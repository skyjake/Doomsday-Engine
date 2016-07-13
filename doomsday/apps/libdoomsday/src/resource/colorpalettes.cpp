/** @file colorpalettes.cpp
 *
 * @authors Copyright © 2013-2015 Daniel Swanson <danij@dengine.net>
 * @authors Copyright (c) 2016 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "doomsday/resource/colorpalettes.h"
#include "doomsday/resource/resources.h"

#include <QMap>

using namespace de;

namespace res {

DENG2_PIMPL_NOREF(ColorPalettes)
{
    typedef QMap<Id::Type, ColorPalette *> ColorPalettes;
    ColorPalettes colorPalettes; // owned

    typedef QMap<String, ColorPalette *> ColorPaletteNames;
    ColorPaletteNames colorPaletteNames;

    Id defaultColorPalette { Id::None };

    ~Impl()
    {
        clear();
    }

    void clear()
    {
        qDeleteAll(colorPalettes);
        colorPalettes.clear();
        colorPaletteNames.clear();

        defaultColorPalette = 0;
    }
};

ColorPalettes::ColorPalettes()
    : d(new Impl)
{}

void ColorPalettes::clearAllColorPalettes()
{
    d->clear();
}

dint ColorPalettes::colorPaletteCount() const
{
    return d->colorPalettes.count();
}

ColorPalette &ColorPalettes::colorPalette(Id const &id) const
{
    auto found = d->colorPalettes.find(id.isNone()? d->defaultColorPalette : id);
    if(found != d->colorPalettes.end()) return *found.value();
    /// @throw MissingResourceError An unknown/invalid id was specified.
    throw Resources::MissingResourceError("ColorPalettes::colorPalette",
                                          "Invalid ID " + id.asText());
}

String ColorPalettes::colorPaletteName(ColorPalette &palette) const
{
    QList<String> const names = d->colorPaletteNames.keys(&palette);
    if(!names.isEmpty())
    {
        return names.first();
    }
    return String();
}

bool ColorPalettes::hasColorPalette(String name) const
{
    return d->colorPaletteNames.contains(name);
}

ColorPalette &ColorPalettes::colorPalette(String name) const
{
    auto found = d->colorPaletteNames.find(name);
    if(found != d->colorPaletteNames.end()) return *found.value();
    /// @throw MissingResourceError An unknown name was specified.
    throw Resources::MissingResourceError("ColorPalettes::colorPalette", "Unknown name '" + name + "'");
}

void ColorPalettes::addColorPalette(res::ColorPalette &newPalette, String const &name)
{
    // Do we already own this palette?
    if(d->colorPalettes.contains(newPalette.id()))
        return;

    d->colorPalettes.insert(newPalette.id(), &newPalette);

    if(!name.isEmpty())
    {
        d->colorPaletteNames.insert(name, &newPalette);
    }

    // If this is the first palette automatically set it as the default.
    if(d->colorPalettes.count() == 1)
    {
        d->defaultColorPalette = newPalette.id();
    }
}

Id ColorPalettes::defaultColorPalette() const
{
    return d->defaultColorPalette;
}

void ColorPalettes::setDefaultColorPalette(res::ColorPalette *newDefaultPalette)
{
    d->defaultColorPalette = newDefaultPalette ? newDefaultPalette->id().asUInt32() : 0;
}

} // namespace res
