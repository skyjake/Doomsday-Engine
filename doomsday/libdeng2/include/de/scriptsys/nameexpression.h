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

#ifndef LIBDENG2_NAMEEXPRESSION_H
#define LIBDENG2_NAMEEXPRESSION_H

#include "../Expression"
#include "../String"
#include "../Flag"

namespace de
{
    /**
     * Responsible for referencing, creating, and deleting variables and record
     * references based an textual identifier.
     *
     * @ingroup script
     */
    class NameExpression : public Expression
    {
    public:
        /// Identifier is not text. @ingroup errors
        DEFINE_ERROR(IdentifierError);

        /// Variable already exists when it was required not to. @ingroup errors
        DEFINE_ERROR(AlreadyExistsError);

        /// The identifier does not specify an existing variable. @ingroup errors
        DEFINE_ERROR(NotFoundError);

        // Note: the flags below are serialized as is, so don't change the existing values.
        
        /// Evaluates to a value. In conjunction with IMPORT, causes the imported
        /// record to be copied to the local namespace.
        DEFINE_FLAG(BY_VALUE, 0);
        
        /// Evaluates to a reference.
        DEFINE_FLAG(BY_REFERENCE, 1);
        
        /// If missing, create a new variable.
        DEFINE_FLAG(NEW_VARIABLE, 2);

        /// If missing, create a new record.
        DEFINE_FLAG(NEW_RECORD, 3);
        
        /// Identifier must exist and will be deleted.
        DEFINE_FLAG(DELETE, 4);
        
        /// Imports an external namespace into the local namespace (as a reference).
        DEFINE_FLAG(IMPORT, 5);

        /// Look for object in local namespace only. 
        DEFINE_FLAG(LOCAL_ONLY, 6);
        
        /// If the identifier is in scope, returns a reference to the process's 
        /// throwaway variable.
        DEFINE_FLAG(THROWAWAY_IF_IN_SCOPE, 7);

        /// Identifier must not already exist in scope.
        DEFINE_FLAG(NOT_IN_SCOPE, 8);

        /// Variable will be set to read-only mode.
        DEFINE_FINAL_FLAG(READ_ONLY, 9, Flags);

    public:
        NameExpression();
        NameExpression(const String& identifier, const Flags& flags = BY_VALUE);
        ~NameExpression();

        /// Returns the identifier in the name expression.
        const String& identifier() const { return identifier_; }

        const Flags& flags() const { return flags_; }

        Value* evaluate(Evaluator& evaluator) const;

        // Implements ISerializable.
        void operator >> (Writer& to) const;
        void operator << (Reader& from);                 
        
    private:
        String identifier_;
        Flags flags_;
    };
}

#endif /* LIBDENG2_NAMEEXPRESSION_H */
