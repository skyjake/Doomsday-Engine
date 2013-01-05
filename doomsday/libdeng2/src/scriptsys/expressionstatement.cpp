/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2004-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/ExpressionStatement"
#include "de/Expression"
#include "de/Context"
#include "de/Evaluator"
#include "de/Writer"
#include "de/ArrayValue"
#include "de/RefValue"
#include "de/Reader"

using namespace de;

ExpressionStatement::~ExpressionStatement()
{
    delete _expression;
}

void ExpressionStatement::execute(Context &context) const
{
    context.evaluator().evaluate(_expression);
    // ...the result will not be used for anything.

    context.proceed();
}

void ExpressionStatement::operator >> (Writer &to) const
{
    to << SerialId(EXPRESSION) << *_expression;
}

void ExpressionStatement::operator << (Reader &from)
{
    SerialId id;
    from >> id;
    if(id != EXPRESSION)
    {
        /// @throw DeserializationError The identifier that species the type of the 
        /// serialized statement was invalid.
        throw DeserializationError("ExpressionStatement::operator <<", "Invalid ID");
    }
    delete _expression;
    _expression = 0;
    _expression = Expression::constructFrom(from);
}
