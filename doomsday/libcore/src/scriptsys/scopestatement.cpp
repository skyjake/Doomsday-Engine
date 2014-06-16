/** @file scopestatement.cpp  Scope statement.
 *
 * @authors Copyright (c) 2014 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/ScopeStatement"
#include "de/Evaluator"
#include "de/Context"
#include "de/RecordValue"
#include "de/ArrayValue"
#include "de/Process"
#include "de/Reader"
#include "de/Writer"

namespace de {

String const ScopeStatement::SUPER_NAME = "__super__";

DENG2_PIMPL_NOREF(ScopeStatement)
{
    QScopedPointer<Expression> identifier;
    QScopedPointer<Expression> superRecords;
    Compound compound;
};

ScopeStatement::ScopeStatement() : d(new Instance)
{}

ScopeStatement::ScopeStatement(Expression *identifier, Expression *superRecords)
    : d(new Instance)
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
    Record &classRecord = eval.evaluateTo<RecordValue>(d->identifier.data()).dereference();

    // Possible super records.
    ArrayValue &newSupers = eval.evaluateTo<ArrayValue>(d->superRecords.data());
    if(newSupers.size())
    {
        if(!classRecord.has(SUPER_NAME))
        {
            classRecord.addArray(SUPER_NAME);
        }
        ArrayValue *supers = &classRecord[SUPER_NAME].value<ArrayValue>();

        DENG2_FOR_EACH_CONST(ArrayValue::Elements, i, newSupers.elements())
        {
            supers->add((*i)->duplicateAsReference());
        }
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
    to << SerialId(SCOPE) << *d->identifier << *d->superRecords << d->compound;
}

void ScopeStatement::operator << (Reader &from)
{
    SerialId id;
    from >> id;
    if(id != SCOPE)
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
