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

#ifndef LIBCORE_ARRAYVALUE_H
#define LIBCORE_ARRAYVALUE_H

#include "de/value.h"
#include "de/list.h"
#include "de/numbervalue.h"
#include "de/vector.h"

namespace de {

/**
 * The ArrayValue class is a subclass of Value that holds a dynamic
 * array of other values. The array is indexed using (integer) numbers.
 *
 * @ingroup data
 */
class DE_PUBLIC ArrayValue : public Value
{
public:
    /// Attempt to index the array with indices that are not defined for the array. @ingroup errors
    DE_ERROR(OutOfBoundsError);

    /// The index used for accessing the array is of the wrong type. @ingroup errors
    DE_ERROR(IllegalIndexError);

    /// Type for the elements. Public because const access to the elements is public.
    typedef List<Value *> Elements;

public:
    ArrayValue();
    ArrayValue(const ArrayValue &other);
    ArrayValue(const StringList &strings);
    ArrayValue(std::initializer_list<Value *> values);

    /**
     * Construct an array out of the values in a vector.
     * @param vec  Any kind of vector type (one of de::Vector<>).
     */
    template <typename VecType>
    ArrayValue(const VecType &vec) {
        for (int i = 0; i < vec.size(); ++i) {
            add(new NumberValue(vec[i]));
        }
    }

    ~ArrayValue();

    /// Const accessor to the array elements.
    const Elements &elements() const { return _elements; }

    /**
     * Adds a new Value to the elements of the array. The value is
     * added to the end of the list of elements.
     * @param value Value to add to the array. The array takes
     *     ownership of the object.
     */
    void add(Value *value);

    /**
     * Adds a new TextValue to the elements of the array. The value is
     * added to the end of the list of elements.
     * @param text Text to add to the array.
     */
    void add(const String &text);

    void addMany(duint count, Number value);

    void addMany(duint count, const String &value);

    /**
     * Pops the last element and gives its ownership to the caller.
     * @return Last element of the array. Ownership transferred.
     */
    Value *popLast();

    /**
     * Pops the first element and gives its ownership to the caller.
     * @return First element of the array. Ownership transferred.
     */
    Value *popFirst();

    /**
     * Inserts a new Value into the elements of the array at an
     * arbitrary location. The new Value's index will be @a index.
     * @param index  Index for the new Value.
     * @param value  Value to add. The array takes ownership of the object.
     */
    void insert(dint index, Value *value);

    /**
     * Replaces an existing Value in the array. The previous value
     * at the index will be destroyed.
     * @param index  Index of the Value.
     * @param value  New value. The array takes ownership of the object.
     */
    void replace(dint index, Value *value);

    /**
     * Removes a Value from the array. The Value will be destroyed.
     * @param index  Index of the Value to remove.
     */
    void remove(dint index);

    /**
     * Finds the element maching @a value and returns its index.
     * @param value  Value to find.
     * @return Index of the element, or -1 if not found.
     */
    dint indexOf(const Value &value) const;

    /**
     * Adds a value to the array.
     * @param value  Value to add. Array gets ownership.
     * @return Reference to the array.
     */
    ArrayValue &operator << (Value *value);

    /**
     * Adds a value to the array.
     * @param value  Value to add. A duplicate of this value is added to the array.
     * @return Reference to the array.
     */
    ArrayValue &operator << (const Value &value);

    /**
     * Returns a reference to a value in the array.
     * @param index  Index of the element.
     * @return  Element at the index.
     */
    const Value &at(dint index) const;

    const Value &front() const { return at(0); }

    const Value &back() const { return at(dint(size()) - 1); }

    /**
     * Empties the array of all values.
     */
    void clear();

    /// Reverse the order of the elements.
    void reverse();

    /// Converts each element to text and returns them as a list.
    StringList toStringList() const;

    // Implementations of pure virtual methods.
    Text typeId() const;
    Value *duplicate() const;
    Text asText() const;
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

    // Implements ISerializable.
    void operator >> (Writer &to) const;
    void operator << (Reader &from);

    /**
     * Calls all the elements in the array with the given arguments. A
     * temporary process is used for each call. An exception is thrown if a
     * non-function element value is encountered.
     *
     * @param args  Arguments for the calls. The first element must be a
     *              DictionaryValue containing any labeled argument values.
     */
    void callElements(const ArrayValue &args);

    /**
     * Convenient element setter for native code.
     * @param index  Index to set.
     * @param value  Value to set.
     */
    void setElement(dint index, Number value);

    /**
     * Convenient element setter for native code.
     * @param index  Index to set.
     * @param value  Value to set.
     */
    void setElement(dint index, const String &value);

    const Value &element(dint index) const;
    const Value &operator [] (dint index) const;

    String asInfo() const;

private:
    Elements::iterator indexToIterator(dint index);
    Elements::const_iterator indexToIterator(dint index) const;

private:
    Elements _elements;

    /// Current position of the iterator.
    dint _iteration;
};

} // namespace de

#endif /* LIBCORE_ARRAYVALUE_H */
