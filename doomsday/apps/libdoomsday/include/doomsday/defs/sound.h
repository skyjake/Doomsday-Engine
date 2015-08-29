/** @file defs/sound.h  Sound definition accessor.
 *
 * @authors Copyright © 2015 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDOOMSDAY_DEFN_SOUND_H
#define LIBDOOMSDAY_DEFN_SOUND_H

#include "definition.h"
#include <de/RecordAccessor>

namespace defn {

/**
 * Utility for handling sound definitions.
 */
class LIBDOOMSDAY_PUBLIC Sound : public Definition
{
public:
    Sound()                    : Definition() {}
    Sound(Sound const &other)  : Definition(other) {}
    Sound(de::Record &d)       : Definition(d) {}
    Sound(de::Record const &d) : Definition(d) {}

    void resetToDefaults();
};

}  // namespace defn

#endif  // LIBDOOMSDAY_DEFN_SOUND_H
