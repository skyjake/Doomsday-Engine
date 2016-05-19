/** @file dmuobject.cpp  Base class for all DMU objects.
 *
 * @authors Copyright © 2006-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2016 Daniel Swanson <danij@dengine.net>
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

#include "dmuobject.h"

#include "dd_share.h"
#include <de/Log>
#include <de/c_wrapper.h>

namespace de {

/**
* Convert propertyType enum constant into a string for error/debug messages.
*/
static char const *value_Str(dint val)
{
    static char valStr[40];
    struct val_s {
        dint val;
        char const *str;
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

    for(duint i = 0; valuetypes[i].str; ++i)
    {
        if(valuetypes[i].val == val)
            return valuetypes[i].str;
    }

    sprintf(valStr, "(unnamed %i)", val);
    return valStr;
}

DmuObject::Args::Args(dint type, duint prop)
    : type(type)
    , prop(prop & ~DMU_FLAG_MASK)
    , modifiers(prop & DMU_FLAG_MASK)
    , valueType(DDVT_NONE)
{
    DENG_ASSERT(VALID_DMU_ELEMENT_TYPE_ID(type));
}

void DmuObject::Args::value(valuetype_t dstValueType, void *dst, duint index) const
{
    if(dstValueType == DDVT_FIXED)
    {
        auto *d = (fixed_t *)dst;

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
        default:
        {
            /// @todo Throw exception.
            QByteArray msg = String("DmuObject::Args::value: DDVT_FIXED incompatible with value type %1.").arg(value_Str(valueType)).toUtf8();
            App_FatalError(msg.constData());
        }
        }
    }
    else if(dstValueType == DDVT_FLOAT)
    {
        auto *d = (dfloat *)dst;

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
            *d = dfloat(doubleValues[index]);
            break;
        default:
        {
            /// @todo Throw exception.
            QByteArray msg = String("DmuObject::Args::value: DDVT_FLOAT incompatible with value type %1.").arg(value_Str(valueType)).toUtf8();
            App_FatalError(msg.constData());
        }
        }
    }
    else if(dstValueType == DDVT_DOUBLE)
    {
        auto *d = (ddouble *)dst;

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
        default:
        {
            /// @todo Throw exception.
            QByteArray msg = String("DmuObject::Args::value: DDVT_DOUBLE incompatible with value type %1.").arg(value_Str(valueType)).toUtf8();
            App_FatalError(msg.constData());
        }
        }
    }
    else if(dstValueType == DDVT_BOOL)
    {
        auto *d = (dd_bool *)dst;

        switch(valueType)
        {
        case DDVT_BOOL:
            *d = booleanValues[index];
            break;
        default:
        {
            /// @todo Throw exception.
            QByteArray msg = String("DmuObject::Args::value: DDVT_BOOL incompatible with value type %1.").arg(value_Str(valueType)).toUtf8();
            App_FatalError(msg.constData());
        }
        }
    }
    else if(dstValueType == DDVT_BYTE)
    {
        auto *d = (byte *)dst;

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
            *d = (byte)floatValues[index];
            break;
        case DDVT_DOUBLE:
            *d = (byte)doubleValues[index];
            break;
        default:
        {
            /// @todo Throw exception.
            QByteArray msg = String("DmuObject::Args::value: DDVT_BYTE incompatible with value type %1.").arg(value_Str(valueType)).toUtf8();
            App_FatalError(msg.constData());
        }
        }
    }
    else if(dstValueType == DDVT_INT)
    {
        auto *d = (dint *)dst;

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
        default:
        {
            /// @todo Throw exception.
            QByteArray msg = String("DmuObject::Args::value: DDVT_INT incompatible with value type %1.").arg(value_Str(valueType)).toUtf8();
            App_FatalError(msg.constData());
        }
        }
    }
    else if(dstValueType == DDVT_SHORT)
    {
        auto *d = (dshort *)dst;

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
        default:
        {
            /// @todo Throw exception.
            QByteArray msg = String("DmuObject::Args::value: DDVT_SHORT incompatible with value type %1.").arg(value_Str(valueType)).toUtf8();
            App_FatalError(msg.constData());
        }
        }
    }
    else if(dstValueType == DDVT_ANGLE)
    {
        auto *d = (angle_t *)dst;

        switch(valueType)
        {
        case DDVT_ANGLE:
            *d = angleValues[index];
            break;
        default:
        {
            /// @todo Throw exception.
            QByteArray msg = String("DmuObject::Args::value: DDVT_ANGLE incompatible with value type %1.").arg(value_Str(valueType)).toUtf8();
            App_FatalError(msg.constData());
        }
        }
    }
    else if(dstValueType == DDVT_BLENDMODE)
    {
        auto *d = (blendmode_t *)dst;

        switch(valueType)
        {
        case DDVT_INT:
            if(intValues[index] > DDNUM_BLENDMODES || intValues[index] < 0)
            {
                QByteArray msg = String("DmuObject::Args::value: %1 is not a valid value for DDVT_BLENDMODE.").arg(intValues[index]).toUtf8();
                App_FatalError(msg.constData());
            }

            *d = blendmode_t(intValues[index]);
            break;
        default:
        {
            /// @todo Throw exception.
            QByteArray msg = String("DmuObject::Args::value: DDVT_BLENDMODE incompatible with value type %1.").arg(value_Str(valueType)).toUtf8();
            App_FatalError(msg.constData());
        }
        }
    }
    else if(dstValueType == DDVT_PTR)
    {
        auto **d = (void **)dst;

        switch(valueType)
        {
        case DDVT_PTR:
            *d = ptrValues[index];
            break;
        default:
        {
            /// @todo Throw exception.
            QByteArray msg = String("DmuObject::Args::value: DDVT_PTR incompatible with value type %1.").arg(value_Str(valueType)).toUtf8();
            App_FatalError(msg.constData());
        }
        }
    }
    else
    {
        /// @todo Throw exception.
        QByteArray msg = String("DmuObject::Args::value: unknown value type %1.").arg(dstValueType).toUtf8();
        App_FatalError(msg.constData());
    }
}

