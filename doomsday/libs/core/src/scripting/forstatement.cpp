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

#include "de/scripting/forstatement.h"
#include "de/scripting/expression.h"
#include "de/scripting/context.h"
#include "de/scripting/evaluator.h"
#include "de/scripting/process.h"
#include "de/value.h"
#include "de/variable.h"
#include "de/refvalue.h"
#include "de/writer.h"
#include "de/reader.h"

using namespace de;

ForStatement::ForStatement() : _iterator(0), _iteration(0)
{}

ForStatement::~ForStatement()
{
    delete _iterator;
    delete _iteration;
}

void ForStatement::execute(Context &context) const
{
    Evaluator &eval = context.evaluator();
    if (!context.iterationValue())
    {
        eval.evaluate(_iteration);
        // We now have the iterated value.
        context.setIterationValue(eval.popResult());
    }

    // The variable gets ownership of this value.
    Value *nextValue = context.iterationValue()->next();
    if (nextValue)
    {
        // Assign the variable specified.
        RefValue &ref = eval.evaluateTo<RefValue>(_iterator);
        ref.assign(nextValue);

        // Let's begin the compound.
        context.start(_compound.firstStatement(), this, this, this);
    }
    else
    {
        context.setIterationValue(nullptr);
        context.proceed();
    }
}

void ForStatement::operator >> (Writer &to) const
{
    to << dbyte(SerialId::For) << *_iterator << *_iteration << _compound;
}

void ForStatement::operator << (Reader &from)
{
    SerialId id;
    from.readAs<dbyte>(id);
    if (id != SerialId::For)
    {
        /// @throw DeserializationError The identifier that species the type of the
        /// serialized statement was invalid.
        throw DeserializationError("ForStatement::operator <<", "Invalid ID");
    }
    delete _iterator;
    delete _iteration;
    _iterator = 0;
    _iteration = 0;

    _iterator = Expression::constructFrom(from);
    _iteration = Expression::constructFrom(from);

    from >> _compound;
}
