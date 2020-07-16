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

#ifndef LIBCORE_DICTIONARYVALUE_H
#define LIBCORE_DICTIONARYVALUE_H

#include "de/value.h"

#include <map>

namespace de {

class ArrayValue;
class Record;

/**
 * Subclass of Value that contains an array of values, indexed by any value.
 *
 * @ingroup data
 */
class DE_PUBLIC DictionaryValue : public Value
{
public:
    /// An invalid key was used. @ingroup errors
    DE_ERROR(KeyError);

    struct ValueRef {
        ValueRef(const Value *v) : value(v) {}
        ValueRef(const ValueRef &other) : value(other.value) {}

        // Operators for the map.
        bool operator < (const ValueRef &other) const {
            return value->compare(*other.value) < 0;
        }

        const Value *value;
    };

    typedef std::map<ValueRef, Value *> Elements;

public:
    DictionaryValue();
    DictionaryValue(const DictionaryValue &other);
    ~DictionaryValue();

    /// Returns a direct reference to the elements map.
    const Elements &elements() const { return _elements; }

    /// Returns a direct reference to the elements map.
    Elements &elements() { return _elements; }

    /**
     * Clears the dictionary of all values.
     */
    void clear();

    /**
     * Adds a key-value pair to the dictionary. If the key already exists, its
     * old value will be replaced by the new one.
     *
     * @param key    Key. Ownership given to DictionaryValue.
     * @param value  Value. Ownership given to DictionaryValue.
     */
    void add(Value *key, Value *value);

    /**
     * Removes a key-value pair from the dictionary.
     *
     * @param key  Key that will be removed.
     */
    void remove(const Value &key);

    void remove(const Elements::iterator &pos);

    const Value *find(const Value &key) const;

    enum ContentSelection { Keys, Values };

    /**
     * Creates an array with the keys or the values of the dictionary.
     *
     * @param selection  Keys or values.
     *
     * @return Caller gets ownership.
     */
    ArrayValue *contentsAsArray(ContentSelection selection) const;

    Record toRecord() const;

    inline const Value &operator[](const Value &index) const { return element(index); }
    inline Value &      operator[](const Value &index) { return element(index); }

    // Implementations of pure virtual methods.
    Text typeId() const;
    Value *duplicate() const;
    Text asText() const;
    Record *memberScope() const;
    dsize size() const;
    const Value &element(const Value &index) const;
    Value &element(const Value &index);
    void setElement(const Value &index, Value *value);
    bool contains(const Value &value) const;
    Value *begin();
    Value *next();
    bool isTrue() const;
    dint compare(const Value &value) const;
    void sum(const Value &value);
    void subtract(const Value &subtrahend);

    // Implements ISerializable.
    void operator >> (Writer &to) const;
    void operator << (Reader &from);

private:
    Elements _elements;
    Elements::iterator _iteration;
    bool _validIteration;
};

} // namespace de

#endif /* LIBCORE_DICTIONARYVALUE_H */
