/** @file dmuargs.cpp Doomsday Map Update (DMU) API arguments.
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

#include <de/Log>

#include "de_base.h"

#include "world/dmuargs.h"

using namespace de;

DmuArgs::DmuArgs(int type, uint prop)
    : type     (type),
      prop     (prop & ~DMU_FLAG_MASK),
      modifiers(prop & DMU_FLAG_MASK),
      valueType(DDVT_NONE),
      booleanValues(0),
      byteValues(0),
      intValues(0),
      fixedValues(0),
      floatValues(0),
      doubleValues(0),
      angleValues(0),
      ptrValues(0)
{
    DENG_ASSERT(VALID_DMU_ELEMENT_TYPE_ID(type));
}

void DmuArgs::value(valuetype_t dstValueType, void *dst, uint index) const
{
    if(dstValueType == DDVT_FIXED)
    {
        fixed_t *d = (fixed_t *)dst;

        switch(valueType)
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
        default: {
            /// @todo Throw exception.
            QByteArray msg = String("DmuArgs::value: DDVT_FIXED incompatible with value type %1.").arg(value_Str(valueType)).toUtf8();
            App_FatalError(msg.constData());
            }
        }
    }
    else if(dstValueType == DDVT_FLOAT)
    {
        float *d = (float *)dst;

        switch(valueType)
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
        default: {
            /// @todo Throw exception.
            QByteArray msg = String("DmuArgs::value: DDVT_FLOAT incompatible with value type %1.").arg(value_Str(valueType)).toUtf8();
            App_FatalError(msg.constData());
            }
        }
    }
    else if(dstValueType == DDVT_DOUBLE)
    {
        double *d = (double *)dst;

        switch(valueType)
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
        default: {
            /// @todo Throw exception.
            QByteArray msg = String("DmuArgs::value: DDVT_DOUBLE incompatible with value type %1.").arg(value_Str(valueType)).toUtf8();
            App_FatalError(msg.constData());
            }
        }
    }
    else if(dstValueType == DDVT_BOOL)
    {
        boolean *d = (boolean *)dst;

        switch(valueType)
        {
        case DDVT_BOOL:
            *d = booleanValues[index];
            break;
        default: {
            /// @todo Throw exception.
            QByteArray msg = String("DmuArgs::value: DDVT_BOOL incompatible with value type %1.").arg(value_Str(valueType)).toUtf8();
            App_FatalError(msg.constData());
            }
        }
    }
    else if(dstValueType == DDVT_BYTE)
    {
        byte *d = (byte *)dst;

        switch(valueType)
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
        default: {
            /// @todo Throw exception.
            QByteArray msg = String("DmuArgs::value: DDVT_BYTE incompatible with value type %1.").arg(value_Str(valueType)).toUtf8();
            App_FatalError(msg.constData());
            }
        }
    }
    else if(dstValueType == DDVT_INT)
    {
        int *d = (int *)dst;

        switch(valueType)
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
        default: {
            /// @todo Throw exception.
            QByteArray msg = String("DmuArgs::value: DDVT_INT incompatible with value type %1.").arg(value_Str(valueType)).toUtf8();
            App_FatalError(msg.constData());
            }
        }
    }
    else if(dstValueType == DDVT_SHORT)
    {
        short *d = (short *)dst;

        switch(valueType)
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
        default: {
            /// @todo Throw exception.
            QByteArray msg = String("DmuArgs::value: DDVT_SHORT incompatible with value type %1.").arg(value_Str(valueType)).toUtf8();
            App_FatalError(msg.constData());
            }
        }
    }
    else if(dstValueType == DDVT_ANGLE)
    {
        angle_t *d = (angle_t *)dst;

        switch(valueType)
        {
        case DDVT_ANGLE:
            *d = angleValues[index];
            break;
        default: {
            /// @todo Throw exception.
            QByteArray msg = String("DmuArgs::value: DDVT_ANGLE incompatible with value type %1.").arg(value_Str(valueType)).toUtf8();
            App_FatalError(msg.constData());
            }
        }
    }
    else if(dstValueType == DDVT_BLENDMODE)
    {
        blendmode_t *d = (blendmode_t *)dst;

        switch(valueType)
        {
        case DDVT_INT:
            if(intValues[index] > DDNUM_BLENDMODES || intValues[index] < 0)
            {
                QByteArray msg = String("DmuArgs::value: %1 is not a valid value for DDVT_BLENDMODE.").arg(intValues[index]).toUtf8();
                App_FatalError(msg.constData());
            }

            *d = blendmode_t(intValues[index]);
            break;
        default: {
            /// @todo Throw exception.
            QByteArray msg = String("DmuArgs::value: DDVT_BLENDMODE incompatible with value type %1.").arg(value_Str(valueType)).toUtf8();
            App_FatalError(msg.constData());
            }
        }
    }
    else if(dstValueType == DDVT_PTR)
    {
        void **d = (void **)dst;

        switch(valueType)
        {
        case DDVT_PTR:
            *d = ptrValues[index];
            break;
        default: {
            /// @todo Throw exception.
            QByteArray msg = String("DmuArgs::value: DDVT_PTR incompatible with value type %1.").arg(value_Str(valueType)).toUtf8();
            App_FatalError(msg.constData());
            }
        }
    }
    else
    {
        /// @todo Throw exception.
        QByteArray msg = String("DmuArgs::value: unknown value type %1.").arg(dstValueType).toUtf8();
        App_FatalError(msg.constData());
    }
}

void DmuArgs::setValue(valuetype_t srcValueType, void const *src, uint index)
{
    if(srcValueType == DDVT_FIXED)
    {
        fixed_t const *s = (fixed_t const *)src;

        switch(valueType)
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
        default: {
            QByteArray msg = String("DmuArgs::setValue: DDVT_FIXED incompatible with value type %1.").arg(value_Str(valueType)).toUtf8();
            App_FatalError(msg.constData());
            }
        }
    }
    else if(srcValueType == DDVT_FLOAT)
    {
        float const *s = (float const *)src;

        switch(valueType)
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
        default: {
            QByteArray msg = String("DmuArgs::setValue: DDVT_FLOAT incompatible with value type %1.").arg(value_Str(valueType)).toUtf8();
            App_FatalError(msg.constData());
            }
        }
    }
    else if(srcValueType == DDVT_DOUBLE)
    {
        double const *s = (double const *)src;

        switch(valueType)
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
        default: {
            QByteArray msg = String("DmuArgs::setValue: DDVT_DOUBLE incompatible with value type %1.").arg(value_Str(valueType)).toUtf8();
            App_FatalError(msg.constData());
            }
        }
    }
    else if(srcValueType == DDVT_BOOL)
    {
        boolean const *s = (boolean const *)src;

        switch(valueType)
        {
        case DDVT_BOOL:
            booleanValues[index] = *s;
            break;
        default: {
            QByteArray msg = String("DmuArgs::setValue: DDVT_BOOL incompatible with value type %1.").arg(value_Str(valueType)).toUtf8();
            App_FatalError(msg.constData());
            }
        }
    }
    else if(srcValueType == DDVT_BYTE)
    {
        byte const *s = (byte const *)src;

        switch(valueType)
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
        default: {
            QByteArray msg = String("DmuArgs::setValue: DDVT_BYTE incompatible with value type %1.").arg(value_Str(valueType)).toUtf8();
            App_FatalError(msg.constData());
            }
        }
    }
    else if(srcValueType == DDVT_INT)
    {
        int const *s = (int const *)src;

        switch(valueType)
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
        default: {
            QByteArray msg = String("DmuArgs::setValue: DDVT_INT incompatible with value type %1.").arg(value_Str(valueType)).toUtf8();
            App_FatalError(msg.constData());
            }
        }
    }
    else if(srcValueType == DDVT_SHORT)
    {
        short const *s = (short const *)src;

        switch(valueType)
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
        default: {
            QByteArray msg = String("DmuArgs::setValue: DDVT_SHORT incompatible with value type %1.").arg(value_Str(valueType)).toUtf8();
            App_FatalError(msg.constData());
            }
        }
    }
    else if(srcValueType == DDVT_ANGLE)
    {
        angle_t const *s = (angle_t const *)src;

        switch(valueType)
        {
        case DDVT_ANGLE:
            angleValues[index] = *s;
            break;
        default: {
            QByteArray msg = String("DmuArgs::setValue: DDVT_ANGLE incompatible with value type %1.").arg(value_Str(valueType)).toUtf8();
            App_FatalError(msg.constData());
            }
        }
    }
    else if(srcValueType == DDVT_BLENDMODE)
    {
        blendmode_t const *s = (blendmode_t const *)src;

        switch(valueType)
        {
        case DDVT_INT:
            intValues[index] = *s;
            break;
        default: {
            QByteArray msg = String("DmuArgs::setValue: DDVT_BLENDMODE incompatible with value type %1.").arg(value_Str(valueType)).toUtf8();
            App_FatalError(msg.constData());
            }
        }
    }
    else if(srcValueType == DDVT_PTR)
    {
        void const *const *s = (void const *const *)src;

        switch(valueType)
        {
        case DDVT_INT:
            // Attempt automatic conversion using P_ToIndex(). Naturally only
            // works with map elements. Failure leads into a fatal error.
            intValues[index] = P_ToIndex(*s);
            break;
        case DDVT_PTR:
            ptrValues[index] = (void *) *s;
            break;
        default: {
            QByteArray msg = String("DmuArgs::setValue: DDVT_PTR incompatible with value type %1.").arg(value_Str(valueType)).toUtf8();
            App_FatalError(msg.constData());
            }
        }
    }
    else
    {
        QByteArray msg = String("DmuArgs::setValue: unknown value type %1.").arg(srcValueType).toUtf8();
        App_FatalError(msg.constData());
    }
}
