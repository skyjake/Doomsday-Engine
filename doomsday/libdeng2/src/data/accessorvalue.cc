/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2009, 2011 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 
#include "de/AccessorValue"

using namespace de;

const Variable::Flags AccessorValue::VARIABLE_MODE = Variable::AllowText |
    Variable::ReadOnly | Variable::NoSerialize;

AccessorValue::AccessorValue()
{}

Value* AccessorValue::duplicate() const
{
    update();
    return duplicateContent();
}

Value::Number AccessorValue::asNumber() const
{
    update();
    return TextValue::asNumber();
}

Value::Text AccessorValue::asText() const
{
    update();
    return TextValue::asText();
}

dsize AccessorValue::size() const
{
    update();
    return TextValue::size();
}

bool AccessorValue::isTrue() const
{
    update();
    return TextValue::isTrue();
}

dint AccessorValue::compare(const Value& value) const
{
    update();
    return TextValue::compare(value);
}

void AccessorValue::sum(const Value& /*value*/)
{
    /// @throw ArithmeticError  Attempted to modify the value of the accessor.
    throw ArithmeticError("AccessorValue::sum", "Accessor values cannot be modified");
}

void AccessorValue::multiply(const Value& /*value*/)
{
    /// @throw ArithmeticError  Attempted to modify the value of the accessor.
    throw ArithmeticError("AccessorValue::multiply", "Accessor values cannot be modified");
}

void AccessorValue::divide(const Value& /*value*/)
{
    /// @throw ArithmeticError  Attempted to modify the value of the accessor.
    throw ArithmeticError("AccessorValue::divide", "Accessor values cannot be modified");
}

void AccessorValue::modulo(const Value& /*divisor*/)
{
    /// @throw ArithmeticError  Attempted to modify the value of the accessor.
    throw ArithmeticError("AccessorValue::modulo", "Accessor values cannot be modified");
}

void AccessorValue::operator >> (Writer& /*to*/) const
{
    /// @throw CannotSerializeError  Attempted to serialize the accessor.
    throw CannotSerializeError("AccessorValue::operator >>", "Accessor cannot be serialized");
}

void AccessorValue::operator << (Reader& /*from*/)
{
    /// @throw CannotSerializeError  Attempted to deserialize the accessor.
    throw CannotSerializeError("AccessorValue::operator <<", "Accessor cannot be deserialized");
}
