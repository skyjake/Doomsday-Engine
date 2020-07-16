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

#include "de/scripting/nameexpression.h"
#include "de/app.h"
#include "de/arrayvalue.h"
#include "de/reader.h"
#include "de/recordvalue.h"
#include "de/refvalue.h"
#include "de/scripting/evaluator.h"
#include "de/scripting/module.h"
#include "de/scripting/process.h"
#include "de/scripting/scopestatement.h"
#include "de/scripting/scriptsystem.h"
#include "de/textvalue.h"
#include "de/writer.h"

namespace de {

const char *NameExpression::LOCAL_SCOPE = "-";

DE_PIMPL_NOREF(NameExpression)
{
    StringList identifierSequence;
   
    Variable *findInRecord(const String & name,
                           const Record & where,
                           Record *&      foundIn,
                           bool           lookInClass = true) const
    {
        if (where.hasMember(name))
        {
            // The name exists in this namespace. Even though the lookup was done as
            // const, the caller expects non-const return values.
            foundIn = const_cast<Record *>(&where);
            return const_cast<Variable *>(&where[name]);
        }
        if (lookInClass && where.hasMember(Record::VAR_SUPER))
        {
            // The namespace is derived from another record. Let's look into each
            // super-record in turn. Check in reverse order; the superclass added last
            // overrides earlier ones.
            const ArrayValue &supers = where.geta(Record::VAR_SUPER);
            for (int i = int(supers.size() - 1); i >= 0; --i)
            {
                if (Variable *found = findInRecord(
                        name, supers.at(i).as<RecordValue>().dereference(), foundIn))
                {
                    return found;
                }
            }
        }
        return 0;
    }

    Variable *findInNamespaces(const String & name,
                               const Evaluator::Namespaces &spaces,
                               bool           localOnly,
                               Record *&      foundInNamespace,
                               Record **      higherNamespace = 0)
    {
        DE_FOR_EACH_CONST(Evaluator::Namespaces, i, spaces)
        {
            Record &ns = *i->names;
            if (Variable *variable =
                    findInRecord(name, ns, foundInNamespace,
                                   // allow looking in class if local not required:
                                   !localOnly))
            {
                // The name exists in this namespace.
                // Also note the higher namespace (for export).
                Evaluator::Namespaces::const_iterator next = i;
                if (++next != spaces.end())
                {
                    if (higherNamespace) *higherNamespace = next->names;
                }
                return variable;
            }
            if (localOnly)
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

NameExpression::NameExpression() : d(new Impl)
{}

NameExpression::NameExpression(const String &identifier, Flags flags)
    : d(new Impl)
{
    d->identifierSequence << "" << identifier;
    setFlags(flags);
}

NameExpression::NameExpression(const StringList &identifierSequence, Flags flags)                              
    : d(new Impl)
{
    d->identifierSequence = identifierSequence;
    setFlags(flags);
}

const String &NameExpression::identifier() const
{
    return d->identifierSequence.back();
}

Value *NameExpression::evaluate(Evaluator &evaluator) const
{
    //LOG_AS("NameExpression::evaluate");
    //LOGDEV_SCR_XVERBOSE_DEBUGONLY("evaluating name:\"%s\" flags:%x", d->identifier << flags());

    // Collect the namespaces to search.
    Evaluator::Namespaces spaces;

    Record *foundInNamespace = nullptr;
    Record *higherNamespace = nullptr;
    Variable *variable = nullptr;

    const String scopeIdentifier = d->identifierSequence.front();
    String identifier = d->identifierSequence.at(1);

    if (!scopeIdentifier || scopeIdentifier == LOCAL_SCOPE)
    {
        if (!scopeIdentifier)
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
        variable = d->findInNamespaces(identifier, spaces, flags().testFlag(LocalOnly),
                                       foundInNamespace, &higherNamespace);
    }
    else
    {
        // An explicit scope has been defined; try to find it first. Look in the current
        // context of the process, ignoring any narrower scopes that may apply here.
        evaluator.process().namespaces(spaces);
        Variable *scope = d->findInNamespaces(scopeIdentifier, spaces, false, foundInNamespace);
        if (!scope)
        {
            throw NotFoundError("NameExpression::evaluate",
                                "Scope '" + scopeIdentifier + "' not found");
        }
        // Locate the identifier from this scope, disregarding the regular
        // namespace context.
        variable = d->findInRecord(identifier, scope->valueAsRecord(),
                                   foundInNamespace);
    }

    // Look up the rest in relation to what was already found.
    for (int i = 2; i < d->identifierSequence.sizei(); ++i)
    {
        if (!variable)
        {
            throw NotFoundError("NameExpression::evaluate",
                                "Scope '" + identifier + "' not found");
        }
        identifier = d->identifierSequence.at(i);
        variable   = d->findInRecord(identifier, variable->valueAsRecord(), foundInNamespace);
    }

    if (flags().testFlag(ThrowawayIfInScope) && variable)
    {
        foundInNamespace = nullptr;
        variable = &evaluator.context().throwaway();
    }

    // If a new variable/record is required and one is in scope, we cannot continue.
    if (flags().testFlag(NotInScope) && variable)
    {
        throw AlreadyExistsError("NameExpression::evaluate",
                                 "Identifier '" + identifier + "' already exists");
    }

    // Create a new subrecord in the namespace? ("record xyz")
    if (flags().testFlag(NewSubrecord) ||
       (flags().testFlag(NewSubrecordIfNotInScope) && !variable))
    {
        // Replaces existing member with this identifier.
        Record &record = spaces.front().names->addSubrecord(identifier);
        return new RecordValue(record);
    }

    // If nothing is found and we are permitted to create new variables, do so.
    // Occurs when assigning into new variables.
    if (!variable && flags().testFlag(NewVariable))
    {
        variable = new Variable(identifier);

        // Add it to the local namespace.
        spaces.front().names->add(variable);

        // Take note of the namespaces.
        foundInNamespace = spaces.front().names;
        if (!higherNamespace && spaces.size() > 1)
        {
            Evaluator::Namespaces::iterator i = spaces.begin();
            higherNamespace = (++i)->names;
        }
    }

#if 0
    // Export variable into a higher namespace?
    if (flags().testFlag(Export))
    {
        DE_ASSERT(!flags().testFlag(ThrowawayIfInScope));

        if (!variable)
        {
            throw NotFoundError("NameExpression::evaluate",
                                "Cannot export nonexistent identifier '" + identifier + "'");
        }
        if (!higherNamespace)
        {
            throw NotFoundError("NameExpression::evaluate",
                                "No higher namespace for exporting '" + identifier + "' into");
        }
        if (higherNamespace != foundInNamespace && foundInNamespace)
        {
            foundInNamespace->remove(*variable);
            higherNamespace->add(variable);
        }
    }
#endif

    // Should we import a namespace?
    if (flags() & Import)
    {
        Record *record = &App::scriptSystem().importModule(identifier,
            evaluator.process().globals()[Record::VAR_FILE].value().asText());

        // Overwrite any existing member with this identifier.
        spaces.front().names->add(variable = new Variable(identifier));

        if (flags().testFlag(ByValue))
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

    if (variable)
    {
        // Variables can be referred to by reference or value.
        if (flags() & ByReference)
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

    throw NotFoundError("NameExpression::evaluate", "Identifier '" + identifier +
                        "' does not exist");
}

void NameExpression::operator >> (Writer &to) const
{
    to << SerialId(NAME);

    Expression::operator >> (to);

    to << uint8_t(d->identifierSequence.size());
    for (const auto &i : d->identifierSequence)
    {
        to << i;
    }
}

void NameExpression::operator << (Reader &from)
{
    SerialId id;
    from >> id;
    if (id != NAME)
    {
        /// @throw DeserializationError The identifier that species the type of the
        /// serialized expression was invalid.
        throw DeserializationError("NameExpression::operator <<", "Invalid ID");
    }

    Expression::operator << (from);

    if (from.version() < DE_PROTOCOL_2_2_0_NameExpression_identifier_sequence)
    {
        String ident, scopeIdent;
        from >> ident;
        if (from.version() >= DE_PROTOCOL_1_15_0_NameExpression_with_scope_identifier)
        {
            from >> scopeIdent;
        }
        d->identifierSequence << scopeIdent;
        d->identifierSequence << ident;
    }
    else
    {
        uint8_t count = 0;
        from >> count;
        while (count-- != 0)
        {
            String ident;
            from >> ident;
            d->identifierSequence << ident;
        }
    }
}

} // namespace de
