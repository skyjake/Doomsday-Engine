/** @file decoration.h  Decoration definition accessor.
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

#ifndef LIBDOOMSDAY_DEFN_DECORATION_H
#define LIBDOOMSDAY_DEFN_DECORATION_H

#include "definition.h"
#include <de/recordaccessor.h>

// Flags for decoration definitions.
#define DCRF_NO_IWAD   0x1  ///< Don't use if from IWAD.
#define DCRF_PWAD      0x2  ///< Can use if from PWAD.
//#define DCRF_EXTERNAL  0x4  ///< Can use if from external resource.

namespace defn {

/**
 * Utility for handling oldschool decoration definitions.
 */
class LIBDOOMSDAY_PUBLIC Decoration : public Definition
{
public:
    Decoration()                        : Definition() {}
    Decoration(const Decoration &other) : Definition(other) {}
    Decoration(de::Record &d)           : Definition(d) {}
    Decoration(const de::Record &d)     : Definition(d) {}

    void resetToDefaults();

    int lightCount() const;
    bool hasLight(int index) const;

    de::Record       &light(int index);
    const de::Record &light(int index) const;

    de::Record &addLight();
};

}  // namespace defn

#endif  // LIBDOOMSDAY_DEFN_DECORATION_H
