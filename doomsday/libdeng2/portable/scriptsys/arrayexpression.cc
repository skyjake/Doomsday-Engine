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

#include "darrayexpression.hh"
#include "devaluator.hh"
#include "dexpression.hh"
#include "darrayvalue.hh"

using namespace de;

ArrayExpression::ArrayExpression()
{}

ArrayExpression::~ArrayExpression()
{
    for(Arguments::iterator i = arguments_.begin(); i != arguments_.end(); ++i)
    {
        delete *i;
    }
}

void ArrayExpression::add(Expression* arg)
{
    arguments_.push_back(arg);
}

void ArrayExpression::push(Evaluator& evaluator, Object* names) const
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

Value* ArrayExpression::evaluate(Evaluator& evaluator) const
{
    ArrayValue* value = new ArrayValue;
    
    // Collect the right number of results into the array.
    for(int count = arguments_.size(); count > 0; --count)
    {
        value->add(evaluator.popResult());
    }
    
    value->reverse();
    
    return value;
}
