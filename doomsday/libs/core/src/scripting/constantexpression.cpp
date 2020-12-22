/*
 * The Doomsday Engine Project -- libcore
 *
 * Copyright © 2004-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/scripting/constantexpression.h"
#include "de/numbervalue.h"
#include "de/nonevalue.h"
#include "de/writer.h"
#include "de/reader.h"
#include "de/math.h"

using namespace de;

ConstantExpression::ConstantExpression() : _value(0)
{}

ConstantExpression::ConstantExpression(Value *value) : _value(value)
{}

ConstantExpression::~ConstantExpression()
{
    delete _value;
}

Value *ConstantExpression::evaluate(Evaluator &) const
{
    DE_ASSERT(_value != 0);
    return _value->duplicate();
}

ConstantExpression *ConstantExpression::None()
{
    return new ConstantExpression(new NoneValue());
}        

ConstantExpression *ConstantExpression::True()
{
    return new ConstantExpression(new NumberValue(NumberValue::True, NumberValue::Boolean));
}

ConstantExpression *ConstantExpression::False()
{
    return new ConstantExpression(new NumberValue(NumberValue::False, NumberValue::Boolean));
}

ConstantExpression *ConstantExpression::Pi()
{
    return new ConstantExpression(new NumberValue(PI));
}

void ConstantExpression::operator >> (Writer &to) const
{
    to << SerialId(CONSTANT);

    Expression::operator >> (to);

    to << *_value;
}

void ConstantExpression::operator << (Reader &from)
{
    SerialId id;
    from >> id;
    if (id != CONSTANT)
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
