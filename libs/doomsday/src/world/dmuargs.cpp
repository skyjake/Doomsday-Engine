/** @file dmuargs.cpp Doomsday Map Update (DMU) API arguments.
 *
 * @authors Copyright © 2006-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "doomsday/world/mapelement.h"
#include "dd_share.h"

#include <de/log.h>
#include <de/c_wrapper.h>

using namespace de;

namespace world {

static DmuArgs::PointerToIndexFunc ptrToIndexFunc;

/**
 * Convert propertyType enum constant into a string for error/debug messages.
 */
static const char *value_Str(dint val)
{
    static char valStr[40];
    struct val_s {
        dint val;
        const char *str;
    } valuetypes[] = {
        { DDVT_BOOL,        "DDVT_BOOL" },
        { DDVT_BYTE,        "DDVT_BYTE" },
        { DDVT_SHORT,       "DDVT_SHORT" },
        { DDVT_INT,         "DDVT_INT" },
        { DDVT_UINT,        "DDVT_UINT" },
        { DDVT_FIXED,       "DDVT_FIXED" },
        { DDVT_ANGLE,       "DDVT_ANGLE" },
        { DDVT_FLOAT,       "DDVT_FLOAT" },
        { DDVT_DOUBLE,      "DDVT_DOUBLE" },
        { DDVT_LONG,        "DDVT_LONG" },
        { DDVT_ULONG,       "DDVT_ULONG" },
        { DDVT_PTR,         "DDVT_PTR" },
        { DDVT_BLENDMODE,   "DDVT_BLENDMODE" },
        { 0, nullptr }
    };

    for (duint i = 0; valuetypes[i].str; ++i)
    {
        if (valuetypes[i].val == val)
            return valuetypes[i].str;
    }

    sprintf(valStr, "(unnamed %i)", val);
    return valStr;
}

DmuArgs::DmuArgs(int type, uint prop)
    : type          (type)
    , prop          (prop & ~DMU_FLAG_MASK)
    , modifiers     (prop & DMU_FLAG_MASK)
    , valueType     (DDVT_NONE)
    , booleanValues (0)
    , byteValues    (0)
    , intValues     (0)
    , fixedValues   (0)
    , floatValues   (0)
    , doubleValues  (0)
    , angleValues   (0)
    , ptrValues     (0)
{
    DE_ASSERT(VALID_DMU_ELEMENT_TYPE_ID(type));
}

