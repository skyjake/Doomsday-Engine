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
        DENG2_ERROR(IllegalPatternError)
        
    public:
        TextValue(const String& initialValue = "");

        /// Converts the TextValue to plain text.
        operator const String& () const;

        Value* duplicate() const;
        Number asNumber() const;
        Text asText() const;
        dsize size() const;
        bool isTrue() const;
        dint compare(const Value& value) const;
        void sum(const Value& value);
        void multiply(const Value& value);
        void divide(const Value& value);
        void modulo(const Value& divisor);
        
        static String substitutePlaceholders(const String& pattern, 
            const std::list<const Value*>& args);
        
        // Implements ISerializable.
        void operator >> (Writer& to) const;
        void operator << (Reader& from);
        
    protected:
        /// Changes the text of the value.
        void setValue(const String& text);
        
    private:
        Text _value;
    };
}

#endif /* LIBDENG2_TEXTVALUE_H */
