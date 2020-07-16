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

#include "de/scripting/dictionaryexpression.h"
#include "de/scripting/evaluator.h"
#include "de/dictionaryvalue.h"
#include "de/writer.h"
#include "de/reader.h"

#include <list>

using namespace de;

DictionaryExpression::DictionaryExpression()
{}

DictionaryExpression::~DictionaryExpression()
{
    clear();
}

void DictionaryExpression::clear()
{
    for (Arguments::iterator i = _arguments.begin(); i != _arguments.end(); ++i)
    {
        delete i->first;
        delete i->second;
    }
    _arguments.clear();
}

void DictionaryExpression::add(Expression *key, Expression *value)
{
    DE_ASSERT(key != nullptr);
    DE_ASSERT(value != nullptr);
    _arguments.push_back(ExpressionPair(key, value));
}

void DictionaryExpression::push(Evaluator &evaluator, Value *scope) const
{
    Expression::push(evaluator, scope);

    // The arguments in reverse order (so they are evaluated in
    // natural order, i.e., the same order they are in the source).
    for (Arguments::const_reverse_iterator i = _arguments.rbegin();
        i != _arguments.rend(); ++i)
    {
        i->second->push(evaluator);
        i->first->push(evaluator);
    }
}

Value *DictionaryExpression::evaluate(Evaluator &evaluator) const
{
    std::unique_ptr<DictionaryValue> dict(new DictionaryValue);

    std::list<Value *> keys, values;

    // Collect the right number of results into the array.
    for (Arguments::const_reverse_iterator i = _arguments.rbegin();
        i != _arguments.rend(); ++i)
    {
        values.push_back(evaluator.popResult());
        keys.push_back(evaluator.popResult());
    }

    // Insert the keys and values into the dictionary in the correct
    // order, i.e., the same order as they are in the source.
    std::list<Value *>::reverse_iterator key = keys.rbegin();
    std::list<Value *>::reverse_iterator value = values.rbegin();
    for (; key != keys.rend(); ++key, ++value)
    {
        dict->add(*key, *value);
    }

    return dict.release();
}

void DictionaryExpression::operator >> (Writer &to) const
{
    to << SerialId(DICTIONARY);

    Expression::operator >> (to);

    to << duint16(_arguments.size());
    for (Arguments::const_iterator i = _arguments.begin(); i != _arguments.end(); ++i)
    {
        to << *i->first << *i->second;
    }
}

void DictionaryExpression::operator << (Reader &from)
{
    SerialId id;
    from >> id;
    if (id != DICTIONARY)
    {
        /// @throw DeserializationError The identifier that species the type of the
        /// serialized expression was invalid.
        throw DeserializationError("DictionaryExpression::operator <<", "Invalid ID");
    }

    Expression::operator << (from);

    duint16 count;
    from >> count;
    clear();
    while (count--)
    {
        std::unique_ptr<Expression> key(Expression::constructFrom(from));
        std::unique_ptr<Expression> value(Expression::constructFrom(from));
        _arguments.push_back(ExpressionPair(key.release(), value.release()));
    }
}
