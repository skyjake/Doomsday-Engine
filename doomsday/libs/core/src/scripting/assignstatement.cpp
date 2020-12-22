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

#include "de/scripting/assignstatement.h"
#include "de/scripting/context.h"
#include "de/scripting/expression.h"
#include "de/arrayvalue.h"
#include "de/refvalue.h"
#include "de/writer.h"
#include "de/reader.h"

using namespace de;

AssignStatement::AssignStatement() : _indexCount(0)
{}

AssignStatement::AssignStatement(Expression *target, const Indices &indices, Expression *value)
    : _indexCount(0)
{
    _args.add(value);
    _indexCount = dint(indices.size());
    for (Indices::const_reverse_iterator i = indices.rbegin(); i != indices.rend(); ++i)
    {
        _args.add(*i);
    }
    _args.add(target);
}

AssignStatement::~AssignStatement()
{}

void AssignStatement::execute(Context &context) const
{
    Evaluator &eval = context.evaluator();
    ArrayValue &results = eval.evaluateTo<ArrayValue>(&_args);

    // We want to pop the value to assign as the first element.
    results.reverse();

    RefValue *ref = dynamic_cast<RefValue *>(results.elements().front());
    if (!ref)
    {
        throw LeftValueError("AssignStatement::execute",
            "Cannot assign into '" + results.at(0).asText() + "'");
    }

    // The new value that will be assigned to the destination.
    // Ownership of this instance will be given to the variable.
    std::unique_ptr<Value> value(results.popLast());

    if (_indexCount > 0)
    {
        // The value we will be modifying.
        Value *target = &ref->dereference();

        for (dint i = 0; i < _indexCount; ++i)
        {
            // Get the evaluated index.
            std::unique_ptr<Value> index(results.popLast()); // Not released -- will be destroyed.
            if (i < _indexCount - 1)
            {
                // Switch targets to a subelement.
                target = &target->element(*index);
            }
            else
            {
                // The setting is done with final value.
                target->setElement(*index, value.release());
            }
        }
    }
    else
    {
        // Extract the value from the results array (no copies).
        ref->assign(value.release());
    }

    // Should we set the variable to read-only mode?
    if (_args.back().flags() & Expression::ReadOnly)
    {
        DE_ASSERT(ref->variable() != NULL);
        ref->variable()->setFlags(Variable::ReadOnly, SetFlags);
    }

    context.proceed();
}

void AssignStatement::operator >> (Writer &to) const
{
    to << dbyte(SerialId::Assign) << duint8(_indexCount) << _args;
}

void AssignStatement::operator << (Reader &from)
{
    SerialId id;
    from.readAs<dbyte>(id);
    if (id != SerialId::Assign)
    {
        /// @throw DeserializationError The identifier that species the type of the
        /// serialized statement was invalid.
        throw DeserializationError("AssignStatement::operator <<", "Invalid ID");
    }
    // Number of indices in assignment.
    duint8 count;
    from >> count;
    _indexCount = count;

    from >> _args;
}