void DmuObject::Args::setValue(valuetype_t srcValueType, void const *src, duint index)
{
    if(srcValueType == DDVT_FIXED)
    {
        auto const *s = (fixed_t const *)src;

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
        default:
        {
            QByteArray msg = String("DmuObject::Args::setValue: DDVT_FIXED incompatible with value type %1.").arg(value_Str(valueType)).toUtf8();
            App_FatalError(msg.constData());
        }
        }
    }
    else if(srcValueType == DDVT_FLOAT)
    {
        auto const *s = (dfloat const *)src;

        switch(valueType)
        {
        case DDVT_BYTE:
            byteValues[index] = *s;
            break;
        case DDVT_INT:
            intValues[index] = (int)*s;
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
        {
            QByteArray msg = String("DmuObject::Args::setValue: DDVT_FLOAT incompatible with value type %1.").arg(value_Str(valueType)).toUtf8();
            App_FatalError(msg.constData());
        }
        }
    }
    else if(srcValueType == DDVT_DOUBLE)
    {
        auto const *s = (ddouble const *)src;

        switch(valueType)
        {
        case DDVT_BYTE:
            byteValues[index] = (byte)*s;
            break;
        case DDVT_INT:
            intValues[index] = (int)*s;
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
        {
            QByteArray msg = String("DmuObject::Args::setValue: DDVT_DOUBLE incompatible with value type %1.").arg(value_Str(valueType)).toUtf8();
            App_FatalError(msg.constData());
        }
        }
    }
    else if(srcValueType == DDVT_BOOL)
    {
        auto const *s = (dd_bool const *)src;

        switch(valueType)
        {
        case DDVT_BOOL:
            booleanValues[index] = *s;
            break;
        default:
        {
            QByteArray msg = String("DmuObject::Args::setValue: DDVT_BOOL incompatible with value type %1.").arg(value_Str(valueType)).toUtf8();
            App_FatalError(msg.constData());
        }
        }
    }
    else if(srcValueType == DDVT_BYTE)
    {
        auto const *s = (byte const *)src;

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
        default:
        {
            QByteArray msg = String("DmuObject::Args::setValue: DDVT_BYTE incompatible with value type %1.").arg(value_Str(valueType)).toUtf8();
            App_FatalError(msg.constData());
        }
        }
    }
    else if(srcValueType == DDVT_INT)
    {
        auto const *s = (dint const *)src;

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
        default:
        {
            QByteArray msg = String("DmuObject::Args::setValue: DDVT_INT incompatible with value type %1.").arg(value_Str(valueType)).toUtf8();
            App_FatalError(msg.constData());
        }
        }
    }
    else if(srcValueType == DDVT_SHORT)
    {
        auto const *s = (dshort const *)src;

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
        default:
        {
            QByteArray msg = String("DmuObject::Args::setValue: DDVT_SHORT incompatible with value type %1.").arg(value_Str(valueType)).toUtf8();
            App_FatalError(msg.constData());
        }
        }
    }
    else if(srcValueType == DDVT_ANGLE)
    {
        auto const *s = (angle_t const *)src;

        switch(valueType)
        {
        case DDVT_ANGLE:
            angleValues[index] = *s;
            break;
        default:
        {
            QByteArray msg = String("DmuObject::Args::setValue: DDVT_ANGLE incompatible with value type %1.").arg(value_Str(valueType)).toUtf8();
            App_FatalError(msg.constData());
        }
        }
    }
    else if(srcValueType == DDVT_BLENDMODE)
    {
        auto const *s = (blendmode_t const *)src;

        switch(valueType)
        {
        case DDVT_INT:
            intValues[index] = *s;
            break;
        default:
        {
            QByteArray msg = String("DmuObject::Args::setValue: DDVT_BLENDMODE incompatible with value type %1.").arg(value_Str(valueType)).toUtf8();
            App_FatalError(msg.constData());
        }
        }
    }
    else if(srcValueType == DDVT_PTR)
    {
        auto const *const *s = (void const *const *)src;

        switch(valueType)
        {
        case DDVT_INT:
            // Attempt automatic conversion using P_ToIndex(). Naturally only
            // works with map elements. Failure leads into a fatal error.
            intValues[index] = P_ToIndex(*s);
            break;
        case DDVT_PTR:
            ptrValues[index] = (void *)*s;
            break;
        default:
        {
            QByteArray msg = String("DmuObject::Args::setValue: DDVT_PTR incompatible with value type %1.").arg(value_Str(valueType)).toUtf8();
            App_FatalError(msg.constData());
        }
        }
    }
    else
    {
        QByteArray msg = String("DmuObject::Args::setValue: unknown value type %1.").arg(srcValueType).toUtf8();
        App_FatalError(msg.constData());
    }
}

