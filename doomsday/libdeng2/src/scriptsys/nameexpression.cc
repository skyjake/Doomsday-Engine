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
#include "de/RecordValue"
#include "de/Writer"
#include "de/Reader"
#include "de/App"
#include "de/Module"

using namespace de;

NameExpression::NameExpression()
{}

NameExpression::NameExpression(const String& identifier, const Flags& flags) 
    : identifier_(identifier), flags_(flags)
{}

NameExpression::~NameExpression()
{}

Value* NameExpression::evaluate(Evaluator& evaluator) const
{
    //std::cout << "NameExpression::evaluator: " << flags_.to_string() << "\n";
    //LOG_DEBUG("path = %s, scope = %x") << path_ << evaluator.names();
    
    // Collect the namespaces to search.
    Evaluator::Namespaces spaces;
    evaluator.namespaces(spaces);
    
    Record* foundInNamespace = 0;
    Variable* variable = 0;
    Record* record = 0;
    
    for(Evaluator::Namespaces::iterator i = spaces.begin(); i != spaces.end(); ++i)
    {
        Record& ns = **i;
        if(ns.hasMember(identifier_))
        {
            // The name exists in this namespace (as a variable).
            variable = &ns[identifier_];
            foundInNamespace = &ns;
            break;
        }
        if(ns.hasSubrecord(identifier_))
        {
            // The name exists in this namespace (as a record).
            record = &ns.subrecord(identifier_);
            foundInNamespace = &ns;
        }
        if(flags_[LOCAL_ONLY_BIT])
        {
            break;
        }
    }

    if(variable && flags_[THROWAWAY_IF_IN_SCOPE_BIT])
    {
        foundInNamespace = 0;
        variable = &evaluator.context().throwaway();
    }

    // If a new variable/record is required and one is in scope, we cannot continue.
    if((variable || record) && flags_[NOT_IN_SCOPE_BIT])
    {
        throw AlreadyExistsError("NameExpression::evaluate", 
            "Identifier '" + identifier_ + "' already exists");
    }
    
    // Should we import a namespace?
    if(flags_[IMPORT_BIT])
    {
        record = &App::importModule(identifier_,
            evaluator.process().globals()["__file__"].value().asText());
            
        // Take a copy if requested.
        if(flags_[BY_VALUE_BIT])
        {
            record = &spaces.front()->add(identifier_, new Record(*record));
        }
    }
    
    // Should we delete the identifier?
    if(flags_[DELETE_BIT])
    {
        if(!variable && !record)
        {
            throw NotFoundError("NameExpression::evaluate", 
                "Cannot delete nonexistent identifier '" + identifier_ + "'");
        }
        assert(foundInNamespace != 0);
        if(variable)
        {
            delete foundInNamespace->remove(*variable);
        }
        else if(record)
        {
            delete foundInNamespace->remove(identifier_);
        }
        return new NoneValue();
    }

    // If nothing is found and we are permitted to create new variables, do so.
    // Occurs when assigning into new variables.
    if(!variable && !record && flags_[NEW_VARIABLE_BIT])
    {
        variable = new Variable(identifier_);
        
        // Add it to the local namespace.
        spaces.front()->add(variable);
    }
    if(variable)
    {
        // Variables can be referred to by reference or value.
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

    // We may be permitted to create a new record.
    if(flags_[NEW_RECORD_BIT])
    {
        if(!record)
        {
            // Add it to the local namespace.
            record = &spaces.front()->addRecord(identifier_);
        }
        else
        {
            // Create a variable referencing the record.
            spaces.front()->add(new Variable(identifier_, new RecordValue(record)));
        }
    }
    if(record)
    {
        // Records can only be referenced.
        return new RecordValue(record);
    }
    
    throw NotFoundError("NameExpression::evaluate", "Identifier '" + identifier_ + 
        "' does not exist");
}

void NameExpression::operator >> (Writer& to) const
{
    to << SerialId(NAME) << identifier_ << duint16(flags_.to_ulong());
}

void NameExpression::operator << (Reader& from)
{
    SerialId id;
    from >> id;
    if(id != NAME)
    {
        /// @throw DeserializationError The identifier that species the type of the 
        /// serialized expression was invalid.
        throw DeserializationError("NameExpression::operator <<", "Invalid ID");
    }
    from >> identifier_;
    duint16 f;
    from >> f;
    flags_ = f;
}
