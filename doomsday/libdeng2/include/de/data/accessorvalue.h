/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2009-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDENG2_ACCESSORVALUE_H
#define LIBDENG2_ACCESSORVALUE_H

#include "../TextValue"
#include "../Variable"

namespace de
{
    /**
     * Special text value that provides access to properties of another object.
     *
     * @ingroup data
     */
    class AccessorValue : public TextValue
    {
    public:
        /// Mode to use for variables that have an accessor value.
        static const Variable::Flags VARIABLE_MODE;
        
    public:
        AccessorValue();
        
        /// Update the text content of the accessor.
        virtual void update() const = 0;

        /// Creates a new value with the content of the accessor. The returned
        /// value should not be an AccessorValue.
        virtual Value *duplicateContent() const = 0;

        Value *duplicate() const;
        Number asNumber() const;
        Text asText() const;
        dsize size() const;
        bool isTrue() const;
        dint compare(Value const &value) const;
        void sum(Value const &value);
        void multiply(Value const &value);
        void divide(Value const &value);
        void modulo(Value const &divisor);
        void operator >> (Writer &to) const;
        void operator << (Reader &from);
    };
}

#endif /* LIBDENG2_ACCESSORVALUE_H */
    
