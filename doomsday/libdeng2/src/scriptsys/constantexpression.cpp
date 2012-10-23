/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2004-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/ConstantExpression"
#include "de/NumberValue"
#include "de/NoneValue"
#include "de/Writer"
#include "de/Reader"
#include "de/math.h"

using namespace de;

ConstantExpression::ConstantExpression() : _value(0)
{}

ConstantExpression::ConstantExpression(Value* value) : _value(value) 
{}

ConstantExpression::~ConstantExpression()
{
    delete _value;
}

Value* ConstantExpression::evaluate(Evaluator&) const
{
    DENG2_ASSERT(_value != 0);
    return _value->duplicate();
}

ConstantExpression* ConstantExpression::None()
{
    return new ConstantExpression(new NoneValue());
}        

ConstantExpression* ConstantExpression::True()
{
    return new ConstantExpression(new NumberValue(NumberValue::VALUE_TRUE));
}

ConstantExpression* ConstantExpression::False()
{
    return new ConstantExpression(new NumberValue(NumberValue::VALUE_FALSE));
}

ConstantExpression* ConstantExpression::Pi()
{
    return new ConstantExpression(new NumberValue(PI));
}

void ConstantExpression::operator >> (Writer& to) const
{
    to << SerialId(CONSTANT);

    Expression::operator >> (to);

    to << *_value;
}

void ConstantExpression::operator << (Reader& from)
{
    SerialId id;
    from >> id;
    if(id != CONSTANT)
    {
        /// @throw DeserializationError The identifier that species the type of the 
        /// serialized expression was invalid.
        throw DeserializationError("ConstantExpression::operator <<", "Invalid ID");
    }

    Expression::operator << (from);

    delete _value;
    _value = 0;
    _value = Value::constructFrom(from);
}
