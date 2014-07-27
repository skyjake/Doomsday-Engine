/** @file defs/sky.h  Model definition accessor.
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

#include "../libdoomsday.h"
#include <de/RecordAccessor>

namespace defn {

// Sky flags.
#define SIF_DRAW_SPHERE     0x1 ///< Always draw the sky sphere.

/**
 * Utility for handling sky definitions.
 */
class LIBDOOMSDAY_PUBLIC Sky : public de::RecordAccessor
{
public:
    Sky() : RecordAccessor(0), _def(0) {}
    Sky(Sky const &other)
        : RecordAccessor(other)
        , _def(other._def) {}
    Sky(de::Record &d) : RecordAccessor(d), _def(&d) {}
    Sky(de::Record const &d) : RecordAccessor(d), _def(0) {}

    void resetToDefaults();

    Sky &operator = (de::Record *d);

    operator bool() const;
    int order() const;

    de::Record &addLayer();

    int layerCount() const;
    bool hasLayer(int index) const;
    de::Record &layer(int index);
    de::Record const &layer(int index) const;

    de::Record &addModel();

    int modelCount() const;
    bool hasModel(int index) const;
    de::Record &model(int index);
    de::Record const &model(int index) const;

private:
    de::Record *_def; ///< Modifiable access.
};

} // namespace defn

#endif // LIBDOOMSDAY_DEFN_SKY_H
