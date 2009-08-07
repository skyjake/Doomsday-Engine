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

#include "de/ArrayExpression"
#include "de/Evaluator"
#include "de/Expression"
#include "de/ArrayValue"
#include "de/Writer"
#include "de/Reader"

using namespace de;

ArrayExpression::ArrayExpression()
{}

ArrayExpression::~ArrayExpression()
{}

void ArrayExpression::clear()
{
    for(Arguments::iterator i = arguments_.begin(); i != arguments_.end(); ++i)
    {
        delete *i;
    }
    arguments_.clear();
}

void ArrayExpression::add(Expression* arg)
{
    arguments_.push_back(arg);
}

void ArrayExpression::push(Evaluator& evaluator, Record* names) const
{
    Expression::push(evaluator, names);
    
    // The arguments in reverse order (so they are evaluated in
    // natural order, i.e., the same order they are in the source).
    for(Arguments::const_reverse_iterator i = arguments_.rbegin();
        i != arguments_.rend(); ++i)
    {
        (*i)->push(evaluator);
    }
}

const Expression& ArrayExpression::at(dint pos) const
{
    return *arguments_.at(pos);
}

Value* ArrayExpression::evaluate(Evaluator& evaluator) const
{
    // Collect the right number of results into the array.
    ArrayValue* value = new ArrayValue;
    for(dint count = arguments_.size(); count > 0; --count)
    {
        value->add(evaluator.popResult());
    }
    value->reverse();
    return value;
}

void ArrayExpression::operator >> (Writer& to) const
{
    to << SerialId(ARRAY) << duint16(arguments_.size());
    for(Arguments::const_iterator i = arguments_.begin(); i != arguments_.end(); ++i)
    {
        to << **i;
    }
}

void ArrayExpression::operator << (Reader& from)
{
    SerialId id;
    from >> id;
    if(id != ARRAY)
    {
        /// @throw DeserializationError The identifier that species the type of the 
        /// serialized expression was invalid.
        throw DeserializationError("ArrayExpression::operator <<", "Invalid ID");
    }
    duint16 count;
    from >> count;
    clear();
    while(count--)
    {
        arguments_.push_back(Expression::constructFrom(from));
    }
}
