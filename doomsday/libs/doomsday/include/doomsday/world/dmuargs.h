/** @file dmuargs.h
 *
 * @authors Copyright © 2006-2014 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDOOMSDAY_WORLD_DMUARGS_H
#define LIBDOOMSDAY_WORLD_DMUARGS_H

#include "../libdoomsday.h"
#include "valuetype.h"
#include <de/legacy/types.h>
#include <functional>

namespace world {

/**
 * Encapsulates the arguments used when routing DMU API calls to map elements.
 */
class LIBDOOMSDAY_PUBLIC DmuArgs
{
public: /// @todo make private
    int type;
    uint prop;
    int modifiers; /// Property modifiers (e.g., line of sector)
    valuetype_t valueType;
    dd_bool *booleanValues;
    byte *byteValues;
    int *intValues;
    fixed_t *fixedValues;
    float *floatValues;
    double *doubleValues;
    angle_t *angleValues;
    void **ptrValues;

    DmuArgs(int type, uint prop);

    /**
     * Read the value of an argument. Does some basic type checking so that
     * incompatible types are not assigned. Simple conversions are also done,
     * e.g., float to fixed.
     */
    void value(valuetype_t valueType, void *dst, uint index) const;

    /**
     * Change the value of an argument. Does some basic type checking so that
     * incompatible types are not assigned. Simple conversions are also done,
     * e.g., float to fixed.
     */
    void setValue(valuetype_t valueType, const void *src, uint index);

    typedef std::function<int (const void *)> PointerToIndexFunc;
    static void setPointerToIndexFunc(PointerToIndexFunc func);
};

} // namespace world

#endif // LIBDOOMSDAY_WORLD_DMUARGS_H
