/** @file material.h  Material definition accessor.
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

#ifndef LIBDOOMSDAY_DEFN_MATERIAL_H
#define LIBDOOMSDAY_DEFN_MATERIAL_H

#include "definition.h"
#include <de/recordaccessor.h>

namespace defn {

/**
 * Utility for handling material-decoration definitions.
 */
class LIBDOOMSDAY_PUBLIC MaterialDecoration : public Definition
{
public:
    MaterialDecoration()                                : Definition() {}
    MaterialDecoration(const MaterialDecoration &other) : Definition(other) {}
    MaterialDecoration(de::Record &d)                   : Definition(d) {}
    MaterialDecoration(const de::Record &d)             : Definition(d) {}

    void resetToDefaults();

    int stageCount() const;
    bool hasStage(int index) const;

    de::Record       &stage(int index);
    const de::Record &stage(int index) const;

    de::Record &addStage();
};

/**
 * Utility for handling material-layer definitions.
 */
class LIBDOOMSDAY_PUBLIC MaterialLayer : public Definition
{
public:
    MaterialLayer()                           : Definition() {}
    MaterialLayer(const MaterialLayer &other) : Definition(other) {}
    MaterialLayer(de::Record &d)              : Definition(d) {}
    MaterialLayer(const de::Record &d)        : Definition(d) {}

    void resetToDefaults();

    int stageCount() const;
    bool hasStage(int index) const;

    de::Record       &stage(int index);
    const de::Record &stage(int index) const;

    de::Record &addStage();
};

/**
 * Utility for handling material definitions.
 */
class LIBDOOMSDAY_PUBLIC Material : public Definition
{
public:
    Material()                      : Definition() {}
    Material(const Material &other) : Definition(other) {}
    Material(de::Record &d)         : Definition(d) {}
    Material(const de::Record &d)   : Definition(d) {}

    void resetToDefaults();

public:  // Layers: -----------------------------------------------------

    int layerCount() const;
    bool hasLayer(int index) const;

    de::Record       &layer(int index);
    const de::Record &layer(int index) const;

    de::Record &addLayer();

public:  // Decorations: ------------------------------------------------

    int decorationCount() const;
    bool hasDecoration(int index) const;

    de::Record       &decoration(int index);
    const de::Record &decoration(int index) const;

    de::Record &addDecoration();
};

}  // namespace defn

#endif  // LIBDOOMSDAY_DEFN_MATERIAL_H
