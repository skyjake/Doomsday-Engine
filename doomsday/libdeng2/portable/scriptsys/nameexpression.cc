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

#include "de/NameExpression"
#include "de/Evaluator"
#include "de/Process"
#include "de/TextValue"
#include "de/RefValue"

using namespace de;

NameExpression::NameExpression(Expression* identifier, const Flags& flags) 
    : identifier_(identifier), flags_(flags)
{}

NameExpression::~NameExpression()
{
    delete identifier_;
}

void NameExpression::push(Evaluator& evaluator, Record* names) const
{
    Expression::push(evaluator, names);
    identifier_->push(evaluator);
}

Value* NameExpression::evaluate(Evaluator& evaluator) const
{
    //LOG_AS("NameExpression::evaluator");
    //LOG_DEBUG("path = %s, scope = %x") << path_ << evaluator.names();
    
    // We are expecting a text value for the identifier.
    std::auto_ptr<Value> ident(evaluator.popResult());
    TextValue* name = dynamic_cast<TextValue*>(ident.get());
    if(!name)
    {
        /// @throw IdentifierError  Identifier is not text.
        throw IdentifierError("NameExpression::evaluator", "Identifier should be a text value");
    }

    // Collect the namespaces to search.
    Evaluator::Namespaces spaces;
    evaluator.namespaces(spaces);
    
    Variable* variable = 0;
    
    for(Evaluator::Namespaces::iterator i = spaces.begin(); i != spaces.end(); ++i)
    {
        Record& ns = **i;
        if(ns.has(*name))
        {
            // The name exists in this namespace.
            variable = &ns[*name];
            break;
        }
        if(flags_[LOCAL_ONLY_BIT])
        {
            break;
        }
    }

    // If a new variable is required and one is in scope, we cannot continue.
    if(variable && (flags_[NOT_IN_SCOPE_BIT]))
    {
        throw AlreadyExistsError("NameExpression::evaluate", "Variable '" + name->asText() + 
            "' already exists");
    }

    // If nothing is found and we are permitted to create new variables, do so.
    // Occurs when assigning into new variables.
    if(!variable && (flags_[NEW_VARIABLE_BIT]))
    {
        variable = new Variable(*name);
        
        // Add it to the local namespace.
        spaces.front()->add(variable);
    }

    // We should now have the variable.
    if(variable)
    {
        if(flags_[BY_REFERENCE_BIT])
        {
            // Reference to the variable.
            return new RefValue(variable);
        }
        else
        {
            // Variables evaluate to their values.
            return variable->value().duplicate();
        }
    }
    
    throw NotFoundError("NameExpression::evaluate", "Identifier '" + name->asText() + 
        "' does not exist");
}
