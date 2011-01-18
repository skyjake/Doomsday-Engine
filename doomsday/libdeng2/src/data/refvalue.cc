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

#include "de/RefValue"
#include "de/Variable"
#include "de/Writer"

using namespace de;

RefValue::RefValue(Variable* variable) : _variable(variable)
{
    if(_variable)
    {
        _variable->audienceForDeletion.add(this);
    }
}

RefValue::~RefValue()
{
    if(_variable)
    {
        _variable->audienceForDeletion.remove(this);
    }
}

void RefValue::verify() const
{
    if(!_variable)
    {
        /// @throw NullError The value no longer points to a variable.
        throw NullError("RefValue::verify", "Value no longer references a variable");
    }
}

Value& RefValue::dereference()
{
    verify();
    return _variable->value();
}

const Value& RefValue::dereference() const
{
    verify();
    return _variable->value();
}

Value* RefValue::duplicate() const
{
    return new RefValue(_variable);
}

Value::Number RefValue::asNumber() const
{
    return dereference().asNumber();
}

Value::Text RefValue::asText() const
{
    return dereference().asText();
}

dsize RefValue::size() const
{
    return dereference().size();
}

const Value& RefValue::element(const Value& index) const
{
    return dereference().element(index);
}

Value& RefValue::element(const Value& index)
{
    return dereference().element(index);
}

void RefValue::setElement(const Value& index, Value* elementValue)
{
    dereference().setElement(index, elementValue);
}

bool RefValue::contains(const Value& value) const
{
    return dereference().contains(value);
}

Value* RefValue::begin()
{
    return dereference().begin();
}

Value* RefValue::next()
{
    return dereference().next();
}

bool RefValue::isTrue() const
{
    return dereference().isTrue();
}

bool RefValue::isFalse() const
{
    return dereference().isFalse();
}

dint RefValue::compare(const Value& value) const
{
    return dereference().compare(value);
}

void RefValue::negate()
{
    dereference().negate();
}

void RefValue::sum(const Value& value)
{
    dereference().sum(value);
}

void RefValue::subtract(const Value& subtrahend)
{
    dereference().subtract(subtrahend);
}

void RefValue::divide(const Value& divisor)
{
    dereference().divide(divisor);
}

void RefValue::multiply(const Value& value)
{
    dereference().multiply(value);
}

void RefValue::modulo(const Value& divisor)
{
    dereference().modulo(divisor);
}

void RefValue::assign(Value* value)
{
    verify();
    _variable->set(value);
}

void RefValue::call(Process& process, const Value& arguments) const
{
    dereference().call(process, arguments);
}

void RefValue::operator >> (Writer& to) const
{
    to << dereference();
}

void RefValue::operator << (Reader& /*from*/)
{
    // Should never happen.
    Q_ASSERT(false);
}

void RefValue::variableBeingDeleted(Variable& variable)
{
    Q_ASSERT(_variable == &variable);
    _variable = 0;
}
