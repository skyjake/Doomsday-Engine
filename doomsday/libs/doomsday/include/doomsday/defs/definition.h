/** @file definition.h  Base class for definition record accessors.
 *
 * @authors Copyright © 2014-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDOOMSDAY_DEFN_DEFINITION_H
#define LIBDOOMSDAY_DEFN_DEFINITION_H

#include "../libdoomsday.h"
#include <de/recordaccessor.h>

namespace defn {

/**
 * Base class for definition record accessors.
 * @ingroup data
 */
class LIBDOOMSDAY_PUBLIC Definition : public de::RecordAccessor
{
public:
    Definition()                        : RecordAccessor(0) {}
    Definition(const Definition &other) : RecordAccessor(other) {}
    Definition(de::Record &d)           : RecordAccessor(d) {}
    Definition(const de::Record &d)     : RecordAccessor(d) {}

    virtual ~Definition();

    de::Record &def();
    const de::Record &def() const;

    /**
     * Determines if this definition accessor points to a record.
     */
    operator bool() const;

    de::String id() const;

    /**
     * Returns the ordinal of the definition. Ordinals are automatically set by
     * DEDRegister as definitions are created.
     */
    int order() const;

    /**
     * Inserts the default members into the definition. All definitions are required to
     * implement this, as it is automatically called for all newly created definitions.
     *
     * All definitions share some common members, so derived classes are required to
     * call this before inserting their own members.
     */
    virtual void resetToDefaults();

    static de::String const VAR_ID;    // id
    static de::String const VAR_ORDER; // __order__
};

} // namespace defn

#endif // LIBDOOMSDAY_DEFN_DEFINITION_H
