/*
 * The Doomsday Engine Project -- Hawthorn
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

#include "dconstantexpression.hh"
#include "dnumbervalue.hh"
#include "dobjectvalue.hh"

using namespace de;

ConstantExpression::ConstantExpression(Value* value) : value_(value) 
{}

ConstantExpression::~ConstantExpression()
{
    delete value_;
}

Value* ConstantExpression::evaluate(Evaluator& /*evaluator*/) const
{
    assert(value_ != 0);
    return value_->duplicate();
}

ConstantExpression* ConstantExpression::None()
{
    return new ConstantExpression(new ObjectValue());
}        

ConstantExpression* ConstantExpression::True()
{
    return new ConstantExpression(new NumberValue(NumberValue::TRUE));
}

ConstantExpression* ConstantExpression::False()
{
    return new ConstantExpression(new NumberValue(NumberValue::FALSE));
}
