/** @file dmuargs.h Doomsday Map Update (DMU) API arguments.
 *
 * @authors Copyright © 2006-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
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
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#ifndef DENG_WORLD_DMUARGS_H
#define DENG_WORLD_DMUARGS_H

#include "api_mapedit.h" // valuetype_t

/**
 * Encapsulates the arguments used when routing DMU API calls to map elements.
 *
 * @ingroup world
 */
class DmuArgs
{
public: /// @todo make private
    int type;
    uint prop;
    int modifiers; /// Property modifiers (e.g., line of sector)
    valuetype_t valueType;
    boolean *booleanValues;
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
    void setValue(valuetype_t valueType, void const *src, uint index);
};

#endif // DENG_WORLD_DMUARGS_H
