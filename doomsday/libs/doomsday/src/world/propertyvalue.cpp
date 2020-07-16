/** @file propertyvalue.cpp  Data types for representing property values.
 * @ingroup data
 *
 * @authors Copyright &copy; 2013 Daniel Swanson <danij@dengine.net>
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

#include "doomsday/world/propertyvalue.h"

#include <de/error.h>

using namespace de;

PropertyValue *BuildPropertyValue(valuetype_t type, void *valueAdr)
{
    DE_ASSERT(valueAdr != nullptr);
    switch (type)
    {
    case DDVT_BYTE:     return new PropertyByteValue  (*(   (byte *) valueAdr));
    case DDVT_SHORT:    return new PropertyInt16Value (*(  (short *) valueAdr));
    case DDVT_INT:      return new PropertyInt32Value (*(    (int *) valueAdr));
    case DDVT_FIXED:    return new PropertyFixedValue (*((fixed_t *) valueAdr));
    case DDVT_ANGLE:    return new PropertyAngleValue (*((angle_t *) valueAdr));
    case DDVT_FLOAT:    return new PropertyFloatValue (*(  (float *) valueAdr));
    case DDVT_DOUBLE:   return new PropertyDoubleValue(*( (double *) valueAdr));
    default:
        throw Error("BuildPropertyValue", stringf("Unknown/not-supported value type %d", type));
    }
}
