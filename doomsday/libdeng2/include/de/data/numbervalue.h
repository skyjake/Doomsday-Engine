/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2004-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDENG2_NUMBERVALUE_H
#define LIBDENG2_NUMBERVALUE_H

#include "../Value"

namespace de
{
    /**
     * The NumberValue class is a subclass of Value that holds a single
     * double precision floating point number.
     *
     * All numbers are stored using ddouble. Integer values can be represented
     * exactly with doubles:
     * http://en.wikipedia.org/wiki/Floating_point#IEEE_754:_floating_point_in_modern_computers
     *
     * @ingroup data
     */
    class DENG2_PUBLIC NumberValue : public Value
    {
    public:
        /// Truth values for logical operations. They are treated as
        /// number values.
        enum {
            VALUE_FALSE = 0,
            VALUE_TRUE = 1
        };
        
    public:
        NumberValue(Number initialValue = 0);

        /**
         * Conversion template that forces a cast to another type.
         */
        template <typename Type>
        Type as() const { return Type(_value); }

        Value* duplicate() const;
        Number asNumber() const;
        Text asText() const;
        bool isTrue() const;
        dint compare(const Value& value) const;
        void negate();        
        void sum(const Value& value);        
        void subtract(const Value& value);
        void divide(const Value& divisor);
        void multiply(const Value& value);
        void modulo(const Value& divisor);

        // Implements ISerializable.
        void operator >> (Writer& to) const;
        void operator << (Reader& from);
        
    private:
        Number _value;
    };
}

#endif /* LIBDENG2_NUMBERVALUE_H */
