/*
 * The Doomsday Engine Project -- libcore
 *
 * Copyright © 2004-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/NameExpression"
#include "de/Evaluator"
#include "de/Process"
#include "de/TextValue"
#include "de/RefValue"
#include "de/ArrayValue"
#include "de/RecordValue"
#include "de/ScopeStatement"
#include "de/Writer"
#include "de/Reader"
#include "de/App"
#include "de/Module"

namespace de {

String const NameExpression::LOCAL_SCOPE = "-";

DENG2_PIMPL_NOREF(NameExpression)
{
    String identifier;
    String scopeIdentifier;

    Instance(String const &id      = "",
             String const &scopeId = "")
        : identifier(id)
        , scopeIdentifier(scopeId)
    {}

    Variable *findInRecord(String const & name,
                           Record const & where,
                           Record *&      foundIn,
                           bool           lookInClass = true) const
    {
        if(where.hasMember(name))
        {
            // The name exists in this namespace. Even though the lookup was done as
            // const, the caller expects non-const return values.
            foundIn = const_cast<Record *>(&where);
            return const_cast<Variable *>(&where[name]);
        }
        if(lookInClass && where.hasMember(Record::SUPER_NAME))
        {
            // The namespace is derived from another record. Let's look into each
            // super-record in turn.
            ArrayValue const &supers = where.geta(Record::SUPER_NAME);
            for(int i = 0; i < int(supers.size()); ++i)
            {
                if(Variable *found = findInRecord(
                        name, supers.at(i).as<RecordValue>().dereference(), foundIn))
                {
                    return found;
                }
            }
        }
        return 0;
    }

    Variable *findInNamespaces(String const & name,
                               Evaluator::Namespaces const &spaces,
                               bool           localOnly,
                               Record *&      foundInNamespace,
                               Record **      higherNamespace = 0)
    {
        DENG2_FOR_EACH_CONST(Evaluator::Namespaces, i, spaces)
        {
            Record &ns = **i;
            if(Variable *variable =
                    findInRecord(name, ns, foundInNamespace,
                                   // allow looking in class if local not required:
                                   !localOnly))
            {
                // The name exists in this namespace.
                // Also note the higher namespace (for export).
                Evaluator::Namespaces::const_iterator next = i;
                if(++next != spaces.end())
                {
                    if(higherNamespace) *higherNamespace = *next;
                }
                return variable;
            }
            if(localOnly)
            {
                // Not allowed to look in outer scopes.
                break;
            }
        }
        return 0;
    }
};

} // namespace de

namespace de {

NameExpression::NameExpression() : d(new Instance)
{}

NameExpression::NameExpression(String const &identifier, Flags const &flags,
                               String const &scopeIdentifier)
    : d(new Instance(identifier, scopeIdentifier))
{
    setFlags(flags);
}

String const &NameExpression::identifier() const
{
    return d->identifier;
}

Value *NameExpression::evaluate(Evaluator &evaluator) const
{
    //LOG_AS("NameExpression::evaluate");
    //std::cout << "NameExpression::evaluator: " << _flags.to_string() << "\n";
    LOGDEV_SCR_XVERBOSE_DEBUGONLY("evaluating name:\"%s\" flags:%x", d->identifier << flags());

    // Collect the namespaces to search.
    Evaluator::Namespaces spaces;

    Record *foundInNamespace = 0;
    Record *higherNamespace = 0;
    Variable *variable = 0;

    if(d->scopeIdentifier.isEmpty() || d->scopeIdentifier == LOCAL_SCOPE)
    {
        if(d->scopeIdentifier != LOCAL_SCOPE)
        {
            // This is the usual case: scope defined by the left side of the member
            // operator, or if that is not specified, the context's namespace stack.
            evaluator.namespaces(spaces);
        }
        else
        {
            // Start with the context's local namespace.
            evaluator.process().namespaces(spaces);
        }
        variable = d->findInNamespaces(d->identifier, spaces, flags().testFlag(LocalOnly),
                                       foundInNamespace, &higherNamespace);
    }
    else
    {
        // An explicit scope has been defined; try to find it first. Look in the current
        // context of the process, ignoring any narrower scopes that may apply here.
        evaluator.process().namespaces(spaces);
        Variable *scope = d->findInNamespaces(d->scopeIdentifier, spaces, false, foundInNamespace);
        if(!scope)
        {
            throw NotFoundError("NameExpression::evaluate",
                                "Scope '" + d->scopeIdentifier + "' not found");
        }
        // Locate the identifier from this scope, disregarding the regular
        // namespace context.
        variable = d->findInRecord(d->identifier, scope->valueAsRecord(),
                                   foundInNamespace);
    }

    if(flags().testFlag(ThrowawayIfInScope) && variable)
    {
        foundInNamespace = 0;
        variable = &evaluator.context().throwaway();
    }

    // If a new variable/record is required and one is in scope, we cannot continue.
    if(flags().testFlag(NotInScope) && variable)
    {
        throw AlreadyExistsError("NameExpression::evaluate", 
            "Identifier '" + d->identifier + "' already exists");
    }

    // Create a new subrecord in the namespace? ("record xyz")
    if(flags().testFlag(NewSubrecord) ||
       (flags().testFlag(NewSubrecordIfNotInScope) && !variable))
    {
        // Replaces existing member with this identifier.
        Record &record = spaces.front()->addRecord(d->identifier);
        return new RecordValue(record);
    }

    // If nothing is found and we are permitted to create new variables, do so.
    // Occurs when assigning into new variables.
    if(!variable && flags().testFlag(NewVariable))
    {
        variable = new Variable(d->identifier);

        // Add it to the local namespace.
        spaces.front()->add(variable);

        // Take note of the namespaces.
        foundInNamespace = spaces.front();
        if(!higherNamespace && spaces.size() > 1)
        {
            Evaluator::Namespaces::iterator i = spaces.begin();
            higherNamespace = *(++i);
        }
    }

    // Export variable into a higher namespace?
    if(flags().testFlag(Export))
    {
        DENG2_ASSERT(!flags().testFlag(ThrowawayIfInScope));

        if(!variable)
        {
            throw NotFoundError("NameExpression::evaluate",
                                "Cannot export nonexistent identifier '" + d->identifier + "'");
        }
        if(!higherNamespace)
        {
            throw NotFoundError("NameExpression::evaluate",
                                "No higher namespace for exporting '" + d->identifier + "' into");
        }
        if(higherNamespace != foundInNamespace && foundInNamespace)
        {
            foundInNamespace->remove(*variable);
            higherNamespace->add(variable);
        }
    }

    // Should we import a namespace?
    if(flags() & Import)
    {
        Record *record = &App::scriptSystem().importModule(d->identifier,
            evaluator.process().globals()["__file__"].value().asText());

        // Overwrite any existing member with this identifier.
        spaces.front()->add(variable = new Variable(d->identifier));

        if(flags().testFlag(ByValue))
        {
            // Take a copy of the record ("import record").
            *variable = new RecordValue(new Record(*record), RecordValue::OwnsRecord);
        }
        else
        {
            // The variable will merely reference the module.
            *variable = new RecordValue(record);
        }

        return new RecordValue(record);
    }
    
    if(variable)
    {
        // Variables can be referred to by reference or value.
        if(flags() & ByReference)
        {
            // Reference to the variable.
            return new RefValue(variable);
        }
        else
        {
            // Variables evaluate to their values. As a special case, values may have
            // ownership of their data. Here we don't want to duplicate the data, only
            // reference it.
            return variable->value().duplicateAsReference();
        }
    }
    
    throw NotFoundError("NameExpression::evaluate", "Identifier '" + d->identifier +
                        "' does not exist");
}

void NameExpression::operator >> (Writer &to) const
{
    to << SerialId(NAME);

    Expression::operator >> (to);

    to << d->identifier << d->scopeIdentifier;
}

void NameExpression::operator << (Reader &from)
{
    SerialId id;
    from >> id;
    if(id != NAME)
    {
        /// @throw DeserializationError The identifier that species the type of the 
        /// serialized expression was invalid.
        throw DeserializationError("NameExpression::operator <<", "Invalid ID");
    }

    Expression::operator << (from);

    from >> d->identifier;

    if(from.version() >= DENG2_PROTOCOL_1_15_0_NameExpression_with_scope_identifier)
    {
        from >> d->scopeIdentifier;
    }
}

} // namespace de
