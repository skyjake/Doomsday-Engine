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

#include "de/scripting/whilestatement.h"
#include "de/scripting/expression.h"
#include "de/scripting/evaluator.h"
#include "de/scripting/context.h"
#include "de/value.h"
#include "de/writer.h"
#include "de/reader.h"

using namespace de;

WhileStatement::~WhileStatement()
{
    delete _loopCondition;
}

void WhileStatement::execute(Context &context) const
{
    Evaluator &eval = context.evaluator();

    if (eval.evaluate(_loopCondition).isTrue())
    {
        // Continue and break jump points are defined within a while compound.
        context.start(_compound.firstStatement(), this, this, this);
    }
    else
    {
        context.proceed();
    }
}

void WhileStatement::operator >> (Writer &to) const
{
    to << dbyte(SerialId::While) << *_loopCondition << _compound;
}

void WhileStatement::operator << (Reader &from)
{
    SerialId id;
    from.readAs<dbyte>(id);
    if (id != SerialId::While)
    {
        /// @throw DeserializationError The identifier that species the type of the
        /// serialized statement was invalid.
        throw DeserializationError("WhileStatement::operator <<", "Invalid ID");
    }
    delete _loopCondition;
    _loopCondition = 0;
    _loopCondition = Expression::constructFrom(from);

    from >> _compound;
}
