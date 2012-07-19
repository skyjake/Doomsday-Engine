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

#ifndef LIBDENG2_DICTIONARYVALUE_H
#define LIBDENG2_DICTIONARYVALUE_H

#include "../Value"

#include <map>

namespace de
{
    /**
     * Subclass of Value that contains an array of values, indexed by any value.
     *
     * @ingroup data
     */
    class DENG2_PUBLIC DictionaryValue : public Value
    {
    public:
        /// An invalid key was used. @ingroup errors
        DENG2_ERROR(KeyError);
        
        struct ValueRef {
            ValueRef(const Value* v) : value(v) {}
            ValueRef(const ValueRef& other) : value(other.value) {}
                 
            // Operators for the map.
            bool operator < (const ValueRef& other) const {
                return value->compare(*other.value) < 0;
            }
                        
            const Value* value;
        };
        
        typedef std::map<ValueRef, Value*> Elements;
        
    public:
        DictionaryValue();
        DictionaryValue(const DictionaryValue& other);
        ~DictionaryValue();

        /// Returns a direct reference to the elements map.
        const Elements& elements() const { return _elements; }

        /**
         * Clears the dictionary of all values.
         */
        void clear();

        /**
         * Adds a key-value pair to the dictionary. If the key already exists, its
         * old value will be replaced by the new one.
         *
         * @param key  Key.
         * @param value  Value.
         */ 
        void add(Value* key, Value* value);
        
        // Implementations of pure virtual methods.
        Value* duplicate() const;
        Text asText() const;
        dsize size() const;     
        const Value& element(const Value& index) const; 
        Value& element(const Value& index); 
        void setElement(const Value& index, Value* value);
        bool contains(const Value& value) const;
        Value* begin();
        Value* next();
        bool isTrue() const;
        dint compare(const Value& value) const;
        void sum(const Value& value);
        void subtract(const Value& subtrahend);
        
        // Implements ISerializable.
        void operator >> (Writer& to) const;
        void operator << (Reader& from);
        
    private:
        Elements _elements;
        Elements::iterator _iteration;
        bool _validIteration;
    };
}

#endif /* LIBDENG2_DICTIONARYVALUE_H */
