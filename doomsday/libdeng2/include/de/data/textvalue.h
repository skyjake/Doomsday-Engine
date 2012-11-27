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

#ifndef LIBDENG2_TEXTVALUE_H
#define LIBDENG2_TEXTVALUE_H

#include "../Value"
#include "../String"

#include <list>

namespace de
{
    /**
     * The TextValue class is a subclass of Value that holds a text string.
     *
     * @ingroup data
     */
    class DENG2_PUBLIC TextValue : public Value
    {
    public:
        /// An error occurs in string pattern replacements. @ingroup errors
        DENG2_ERROR(IllegalPatternError);
        
    public:
        TextValue(String const &initialValue = "");

        /// Converts the TextValue to plain text.
        operator String const &() const;

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
        
        static String substitutePlaceholders(String const &pattern, 
            const std::list<Value const *> &args);
        
        // Implements ISerializable.
        void operator >> (Writer &to) const;
        void operator << (Reader &from);
        
    protected:
        /// Changes the text of the value.
        void setValue(String const &text);
        
    private:
        Text _value;
    };
}

#endif /* LIBDENG2_TEXTVALUE_H */
