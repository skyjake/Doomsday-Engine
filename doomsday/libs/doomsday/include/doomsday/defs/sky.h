/** @file defs/sky.h  Sky definition accessor.
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

#ifndef LIBDOOMSDAY_DEFN_SKY_H
#define LIBDOOMSDAY_DEFN_SKY_H

#include "definition.h"
#include <de/recordaccessor.h>

namespace defn {

// Sky flags.
#define SIF_DRAW_SPHERE     0x1 ///< Always draw the sky sphere.

// Sky layer flags.
#define SLF_ENABLE          0x1  ///< @c true= enable the layer.
#define SLF_MASK            0x2  ///< @c true= mask the layer.

#define DEFAULT_SKY_HEIGHT               ( .666667f )
#define DEFAULT_SKY_HORIZON_OFFSET       ( -0.105f )
#define DEFAULT_SKY_SPHERE_XOFFSET       ( 0 )
#define DEFAULT_SKY_SPHERE_FADEOUT_LIMIT ( .3f )

/**
 * Utility for handling sky definitions.
 */
class LIBDOOMSDAY_PUBLIC Sky : public Definition
{
public:
    Sky()                    : Definition() {}
    Sky(const Sky &other)    : Definition(other) {}
    Sky(de::Record &d)       : Definition(d) {}
    Sky(const de::Record &d) : Definition(d) {}

    void resetToDefaults();

    de::Record &addLayer();
    int layerCount() const;
    bool hasLayer(int index) const;
    de::Record &layer(int index);
    const de::Record &layer(int index) const;

    de::Record &addModel();
    int modelCount() const;
    bool hasModel(int index) const;
    de::Record &model(int index);
    const de::Record &model(int index) const;
};

} // namespace defn

#endif // LIBDOOMSDAY_DEFN_SKY_H
