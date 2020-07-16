/** @file scopestatement.cpp  Scope statement.
 *
 * @authors Copyright (c) 2014-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/scripting/scopestatement.h"
#include "de/scripting/evaluator.h"
#include "de/scripting/context.h"
#include "de/recordvalue.h"
#include "de/arrayvalue.h"
#include "de/scripting/process.h"
#include "de/reader.h"
#include "de/writer.h"

namespace de {

DE_PIMPL_NOREF(ScopeStatement)
{
    std::unique_ptr<Expression> identifier;
    std::unique_ptr<Expression> superRecords;
    Compound compound;
};

ScopeStatement::ScopeStatement() : d(new Impl)
{}

ScopeStatement::ScopeStatement(Expression *identifier, Expression *superRecords)
    : d(new Impl)
{
    d->identifier.reset(identifier);
    d->superRecords.reset(superRecords);
}

Compound &ScopeStatement::compound()
{
    return d->compound;
}

void ScopeStatement::execute(Context &context) const
{
    Evaluator &eval = context.evaluator();

    // Get the identified class record.
    Record &classRecord = eval.evaluateTo<RecordValue>(d->identifier.get()).dereference();

    // Possible super records.
    eval.evaluate(d->superRecords.get());
    std::unique_ptr<ArrayValue> newSupers(eval.popResultAs<ArrayValue>());
    while (newSupers->size() > 0)
    {
        classRecord.addSuperRecord(newSupers->popFirst());
    }

    // This context continues past the compound.
    context.proceed();

    // Continue executing in the specified scope.
    Context *scope = new Context(Context::Namespace, &context.process(), &classRecord);
    scope->start(d->compound.firstStatement());
    context.process().pushContext(scope);
}

void ScopeStatement::operator >> (Writer &to) const
{
    to << dbyte(SerialId::Scope) << *d->identifier << *d->superRecords << d->compound;
}

void ScopeStatement::operator << (Reader &from)
{
    SerialId id;
    from.readAs<dbyte>(id);
    if (id != SerialId::Scope)
    {
        /// @throw DeserializationError The identifier that species the type of the
        /// serialized statement was invalid.
        throw DeserializationError("ScopeStatement::operator <<", "Invalid ID");
    }
    d->identifier.reset(Expression::constructFrom(from));
    d->superRecords.reset(Expression::constructFrom(from));
    from >> d->compound;
}

} // namespace de