//- DmuObject ---------------------------------------------------------------------------

DENG2_PIMPL_NOREF(DmuObject)
{
    dint type;
    DmuObject *parent = nullptr;

    dint indexInMap = NoIndex;
    dint indexInArchive = NoIndex;

    Instance(dint type) : type(type) {}
};

DmuObject::DmuObject(dint type, dint indexInMap)
    : d(new Instance(type))
{
    setIndexInMap(indexInMap);
}

dint DmuObject::type() const
{
    return d->type;
}

String DmuObject::describe() const
{
    return "abstract DmuObject";
}

String DmuObject::description(dint /*verbosity*/) const
{
    String desc = describe();
    if(indexInMap() != NoIndex)
    {
        desc += String(" #%1").arg(indexInMap());
    }
    return desc;
}

dint DmuObject::indexInMap() const
{
    return d->indexInMap;
}

void DmuObject::setIndexInMap(dint newIndex)
{
    d->indexInMap = newIndex;
}

dint DmuObject::indexInArchive() const
{
    return d->indexInArchive;
}

void DmuObject::setIndexInArchive(dint newIndex)
{
    d->indexInArchive = newIndex;
}

dint DmuObject::property(Args &args) const
{
    switch(args.prop)
    {
    case DMU_ARCHIVE_INDEX:
        args.setValue(DMT_ARCHIVE_INDEX, &d->indexInArchive, 0);
        break;

    default:
        /// @throw UnknownPropertyError  The requested property is not readable.
        throw UnknownPropertyError(QString("%1::property").arg(DMU_Str(d->type)),
                                   QString("'%1' is unknown/not readable").arg(DMU_Str(args.prop)));
    }

    return false; // Continue iteration.
}

dint DmuObject::setProperty(Args const &args)
{
    /// @throw WritePropertyError  The requested property is not writable.
    throw WritePropertyError(QString("%1::setProperty").arg(DMU_Str(d->type)),
                             QString("'%1' is unknown/not writable").arg(DMU_Str(args.prop)));
}

bool DmuObject::hasParent() const
{
    return d->parent != nullptr;
}

DmuObject &DmuObject::parent()
{
    return const_cast<DmuObject &>(const_cast<DmuObject const *>(this)->parent());
}

DmuObject const &DmuObject::parent() const
{
    if(d->parent) return *d->parent;
    /// @throw MissingParentError  Attempted with no parent element is attributed.
    throw MissingParentError("DmuObject::parent", "No parent map element is attributed");
}

void DmuObject::setParent(DmuObject *newParent)
{
    if(newParent == this)
    {
        /// @throw InvalidParentError  Attempted to attribute *this* element as parent of itself.
        throw InvalidParentError("DmuObject::setParent", "Cannot attribute 'this' map element as a parent of itself");
    }
    d->parent = newParent;
}

}  // namespace de
