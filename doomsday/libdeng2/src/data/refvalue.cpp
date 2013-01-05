/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2009-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

RefValue::RefValue(Variable *variable) : _variable(variable)
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
        throw NullError("RefValue::verify", "Value does not reference a variable");
    }
}

Value &RefValue::dereference()
{
    verify();
    return _variable->value();
}

Value const &RefValue::dereference() const
{
    verify();
    return _variable->value();
}

Value *RefValue::duplicate() const
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

Value const &RefValue::element(Value const &index) const
{
    return dereference().element(index);
}

Value &RefValue::element(Value const &index)
{
    return dereference().element(index);
}

void RefValue::setElement(Value const &index, Value *elementValue)
{
    dereference().setElement(index, elementValue);
}

bool RefValue::contains(Value const &value) const
{
    return dereference().contains(value);
}

Value *RefValue::begin()
{
    return dereference().begin();
}

Value *RefValue::next()
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

dint RefValue::compare(Value const &value) const
{
    return dereference().compare(value);
}

void RefValue::negate()
{
    dereference().negate();
}

void RefValue::sum(Value const &value)
{
    dereference().sum(value);
}

void RefValue::subtract(Value const &subtrahend)
{
    dereference().subtract(subtrahend);
}

void RefValue::divide(Value const &divisor)
{
    dereference().divide(divisor);
}

void RefValue::multiply(Value const &value)
{
    dereference().multiply(value);
}

void RefValue::modulo(Value const &divisor)
{
    dereference().modulo(divisor);
}

void RefValue::assign(Value *value)
{
    verify();
    _variable->set(value);
}

void RefValue::call(Process &process, Value const &arguments) const
{
    dereference().call(process, arguments);
}

void RefValue::operator >> (Writer &to) const
{
    to << dereference();
}

void RefValue::operator << (Reader &/*from*/)
{
    // Should never happen.
    DENG2_ASSERT(false);
}

void RefValue::variableBeingDeleted(Variable &DENG2_DEBUG_ONLY(variable))
{
    DENG2_ASSERT(_variable == &variable);
    _variable = 0;
}
