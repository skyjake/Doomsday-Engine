/** @file defs/music.h  Music definition accessor.
 *
 * @authors Copyright © 2014-2015 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDOOMSDAY_DEFN_MUSIC_H
#define LIBDOOMSDAY_DEFN_MUSIC_H

#include "definition.h"
#include <de/recordaccessor.h>

namespace defn {

/**
 * Utility for handling music definitions.
 */
class LIBDOOMSDAY_PUBLIC Music : public Definition
{
public:
    Music()                    : Definition() {}
    Music(const Music &other)  : Definition(other) {}
    Music(de::Record &d)       : Definition(d) {}
    Music(const de::Record &d) : Definition(d) {}

    void resetToDefaults();

    /**
     * Returns the associated CD track number if defined else @c 0.
     */
    int cdTrack() const;
};

} // namespace defn

#endif // LIBDOOMSDAY_DEFN_MUSIC_H
