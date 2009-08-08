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

namespace de
{
    class Evaluator;
    class Value;
    class Record;

    /**
     * Base class for expressions.
     *
     * @ingroup script
     */
    class Expression : public ISerializable
    {
    public:
        /// Deserialization of an expression failed. @ingroup errors
        DEFINE_ERROR(DeserializationError);
                
    public:
        virtual ~Expression();

        virtual void push(Evaluator& evaluator, Record* names = 0) const;
        
        virtual Value* evaluate(Evaluator& evaluator) const = 0;
        
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
    };
}

#endif /* LIBDENG2_EXPRESSION_H */