void DmuArgs::value(valuetype_t dstValueType, void *dst, uint index) const
{
#define DMUARGS_LOG_ERROR(retval) { *d = retval; LOG_AS("DmuArgs::value"); \
    LOGDEV_MAP_ERROR("%s incompatible with value type %s") \
            << value_Str(dstValueType) << value_Str(valueType); }

    if (dstValueType == DDVT_FIXED)
    {
        fixed_t *d = (fixed_t *)dst;

        switch (valueType)
        {
        case DDVT_BYTE:
            *d = (byteValues[index] << FRACBITS);
            break;
        case DDVT_INT:
            *d = (intValues[index] << FRACBITS);
            break;
        case DDVT_FIXED:
            *d = fixedValues[index];
            break;
        case DDVT_FLOAT:
            *d = FLT2FIX(floatValues[index]);
            break;
        case DDVT_DOUBLE:
            *d = FLT2FIX(doubleValues[index]);
            break;
        default:
            DMUARGS_LOG_ERROR(0);
            break;
        }
    }
    else if (dstValueType == DDVT_FLOAT)
    {
        float *d = (float *)dst;

        switch (valueType)
        {
        case DDVT_BYTE:
            *d = byteValues[index];
            break;
        case DDVT_INT:
            *d = intValues[index];
            break;
        case DDVT_FIXED:
            *d = FIX2FLT(fixedValues[index]);
            break;
        case DDVT_FLOAT:
            *d = floatValues[index];
            break;
        case DDVT_DOUBLE:
            *d = (float)doubleValues[index];
            break;
        default:
            DMUARGS_LOG_ERROR(0);
            break;
        }
    }
    else if (dstValueType == DDVT_DOUBLE)
    {
        double *d = (double *)dst;

        switch (valueType)
        {
        case DDVT_BYTE:
            *d = byteValues[index];
            break;
        case DDVT_INT:
            *d = intValues[index];
            break;
        case DDVT_FIXED:
            *d = FIX2FLT(fixedValues[index]);
            break;
        case DDVT_FLOAT:
            *d = floatValues[index];
            break;
        case DDVT_DOUBLE:
            *d = doubleValues[index];
            break;
        default:
            DMUARGS_LOG_ERROR(0);
            break;
        }
    }
    else if (dstValueType == DDVT_BOOL)
    {
        dd_bool *d = (dd_bool *)dst;

        switch (valueType)
        {
        case DDVT_BOOL:
            *d = booleanValues[index];
            break;
        default:
            DMUARGS_LOG_ERROR(false);
            break;
        }
    }
    else if (dstValueType == DDVT_BYTE)
    {
        byte *d = (byte *)dst;

        switch (valueType)
        {
        case DDVT_BOOL:
            *d = booleanValues[index];
            break;
        case DDVT_BYTE:
            *d = byteValues[index];
            break;
        case DDVT_INT:
            *d = intValues[index];
            break;
        case DDVT_FLOAT:
            *d = (byte) floatValues[index];
            break;
        case DDVT_DOUBLE:
            *d = (byte) doubleValues[index];
            break;
        default:
            DMUARGS_LOG_ERROR(0);
            break;
        }
    }
    else if (dstValueType == DDVT_INT)
    {
        int *d = (int *)dst;

        switch (valueType)
        {
        case DDVT_BOOL:
            *d = booleanValues[index];
            break;
        case DDVT_BYTE:
            *d = byteValues[index];
            break;
        case DDVT_INT:
            *d = intValues[index];
            break;
        case DDVT_FLOAT:
            *d = floatValues[index];
            break;
        case DDVT_DOUBLE:
            *d = doubleValues[index];
            break;
        case DDVT_FIXED:
            *d = (fixedValues[index] >> FRACBITS);
            break;
        default:
            DMUARGS_LOG_ERROR(0);
            break;
        }
    }
    else if (dstValueType == DDVT_SHORT)
    {
        short *d = (short *)dst;

        switch (valueType)
        {
        case DDVT_BOOL:
            *d = booleanValues[index];
            break;
        case DDVT_BYTE:
            *d = byteValues[index];
            break;
        case DDVT_INT:
            *d = intValues[index];
            break;
        case DDVT_FLOAT:
            *d = floatValues[index];
            break;
        case DDVT_DOUBLE:
            *d = doubleValues[index];
            break;
        case DDVT_FIXED:
            *d = (fixedValues[index] >> FRACBITS);
            break;
        default:
            DMUARGS_LOG_ERROR(0);
            break;
        }
    }
    else if (dstValueType == DDVT_ANGLE)
    {
        angle_t *d = (angle_t *)dst;

        switch (valueType)
        {
        case DDVT_ANGLE:
            *d = angleValues[index];
            break;
        default:
            DMUARGS_LOG_ERROR(0);
            break;
        }
    }
    else if (dstValueType == DDVT_BLENDMODE)
    {
        blendmode_t *d = (blendmode_t *)dst;

        switch (valueType)
        {
        case DDVT_INT:
            if (intValues[index] >= 0 && intValues[index] < DDNUM_BLENDMODES)
            {
                *d = blendmode_t(intValues[index]);
            }
            else
            {
                LOG_AS("DmuArgs::setValue");
                LOGDEV_MAP_ERROR("%i is not a valid value for DDVT_BLENDMODE") << intValues[index];
            }
            break;
        default:
            DMUARGS_LOG_ERROR(BM_NORMAL);
            break;
        }
    }
    else if (dstValueType == DDVT_PTR)
    {
        void **d = (void **)dst;

        switch (valueType)
        {
        case DDVT_PTR:
            *d = ptrValues[index];
            break;
        default:
            DMUARGS_LOG_ERROR(nullptr);
            break;
        }
    }
    else
    {
        LOG_AS("DmuArgs::value");
        LOGDEV_MAP_ERROR("Unknown value type %i") << dstValueType;
    }

#undef DMUARGS_LOG_ERROR
}

