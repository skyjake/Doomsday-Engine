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

#include "ddictionaryexpression.hh"
#include "ddictionaryvalue.hh"
#include "devaluator.hh"

#include <memory>
#include <list>

using namespace de;
using std::auto_ptr;

DictionaryExpression::DictionaryExpression()
{}

DictionaryExpression::~DictionaryExpression()
{
    for(Arguments::iterator i = arguments_.begin(); i != arguments_.end(); ++i)
    {
        delete i->first;
        delete i->second;
    }
}

void DictionaryExpression::add(Expression* key, Expression* value)
{
    assert(key != NULL);
    assert(value != NULL);
    arguments_.push_back(ExpressionPair(key, value));
}

void DictionaryExpression::push(Evaluator& evaluator, Object* names) const
{
	Expression::push(evaluator, names);
	
	// The arguments in reverse order (so they are evaluated in
	// natural order, i.e., the same order they are in the source).
	for(Arguments::const_reverse_iterator i = arguments_.rbegin();
		i != arguments_.rend(); ++i)
	{
        i->second->push(evaluator);
        i->first->push(evaluator);
	}    
}

Value* DictionaryExpression::evaluate(Evaluator& evaluator) const
{
    auto_ptr<DictionaryValue> dict(new DictionaryValue);
    
    std::list<Value*> keys, values;
    
    // Collect the right number of results into the array.
	for(Arguments::const_reverse_iterator i = arguments_.rbegin();
		i != arguments_.rend(); ++i)
    {
        values.push_back(evaluator.popResult());
        keys.push_back(evaluator.popResult());
    }

    // Insert the keys and values into the dictionary in the correct
    // order, i.e., the same order as they are in the source.
    std::list<Value*>::reverse_iterator key = keys.rbegin();
    std::list<Value*>::reverse_iterator value = values.rbegin();
    for(; key != keys.rend(); ++key, ++value)
    {
        dict->add(*key, *value);
    }
    
    return dict.release();
}
