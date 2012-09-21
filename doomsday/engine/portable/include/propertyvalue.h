/**
 * @file propertyvalue.h
 * Data types for representing property values. @ingroup data
 *
 * Data type class hierarchy with integral RTTI mechanism and basic in-place
 * value/type conversions.
 *
 * @author Copyright &copy; 2012 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG_DATA_PROPERTYVALUE_H
#define LIBDENG_DATA_PROPERTYVALUE_H

#include "de_base.h"

class PropertyValue
{
public:
    virtual ~PropertyValue() {}

    virtual valuetype_t type() const = 0;
    virtual char const* typeName() const = 0;

    virtual byte    asByte()  const = 0;
    virtual int16_t asInt16() const = 0;
    virtual int32_t asInt32() const = 0;
    virtual fixed_t asFixed() const = 0;
    virtual angle_t asAngle() const = 0;
    virtual float   asFloat() const = 0;
};

class PropertyByteValue : public PropertyValue
{
public:
    PropertyByteValue(byte value) : PropertyValue(), value_(value) {}

    valuetype_t type() const { return DDVT_BYTE; }
    char const* typeName() const { return "byte"; }

    byte value() const { return value_; }

    byte    asByte()  const { return value_; }
    int16_t asInt16() const { return value_; }
    int32_t asInt32() const { return value_; }
    fixed_t asFixed() const { return value_ << FRACBITS; }
    angle_t asAngle() const { return value_; }
    float   asFloat() const { return value_; }

private:
    byte value_;
};

class PropertyInt16Value : public PropertyValue
{
public:
    PropertyInt16Value(int16_t value) : PropertyValue(), value_(value) {}

    valuetype_t type() const { return DDVT_SHORT; }
    char const* typeName() const { return "int16"; }

    int16_t value() const { return value_; }

    byte    asByte()  const { return value_; }
    int16_t asInt16() const { return value_; }
    int32_t asInt32() const { return value_; }
    fixed_t asFixed() const { return value_ << FRACBITS; }
    angle_t asAngle() const { return value_; }
    float   asFloat() const { return value_; }

private:
    int16_t value_;
};

class PropertyInt32Value : public PropertyValue
{
public:
    PropertyInt32Value(int32_t value) : PropertyValue(), value_(value) {}

    valuetype_t type() const { return DDVT_INT; }
    char const* typeName() const { return "int32"; }

    int32_t value() const { return value_; }

    byte    asByte()  const { return value_; }
    int16_t asInt16() const { return value_; }
    int32_t asInt32() const { return value_; }
    fixed_t asFixed() const { return value_ << FRACBITS; }
    angle_t asAngle() const { return value_; }
    float   asFloat() const { return value_; }

private:
    int32_t value_;
};

class PropertyFixedValue : public PropertyValue
{
public:
    PropertyFixedValue(fixed_t value) : PropertyValue(), value_(value) {}

    valuetype_t type() const { return DDVT_FIXED; }
    char const* typeName() const { return "fixed"; }

    fixed_t value() const { return value_; }

    byte    asByte()  const { return value_ >> FRACBITS; }
    int16_t asInt16() const { return value_ >> FRACBITS; }
    int32_t asInt32() const { return value_ >> FRACBITS; }
    fixed_t asFixed() const { return value_; }
    angle_t asAngle() const { return value_ >> FRACBITS; }
    float   asFloat() const { return FIX2FLT(value_); }

private:
    fixed_t value_;
};

class PropertyAngleValue : public PropertyValue
{
public:
    PropertyAngleValue(angle_t value) : PropertyValue(), value_(value) {}

    valuetype_t type() const { return DDVT_ANGLE; }
    char const* typeName() const { return "angle"; }

    angle_t value() const { return value_; }

    byte    asByte()  const { return value_; }
    int16_t asInt16() const { return value_; }
    int32_t asInt32() const { return value_; }
    fixed_t asFixed() const { return value_ << FRACBITS; }
    angle_t asAngle() const { return value_; }
    float   asFloat() const { return value_; }

private:
    angle_t value_;
};

class PropertyFloatValue : public PropertyValue
{
public:
    PropertyFloatValue(float value) : PropertyValue(), value_(value) {}

    valuetype_t type() const { return DDVT_FLOAT; }
    char const* typeName() const { return "float"; }

    float value() const { return value_; }

    byte    asByte()  const { return value_; }
    int16_t asInt16() const { return value_; }
    int32_t asInt32() const { return value_; }
    fixed_t asFixed() const { return FLT2FIX(value_); }
    angle_t asAngle() const { return value_; }
    float   asFloat() const { return value_; }

private:
    float value_;
};

/**
 * Factory constructor for instantiation of new PropertyValues.
 *
 * @param type          DDVT_* value type identifier for the value pointed at by @a valueAdr.
 * @param valueAdr      Address of the value to be read into the the new property value.
 *
 * @return Newly constructed PropertyValue-derived instance.
 */
PropertyValue* BuildPropertyValue(valuetype_t type, void* valueAdr);

#endif /// LIBDENG_DATA_PROPERTYVALUE_H
