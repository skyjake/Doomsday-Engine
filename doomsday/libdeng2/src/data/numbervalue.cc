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

#include <sstream>

using namespace de;

NumberValue::NumberValue(Number initialValue)
    : value_(initialValue) 
{}

Value* NumberValue::duplicate() const
{
    return new NumberValue(value_);
}

Value::Number NumberValue::asNumber() const
{
	return value_;
}

Value::Text NumberValue::asText() const
{
	std::ostringstream str;
	str << value_;
	return str.str();
}

bool NumberValue::isTrue() const
{
    return (value_ != 0);
}

dint NumberValue::compare(const Value& value) const
{
    const NumberValue* other = dynamic_cast<const NumberValue*>(&value);
    if(other)
    {
        if(fequal(value_, other->value_))
        {
            return 0;
        }
        return cmp(value_, other->value_);
    }    
    return Value::compare(value);
}

void NumberValue::negate()
{
    value_ = -value_;
}

void NumberValue::sum(const Value& value)
{
    const NumberValue* other = dynamic_cast<const NumberValue*>(&value);
    if(!other)
    {
        throw ArithmeticError("NumberValue::sum", "Values cannot be summed");
    }
    
    value_ += other->value_;
}

void NumberValue::subtract(const Value& value)
{
    const NumberValue* other = dynamic_cast<const NumberValue*>(&value);
    if(!other)
    {
        throw ArithmeticError("Value::subtract", "Value cannot be subtracted from");    
    }
    
    value_ -= other->value_;
}

void NumberValue::divide(const Value& divisor)
{
    const NumberValue* other = dynamic_cast<const NumberValue*>(&divisor);
    if(!other)
    {
        throw ArithmeticError("NumberValue::divide", "Value cannot be divided");
    }
    
    value_ /= other->value_;
}
 
void NumberValue::multiply(const Value& value)
{
    const NumberValue* other = dynamic_cast<const NumberValue*>(&value);
    if(!other)
    {
        throw ArithmeticError("NumberValue::multiply", "Value cannot be multiplied");
    }
    
    value_ *= other->value_;
}

void NumberValue::modulo(const Value& divisor)
{
    const NumberValue* other = dynamic_cast<const NumberValue*>(&divisor);
    if(!other)
    {
        throw ArithmeticError("Value::modulo", "Modulo not defined");
    }

    // Modulo is done with integers.
    value_ = int(value_) % int(other->value_);
}

void NumberValue::operator >> (Writer& to) const
{
    to << SerialId(NUMBER) << value_;
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
    from >> value_;
}
