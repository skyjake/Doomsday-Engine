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

#ifndef LIBDENG2_EXPRESSION_H
#define LIBDENG2_EXPRESSION_H

#include "../ISerializable"

#include <QFlags>

namespace de
{
    class Evaluator;
    class Value;
    class Record;

    /**
     * Base class for expressions.
     *
     * @note  All expression classes must call the serialization methods of this class
     *        so that the expression flags are properly serialized.
     *
     * @ingroup script
     */
    class Expression : public ISerializable
    {
    public:
        /// Deserialization of an expression failed. @ingroup errors
        DEFINE_ERROR(DeserializationError);

        // Flags for evaluating expressions.
        // Note: these are serialized as is, so don't change the existing values.
        enum Flag
        {
            /*ByValue = 0x1,
            ByReference = 0x2,
            LocalNamespaceOnly = 0x4,
            RequireNewIdentifier = 0x8,
            AllowNewRecords = 0x10,
            AllowNewVariables = 0x20,
            DeleteIdentifier = 0x40,
            ImportNamespace = 0x80,
            ThrowawayIfInScope = 0x100,
            SetReadOnly = 0x200*/

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
        virtual ~Expression();

        virtual void push(Evaluator& evaluator, Record* names = 0) const;
        
        virtual Value* evaluate(Evaluator& evaluator) const = 0;
        
        /**
         * Returns the flags of the expression.
         */
        const Flags& flags () const;

        /**
         * Sets the flags of the expression.
         */
        void setFlags(Flags f);

        /**
         * Subclasses must call this in their serialization method.
         */
        void operator >> (Writer& to) const;

        /**
         * Subclasses must call this in their deserialization method.
         */
        void operator << (Reader& from);

    public:
        /**
         * Constructs an expression by deserializing one from a reader.
         *
         * @param reader  Reader.
         *
         * @return  The deserialized expression. Caller gets ownership.
         */
        static Expression* constructFrom(Reader& reader);

    protected:
        typedef dbyte SerialId;
        
        enum SerialIds {
            ARRAY,
            BUILT_IN,
            CONSTANT,
            DICTIONARY,
            NAME,
            OPERATOR
        };

    private:
        Flags _flags;
    };
}

#endif /* LIBDENG2_EXPRESSION_H */
