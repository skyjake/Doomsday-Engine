/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright © 2009-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * LGPL: http://www.gnu.org/licenses/lgpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser
 * General Public License for more details. You should have received a copy of
 * the GNU Lesser General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small> 
 */
 
#include "de/AccessorValue"

using namespace de;

Variable::Flags const AccessorValue::VARIABLE_MODE = Variable::AllowText |
    Variable::ReadOnly | Variable::NoSerialize;

AccessorValue::AccessorValue()
{}

Value *AccessorValue::duplicate() const
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

dint AccessorValue::compare(Value const &value) const
{
    update();
    return TextValue::compare(value);
}

void AccessorValue::sum(Value const &/*value*/)
{
    /// @throw ArithmeticError  Attempted to modify the value of the accessor.
    throw ArithmeticError("AccessorValue::sum", "Accessor values cannot be modified");
}

void AccessorValue::multiply(Value const &/*value*/)
{
    /// @throw ArithmeticError  Attempted to modify the value of the accessor.
    throw ArithmeticError("AccessorValue::multiply", "Accessor values cannot be modified");
}

void AccessorValue::divide(Value const &/*value*/)
{
    /// @throw ArithmeticError  Attempted to modify the value of the accessor.
    throw ArithmeticError("AccessorValue::divide", "Accessor values cannot be modified");
}

void AccessorValue::modulo(Value const &/*divisor*/)
{
    /// @throw ArithmeticError  Attempted to modify the value of the accessor.
    throw ArithmeticError("AccessorValue::modulo", "Accessor values cannot be modified");
}

void AccessorValue::operator >> (Writer &/*to*/) const
{
    /// @throw CannotSerializeError  Attempted to serialize the accessor.
    throw CannotSerializeError("AccessorValue::operator >>", "Accessor cannot be serialized");
}

void AccessorValue::operator << (Reader &/*from*/)
{
    /// @throw CannotSerializeError  Attempted to deserialize the accessor.
    throw CannotSerializeError("AccessorValue::operator <<", "Accessor cannot be deserialized");
}
