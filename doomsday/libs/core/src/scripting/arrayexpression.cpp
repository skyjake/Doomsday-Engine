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

#include "de/scripting/arrayexpression.h"
#include "de/scripting/evaluator.h"
#include "de/scripting/expression.h"
#include "de/arrayvalue.h"
#include "de/writer.h"
#include "de/reader.h"

using namespace de;

ArrayExpression::ArrayExpression()
{}

ArrayExpression::~ArrayExpression()
{
    clear();
}

void ArrayExpression::clear()
{
    for (Arguments::iterator i = _arguments.begin(); i != _arguments.end(); ++i)
    {
        delete *i;
    }
    _arguments.clear();
}

void ArrayExpression::add(Expression *arg)
{
    _arguments.push_back(arg);
}

void ArrayExpression::push(Evaluator &evaluator, Value *scope) const
{
    Expression::push(evaluator, scope);
    
    // The arguments in reverse order (so they are evaluated in
    // natural order, i.e., the same order they are in the source).
    for (Arguments::const_reverse_iterator i = _arguments.rbegin();
        i != _arguments.rend(); ++i)
    {
        (*i)->push(evaluator);
    }
}

const Expression &ArrayExpression::at(dint pos) const
{
    return *_arguments.at(pos);
}

Value *ArrayExpression::evaluate(Evaluator &evaluator) const
{
    // Collect the right number of results into the array.
    ArrayValue *value = new ArrayValue;
    for (dsize count = _arguments.size(); count > 0; --count)
    {
        value->add(evaluator.popResult());
    }
    value->reverse();
    return value;
}

void ArrayExpression::operator >> (Writer &to) const
{
    to << SerialId(ARRAY);

    Expression::operator >> (to);

    to << duint16(_arguments.size());
    for (Arguments::const_iterator i = _arguments.begin(); i != _arguments.end(); ++i)
    {
        to << **i;
    }
}

void ArrayExpression::operator << (Reader &from)
{
    SerialId id;
    from >> id;
    if (id != ARRAY)
    {
        /// @throw DeserializationError The identifier that species the type of the 
        /// serialized expression was invalid.
        throw DeserializationError("ArrayExpression::operator <<", "Invalid ID");
    }

    Expression::operator << (from);

    duint16 count;
    from >> count;
    clear();
    while (count--)
    {
        _arguments.push_back(Expression::constructFrom(from));
    }
}