void DmuArgs::setValue(valuetype_t srcValueType, const void *src, uint index)
{
#define DMUARGS_LOG_ERROR() { LOG_AS("DmuArgs::setValue"); \
    LOGDEV_MAP_ERROR("%s incompatible with value type %s") \
            << value_Str(srcValueType) << value_Str(valueType); }

    if (srcValueType == DDVT_FIXED)
    {
        const fixed_t *s = (const fixed_t *)src;

        switch (valueType)
        {
        case DDVT_BYTE:
            byteValues[index] = (*s >> FRACBITS);
            break;
        case DDVT_INT:
            intValues[index] = (*s >> FRACBITS);
            break;
        case DDVT_FIXED:
            fixedValues[index] = *s;
            break;
        case DDVT_FLOAT:
            floatValues[index] = FIX2FLT(*s);
            break;
        case DDVT_DOUBLE:
            doubleValues[index] = FIX2FLT(*s);
            break;
        default:
            DMUARGS_LOG_ERROR();
            break;
        }
    }
    else if (srcValueType == DDVT_FLOAT)
    {
        const float *s = (const float *)src;

        switch (valueType)
        {
        case DDVT_BYTE:
            byteValues[index] = *s;
            break;
        case DDVT_INT:
            intValues[index] = (int) *s;
            break;
        case DDVT_FIXED:
            fixedValues[index] = FLT2FIX(*s);
            break;
        case DDVT_FLOAT:
            floatValues[index] = *s;
            break;
        case DDVT_DOUBLE:
            doubleValues[index] = (double)*s;
            break;
        default:
            DMUARGS_LOG_ERROR();
            break;
        }
    }
    else if (srcValueType == DDVT_DOUBLE)
    {
        const double *s = (const double *)src;

        switch (valueType)
        {
        case DDVT_BYTE:
            byteValues[index] = (byte)*s;
            break;
        case DDVT_INT:
            intValues[index] = (int) *s;
            break;
        case DDVT_FIXED:
            fixedValues[index] = FLT2FIX(*s);
            break;
        case DDVT_FLOAT:
            floatValues[index] = (float)*s;
            break;
        case DDVT_DOUBLE:
            doubleValues[index] = *s;
            break;
        default:
            DMUARGS_LOG_ERROR();
            break;
        }
    }
    else if (srcValueType == DDVT_BOOL)
    {
        const dd_bool *s = (const dd_bool *)src;

        switch (valueType)
        {
        case DDVT_BOOL:
            booleanValues[index] = *s;
            break;
        default:
            DMUARGS_LOG_ERROR();
            break;
        }
    }
    else if (srcValueType == DDVT_BYTE)
    {
        const byte *s = (const byte *)src;

        switch (valueType)
        {
        case DDVT_BOOL:
            booleanValues[index] = *s;
            break;
        case DDVT_BYTE:
            byteValues[index] = *s;
            break;
        case DDVT_INT:
            intValues[index] = *s;
            break;
        case DDVT_FLOAT:
            floatValues[index] = *s;
            break;
        case DDVT_DOUBLE:
            doubleValues[index] = *s;
            break;
        default:
            DMUARGS_LOG_ERROR();
            break;
        }
    }
    else if (srcValueType == DDVT_INT)
    {
        const int *s = (const int *)src;

        switch (valueType)
        {
        case DDVT_BOOL:
            booleanValues[index] = *s;
            break;
        case DDVT_BYTE:
            byteValues[index] = *s;
            break;
        case DDVT_INT:
            intValues[index] = *s;
            break;
        case DDVT_FLOAT:
            floatValues[index] = *s;
            break;
        case DDVT_DOUBLE:
            doubleValues[index] = *s;
            break;
        case DDVT_FIXED:
            fixedValues[index] = (*s << FRACBITS);
            break;
        default:
            DMUARGS_LOG_ERROR();
            break;
        }
    }
    else if (srcValueType == DDVT_SHORT)
    {
        const short *s = (const short *)src;

        switch (valueType)
        {
        case DDVT_BOOL:
            booleanValues[index] = *s;
            break;
        case DDVT_BYTE:
            byteValues[index] = *s;
            break;
        case DDVT_INT:
            intValues[index] = *s;
            break;
        case DDVT_FLOAT:
            floatValues[index] = *s;
            break;
        case DDVT_DOUBLE:
            doubleValues[index] = *s;
            break;
        case DDVT_FIXED:
            fixedValues[index] = (*s << FRACBITS);
            break;
        default:
            DMUARGS_LOG_ERROR();
            break;
        }
    }
    else if (srcValueType == DDVT_ANGLE)
    {
        const angle_t *s = (const angle_t *)src;

        switch (valueType)
        {
        case DDVT_ANGLE:
            angleValues[index] = *s;
            break;
        default:
            DMUARGS_LOG_ERROR();
            break;
        }
    }
    else if (srcValueType == DDVT_BLENDMODE)
    {
        const blendmode_t *s = (const blendmode_t *)src;

        switch (valueType)
        {
        case DDVT_INT:
            intValues[index] = *s;
            break;
        default:
            DMUARGS_LOG_ERROR();
            break;
        }
    }
    else if (srcValueType == DDVT_PTR)
    {
        const void *const *s = (const void *const *)src;

        switch (valueType)
        {
        case DDVT_INT:
            // Attempt automatic conversion using P_ToIndex(). Naturally only
            // works with map elements. Failure leads into a fatal error.
            DE_ASSERT(ptrToIndexFunc);
            intValues[index] = ptrToIndexFunc(*s);
            break;
        case DDVT_PTR:
            ptrValues[index] = (void *) *s;
            break;
        default:
            DMUARGS_LOG_ERROR();
            break;
        }
    }
    else
    {
        LOG_AS("DmuArgs::setValue");
        LOGDEV_MAP_ERROR("Unknown value type %i") << srcValueType;
    }
#undef DMUARGS_LOG_ERROR
}

void DmuArgs::setPointerToIndexFunc(PointerToIndexFunc func)
{
    ptrToIndexFunc = func;
}

} // namespace world
