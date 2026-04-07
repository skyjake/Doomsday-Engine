/** @file propertyvalue.h Data types for representing world map property values.
 *
 * @ingroup data
 *
 * Data type class hierarchy with integral RTTI mechanism and basic in-place
 * value/type conversions.
 *
 * @authors Copyright © 2013 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDOOMSDAY_WORLD_PROPERTYVALUE_H
#define LIBDOOMSDAY_WORLD_PROPERTYVALUE_H

#include "../libdoomsday.h"
#include "valuetype.h"
#include <de/legacy/types.h>
#include <de/legacy/fixedpoint.h>

class LIBDOOMSDAY_PUBLIC PropertyValue
{
public:
    virtual ~PropertyValue() {}

    virtual valuetype_t type() const = 0;
    virtual char const* typeName() const = 0;

    virtual byte    asByte()   const = 0;
    virtual int16_t asInt16()  const = 0;
    virtual int32_t asInt32()  const = 0;
    virtual fixed_t asFixed()  const = 0;
    virtual angle_t asAngle()  const = 0;
    virtual float   asFloat()  const = 0;
    virtual double  asDouble() const = 0;
};

class LIBDOOMSDAY_PUBLIC PropertyByteValue : public PropertyValue
{
public:
    PropertyByteValue(byte value) : PropertyValue(), value_(value) {}

    valuetype_t type() const { return DDVT_BYTE; }
    char const* typeName() const { return "byte"; }

    byte value() const { return value_; }

    byte    asByte()   const { return value_; }
    int16_t asInt16()  const { return value_; }
    int32_t asInt32()  const { return value_; }
    fixed_t asFixed()  const { return value_ << FRACBITS; }
    angle_t asAngle()  const { return value_; }
    float   asFloat()  const { return value_; }
    double  asDouble() const { return value_; }

private:
    byte value_;
};

class LIBDOOMSDAY_PUBLIC PropertyInt16Value : public PropertyValue
{
public:
    PropertyInt16Value(int16_t value) : PropertyValue(), value_(value) {}

    valuetype_t type() const { return DDVT_SHORT; }
    char const* typeName() const { return "int16"; }

    int16_t value() const { return value_; }

    byte    asByte()   const { return byte(value_); }
    int16_t asInt16()  const { return value_; }
    int32_t asInt32()  const { return int32_t(value_); }
    fixed_t asFixed()  const { return fixed_t(value_ << FRACBITS); }
    angle_t asAngle()  const { return angle_t(value_); }
    float   asFloat()  const { return float(value_); }
    double  asDouble() const { return double(value_); }

private:
    int16_t value_;
};

class LIBDOOMSDAY_PUBLIC PropertyInt32Value : public PropertyValue
{
public:
    PropertyInt32Value(int32_t value) : PropertyValue(), value_(value) {}

    valuetype_t type() const { return DDVT_INT; }
    char const* typeName() const { return "int32"; }

    int32_t value() const { return value_; }

    byte    asByte()   const { return byte(value_); }
    int16_t asInt16()  const { return int16_t(value_); }
    int32_t asInt32()  const { return value_; }
    fixed_t asFixed()  const { return fixed_t(value_ << FRACBITS); }
    angle_t asAngle()  const { return angle_t(value_); }
    float   asFloat()  const { return float(value_); }
    double  asDouble() const { return double(value_); }

private:
    int32_t value_;
};

class LIBDOOMSDAY_PUBLIC PropertyFixedValue : public PropertyValue
{
public:
    PropertyFixedValue(fixed_t value) : PropertyValue(), value_(value) {}

    valuetype_t type() const { return DDVT_FIXED; }
    char const* typeName() const { return "fixed"; }

    fixed_t value() const { return value_; }

    byte    asByte()   const { return byte(value_ >> FRACBITS); }
    int16_t asInt16()  const { return int16_t(value_ >> FRACBITS); }
    int32_t asInt32()  const { return int32_t(value_ >> FRACBITS); }
    fixed_t asFixed()  const { return value_; }
    angle_t asAngle()  const { return angle_t(value_ >> FRACBITS); }
    float   asFloat()  const { return FIX2FLT(value_); }
    double  asDouble() const { return FIX2FLT(value_); }

private:
    fixed_t value_;
};

class LIBDOOMSDAY_PUBLIC PropertyAngleValue : public PropertyValue
{
public:
    PropertyAngleValue(angle_t value) : PropertyValue(), value_(value) {}

    valuetype_t type() const { return DDVT_ANGLE; }
    char const* typeName() const { return "angle"; }

    angle_t value() const { return value_; }

    byte    asByte()   const { return byte(value_); }
    int16_t asInt16()  const { return int16_t(value_); }
    int32_t asInt32()  const { return int32_t(value_); }
    fixed_t asFixed()  const { return fixed_t(value_ << FRACBITS); }
    angle_t asAngle()  const { return value_; }
    float   asFloat()  const { return float(value_); }
    double  asDouble() const { return double(value_); }

private:
    angle_t value_;
};

class LIBDOOMSDAY_PUBLIC PropertyFloatValue : public PropertyValue
{
public:
    PropertyFloatValue(float value) : PropertyValue(), value_(value) {}

    valuetype_t type() const { return DDVT_FLOAT; }
    char const* typeName() const { return "float"; }

    float value() const { return value_; }

    byte    asByte()   const { return byte(value_); }
    int16_t asInt16()  const { return int16_t(value_); }
    int32_t asInt32()  const { return int32_t(value_); }
    fixed_t asFixed()  const { return FLT2FIX(value_); }
    angle_t asAngle()  const { return angle_t(value_); }
    float   asFloat()  const { return value_; }
    double  asDouble() const { return value_; }

private:
    float value_;
};

class LIBDOOMSDAY_PUBLIC PropertyDoubleValue : public PropertyValue
{
public:
    PropertyDoubleValue(double value) : PropertyValue(), value_(value) {}

    valuetype_t type() const { return DDVT_DOUBLE; }
    const char *typeName() const { return "double"; }

    double value() const { return value_; }

    byte    asByte()   const { return byte(value_); }
    int16_t asInt16()  const { return int16_t(value_); }
    int32_t asInt32()  const { return int32_t(value_); }
    fixed_t asFixed()  const { return FLT2FIX(value_); }
    angle_t asAngle()  const { return angle_t(value_); }
    float   asFloat()  const { return float(value_); }
    double  asDouble() const { return value_; }

private:
    double value_;
};

/**
 * Factory constructor for instantiation of new PropertyValues.
 *
 * @param type          DDVT_* value type identifier for the value pointed at by @a valueAdr.
 * @param valueAdr      Address of the value to be read into the new property value.
 *
 * @return Newly constructed PropertyValue-derived instance.
 */
LIBDOOMSDAY_PUBLIC PropertyValue *BuildPropertyValue(valuetype_t type, void *valueAdr);

#endif // LIBDOOMSDAY_WORLD_PROPERTYVALUE_H
