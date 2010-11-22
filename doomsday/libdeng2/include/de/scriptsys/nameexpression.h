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

#include <QFlags>

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
        enum Flag
        {
            /// Evaluates to a value. In conjunction with IMPORT, causes the imported
            /// record to be copied to the local namespace.
            ByValue = 0x1,

            /// Evaluates to a reference.
            ByReference = 0x2,

            /// If missing, create a new variable.
            NewVariable = 0x4,

            /// If missing, create a new record.
            NewRecord = 0x8,

            /// Identifier must exist and will be deleted.
            Delete = 0x10,

            /// Imports an external namespace into the local namespace (as a reference).
            Import = 0x20,

            /// Look for object in local namespace only.
            LocalOnly = 0x40,

            /// If the identifier is in scope, returns a reference to the process's
            /// throwaway variable.
            ThrowawayIfInScope = 0x80,

            /// Identifier must not already exist in scope.
            NotInScope = 0x100,

            /// Variable will be set to read-only mode.
            ReadOnly = 0x200
        };
        Q_DECLARE_FLAGS(Flags, Flag);

    public:
        NameExpression();
        NameExpression(const String& identifier, const Flags& flags = ByValue);
        ~NameExpression();

        /// Returns the identifier in the name expression.
        const String& identifier() const { return _identifier; }

        const Flags& flags() const { return _flags; }

        Value* evaluate(Evaluator& evaluator) const;

        // Implements ISerializable.
        void operator >> (Writer& to) const;
        void operator << (Reader& from);                 
        
    private:
        String _identifier;
        Flags _flags;
    };
}

Q_DECLARE_OPERATORS_FOR_FLAGS(de::NameExpression::Flags);

#endif /* LIBDENG2_NAMEEXPRESSION_H */
