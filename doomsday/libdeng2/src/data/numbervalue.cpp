/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2004-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "de/NumberValue"
#include "de/Writer"
#include "de/Reader"
#include "de/math.h"

#include <QTextStream>

using namespace de;

NumberValue::NumberValue(Number initialValue)
    : _value(initialValue) 
{}

Value* NumberValue::duplicate() const
{
    return new NumberValue(_value);
}

Value::Number NumberValue::asNumber() const
{
    return _value;
}

Value::Text NumberValue::asText() const
{
    Text result;
    QTextStream s(&result);
    s << _value;
    return result;
}

bool NumberValue::isTrue() const
{
    return (_value != 0);
}

dint NumberValue::compare(const Value& value) const
{
    const NumberValue* other = dynamic_cast<const NumberValue*>(&value);
    if(other)
    {
        if(fequal(_value, other->_value))
        {
            return 0;
        }
        return cmp(_value, other->_value);
    }    
    return Value::compare(value);
}

void NumberValue::negate()
{
    _value = -_value;
}

void NumberValue::sum(const Value& value)
{
    const NumberValue* other = dynamic_cast<const NumberValue*>(&value);
    if(!other)
    {
        throw ArithmeticError("NumberValue::sum", "Values cannot be summed");
    }
    
    _value += other->_value;
}

void NumberValue::subtract(const Value& value)
{
    const NumberValue* other = dynamic_cast<const NumberValue*>(&value);
    if(!other)
    {
        throw ArithmeticError("Value::subtract", "Value cannot be subtracted from");    
    }
    
    _value -= other->_value;
}

void NumberValue::divide(const Value& divisor)
{
    const NumberValue* other = dynamic_cast<const NumberValue*>(&divisor);
    if(!other)
    {
        throw ArithmeticError("NumberValue::divide", "Value cannot be divided");
    }
    
    _value /= other->_value;
}
 
void NumberValue::multiply(const Value& value)
{
    const NumberValue* other = dynamic_cast<const NumberValue*>(&value);
    if(!other)
    {
        throw ArithmeticError("NumberValue::multiply", "Value cannot be multiplied");
    }
    
    _value *= other->_value;
}

void NumberValue::modulo(const Value& divisor)
{
    const NumberValue* other = dynamic_cast<const NumberValue*>(&divisor);
    if(!other)
    {
        throw ArithmeticError("Value::modulo", "Modulo not defined");
    }

    // Modulo is done with integers.
    _value = int(_value) % int(other->_value);
}

void NumberValue::operator >> (Writer& to) const
{
    to << SerialId(NUMBER) << _value;
}

void NumberValue::operator << (Reader& from)
{
    SerialId id;
    from >> id;
    if(id != NUMBER)
    {
        /// @throw DeserializationError The identifier that species the type of the 
        /// serialized value was invalid.
        throw DeserializationError("NumberValue::operator <<", "Invalid ID");
    }
    from >> _value;
}
