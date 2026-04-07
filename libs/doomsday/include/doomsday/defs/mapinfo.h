/** @file defs/mapinfo.h  MapInfo definition accessor.
 *
 * @authors Copyright © 2014 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDOOMSDAY_DEFN_MAPINFO_H
#define LIBDOOMSDAY_DEFN_MAPINFO_H

#include "definition.h"
#include <de/recordaccessor.h>

/// @todo These values should be tweaked a bit.
#define DEFAULT_FOG_START       0
#define DEFAULT_FOG_END         2100
#define DEFAULT_FOG_DENSITY     0.0001f
#define DEFAULT_FOG_COLOR_RED   138.0f/255
#define DEFAULT_FOG_COLOR_GREEN 138.0f/255
#define DEFAULT_FOG_COLOR_BLUE  138.0f/255

namespace defn {

/**
 * Utility for handling mapinfo definitions.
 */
class LIBDOOMSDAY_PUBLIC MapInfo : public Definition
{
public:
    MapInfo()                     : Definition() {}
    MapInfo(const MapInfo &other) : Definition(other) {}
    MapInfo(de::Record &d)        : Definition(d) {}
    MapInfo(const de::Record &d)  : Definition(d) {}

    void resetToDefaults();
};

} // namespace defn

#endif // LIBDOOMSDAY_DEFN_MAPINFO_H
