/** @file defs/model.h  Model definition accessor.
 *
 * @authors Copyright (c) 2014-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDOOMSDAY_DEFN_MODEL_H
#define LIBDOOMSDAY_DEFN_MODEL_H

#include "definition.h"
#include <de/recordaccessor.h>

namespace defn {

/**
 * Utility for handling model definitions.
 */
class LIBDOOMSDAY_PUBLIC Model : public Definition
{
public:
    Model()                    : Definition() {}
    Model(const Model &other)  : Definition(other) {}
    Model(de::Record &d)       : Definition(d) {}
    Model(const de::Record &d) : Definition(d) {}

    void resetToDefaults();

    de::Record &addSub();
    int subCount() const;
    bool hasSub(int index) const;
    de::Record &sub(int index);
    const de::Record &sub(int index) const;

    void cleanupAfterParsing(const de::Record &prev);
};

} // namespace defn

#endif // LIBDOOMSDAY_DEFN_MODEL_H
