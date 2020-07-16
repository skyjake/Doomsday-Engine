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

#include "de/scripting/expressionstatement.h"
#include "de/scripting/expression.h"
#include "de/scripting/context.h"
#include "de/scripting/evaluator.h"
#include "de/writer.h"
#include "de/arrayvalue.h"
#include "de/refvalue.h"
#include "de/reader.h"

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
    to << dbyte(SerialId::Expression) << *_expression;
}

void ExpressionStatement::operator << (Reader &from)
{
    SerialId id;
    from.readAs<dbyte>(id);
    if (id != SerialId::Expression)
    {
        /// @throw DeserializationError The identifier that species the type of the
        /// serialized statement was invalid.
        throw DeserializationError("ExpressionStatement::operator <<", "Invalid ID");
    }
    delete _expression;
    _expression = 0;
    _expression = Expression::constructFrom(from);
}
