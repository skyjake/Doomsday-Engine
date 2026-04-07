/** @file colorpalettes.cpp
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

#include "doomsday/res/colorpalettes.h"
#include "doomsday/res/resources.h"

#include <de/keymap.h>

using namespace de;

namespace res {

DE_PIMPL_NOREF(ColorPalettes)
{
    typedef KeyMap<Id::Type, ColorPalette *> ColorPalettes;
    ColorPalettes colorPalettes; // owned

    typedef KeyMap<String, ColorPalette *> ColorPaletteNames;
    ColorPaletteNames colorPaletteNames;

    Id defaultColorPalette { Id::None };

    ~Impl()
    {
        clear();
    }

    void clear()
    {
        colorPalettes.deleteAll();
        colorPalettes.clear();
        colorPaletteNames.clear();

        defaultColorPalette = 0;
    }

    DE_PIMPL_AUDIENCE(Addition)
};

DE_AUDIENCE_METHOD(ColorPalettes, Addition)

ColorPalettes::ColorPalettes()
    : d(new Impl)
{}

void ColorPalettes::clearAllColorPalettes()
{
    d->clear();
}

dint ColorPalettes::colorPaletteCount() const
{
    return dint(d->colorPalettes.size());
}

ColorPalette &ColorPalettes::colorPalette(const Id &id) const
{
    auto found = d->colorPalettes.find(id.isNone()? d->defaultColorPalette : id);
    if (found != d->colorPalettes.end()) return *found->second;
    /// @throw MissingResourceError An unknown/invalid id was specified.
    throw Resources::MissingResourceError("ColorPalettes::colorPalette",
                                          "Invalid ID " + id.asText());
}

String ColorPalettes::colorPaletteName(ColorPalette &palette) const
{
    for (auto &i : d->colorPaletteNames)
    {
        if (i.second == &palette) return i.first;
    }
    return {};
}

bool ColorPalettes::hasColorPalette(const String& name) const
{
    return d->colorPaletteNames.contains(name);
}

ColorPalette &ColorPalettes::colorPalette(const String& name) const
{
    auto found = d->colorPaletteNames.find(name);
    if (found != d->colorPaletteNames.end()) return *found->second;
    /// @throw MissingResourceError An unknown name was specified.
    throw Resources::MissingResourceError("ColorPalettes::colorPalette", "Unknown name '" + name + "'");
}

void ColorPalettes::addColorPalette(ColorPalette &newPalette, const String &name)
{
    // Do we already own this palette?
    if (d->colorPalettes.contains(newPalette.id()))
        return;

    d->colorPalettes.insert(newPalette.id(), &newPalette);

    if (!name.isEmpty())
    {
        d->colorPaletteNames.insert(name, &newPalette);
    }

    // If this is the first palette automatically set it as the default.
    if (d->colorPalettes.size() == 1)
    {
        d->defaultColorPalette = newPalette.id();
    }

    DE_NOTIFY(Addition, i) i->colorPaletteAdded(newPalette);
}

Id ColorPalettes::defaultColorPalette() const
{
    return d->defaultColorPalette;
}

void ColorPalettes::setDefaultColorPalette(ColorPalette *newDefaultPalette)
{
    d->defaultColorPalette = newDefaultPalette ? newDefaultPalette->id().asUInt32() : 0;
}

} // namespace res
