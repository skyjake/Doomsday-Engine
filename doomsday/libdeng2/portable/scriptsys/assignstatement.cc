/*
 * The Doomsday Engine Project -- libdeng2
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

#include "de/AssignStatement"
#include "de/Context"
#include "de/Expression"
#include "de/NameExpression"
#include "de/ArrayValue"
#include "de/RefValue"

using namespace de;

AssignStatement::AssignStatement(NameExpression* target, const Indices& indices, Expression* value) 
    : indexCount_(0)
{
    args_.add(target);
    indexCount_ = indices.size();
    for(Indices::const_iterator i = indices.begin(); i != indices.end(); ++i)
    {
        args_.add(*i);
    }
    args_.add(value);
}

AssignStatement::~AssignStatement()
{}

void AssignStatement::execute(Context& context) const
{
    Evaluator& eval = context.evaluator();
    eval.evaluate(&args_);
    std::auto_ptr<ArrayValue> results(static_cast<ArrayValue*>(eval.popResult()));

    RefValue* ref = dynamic_cast<RefValue*>(results->elements()[0]);
    if(!ref)
    {
        throw LeftValueError("AssignStatement::execute",
            "Cannot assign into '" + results->elements()[0]->asText() + "'");
    }

    // The new value that will be assigned to the destination.
    // Ownership of this instance will be given to the variable.
    std::auto_ptr<Value> value(results->pop()); 

    if(indexCount_ > 0)
    {
        throw Error("AssignStatement::execute", "Assignment indexing not implemented");
/*        Value* target = &ref->variable().value();

        for(dint i = 0; i < indexCount_; ++i)
        {   
            auto_ptr<Value> index(results->pop()); // Not released -- will be destroyed.
            if(i < indexCount_ - 1)
            {
                // Move into a subelement.
                target = target->element(*index.get());
            }
            else
            {
                // The last one does the setting.
                target->setElement(*index.get(), value.get());                    
            }
        }*/
    }
    else
    {
        // Extract the value from the results array (no copies).
        ref->assign(value.release());
    }

    context.proceed();
}
