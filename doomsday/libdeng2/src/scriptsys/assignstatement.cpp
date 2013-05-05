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

#include "de/AssignStatement"
#include "de/Context"
#include "de/Expression"
#include "de/NameExpression"
#include "de/ArrayValue"
#include "de/RefValue"
#include "de/Writer"
#include "de/Reader"

using namespace de;

AssignStatement::AssignStatement() : _indexCount(0)
{}

AssignStatement::AssignStatement(Expression *target, Indices const &indices, Expression *value) 
    : _indexCount(0)
{
    _args.add(value);
    _indexCount = indices.size();
    for(Indices::const_reverse_iterator i = indices.rbegin(); i != indices.rend(); ++i)
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
    if(!ref)
    {
        throw LeftValueError("AssignStatement::execute",
            "Cannot assign into '" + results.at(0).asText() + "'");
    }

    // The new value that will be assigned to the destination.
    // Ownership of this instance will be given to the variable.
    QScopedPointer<Value> value(results.pop());

    if(_indexCount > 0)
    {
        // The value we will be modifying.
        Value *target = &ref->dereference();

        for(dint i = 0; i < _indexCount; ++i)
        {   
            // Get the evaluated index.
            QScopedPointer<Value> index(results.pop()); // Not released -- will be destroyed.
            if(i < _indexCount - 1)
            {
                // Switch targets to a subelement.
                target = &target->element(*index.data());
            }
            else
            {
                // The setting is done with final value. Ownership transferred.
                target->setElement(*index.data(), value.take());
            }
        }
    }
    else
    {
        // Extract the value from the results array (no copies).
        ref->assign(value.take());
    }
    
    // Should we set the variable to read-only mode?
    if(_args.back().flags() & Expression::ReadOnly)
    {
        DENG2_ASSERT(ref->variable() != NULL);
        ref->variable()->setMode(Variable::ReadOnly | ref->variable()->mode());
    }

    context.proceed();
}

void AssignStatement::operator >> (Writer &to) const
{
    to << SerialId(ASSIGN) << duint8(_indexCount) << _args;    
}

void AssignStatement::operator << (Reader &from)
{
    SerialId id;
    from >> id;
    if(id != ASSIGN)
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
