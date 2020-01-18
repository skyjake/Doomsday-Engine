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

#ifndef LIBDENG2_VALUE_H
#define LIBDENG2_VALUE_H

#include "../libcore.h"
#include "../Deletable"
#include "../ISerializable"
#include "../String"

#include <QVariant>

namespace de {

class Process;
class Record;

/**
 * The base class for all runtime values.  This is an abstract class.
 *
 * @ingroup data
 */
class DENG2_PUBLIC Value : public Deletable, public String::IPatternArg, public ISerializable
{
public:
    /// An illegal operation (i.e., one that is not defined by the Value) was attempted.
    /// @ingroup errors
    DENG2_ERROR(IllegalError);

    /// An illegal conversion was attempted. @ingroup errors
    DENG2_SUB_ERROR(IllegalError, ConversionError);

    /// An illegal arithmetic operation is attempted (e.g., division by text). @ingroup errors
    DENG2_SUB_ERROR(IllegalError, ArithmeticError);

    /// Value cannot be serialized. @ingroup errors
    DENG2_SUB_ERROR(IllegalError, CannotSerializeError);

    // Types used by all values:
    typedef ddouble Number;     /**< Numbers are in double-precision. */
    typedef String  Text;       /**< Text strings. */

public:
    virtual ~Value();

    /**
     * Creates a duplicate copy of the value.
     *
     * @return New Value object. Caller gets ownership of the object.
     */
    virtual Value *duplicate() const = 0;

    /**
     * Creates a duplicate copy of the value. If the value has ownership of its data,
     * the ownership relationship is not replicated in the duplicate, but instead
     * the duplicate references the original data.
     *
     * If the Value does not support this type of ownership, this method behaves just
     * like duplicate().
     *
     * @return New Value object. Caller gets ownership of the object.
     */
    virtual Value *duplicateAsReference() const;

    /**
     * Returns the type of the value as a text string identifier. For instance, for
     * a TextValue this would be 'Text'.
     *
     * @return Type identifier.
     */
    virtual Text typeId() const = 0;

    /**
     * Convert the value to a number.  Implementing this is
     * optional.  The default implementation will raise an
     * exception.
     */
    virtual Number asNumber() const;

    /**
     * Convert the value to a number. This will never raise an
     * exception. If the conversion is not possible, it will
     * return @a defaultValue.
     *
     * @return Value as number.
     */
    virtual Number asSafeNumber(Number const &defaultValue = 0.0) const;

    /**
     * Convert the value to the nearest 32-bit signed integer. Uses asNumber().
     */
    int asInt() const;

    /**
     * Convert the value to the nearest 32-bit unsigned integer. Uses asNumber().
     */
    int asUInt() const;

    /**
     * Convert the value to a list of strings using asText().
     * @return List of text strings.
     */
    StringList asStringList() const;

    /**
     * Convert the value to into a text string.  All values have
     * to implement this.
     */
    virtual Text asText() const = 0;

    template <typename ValueType>
    ValueType &as() {
        ValueType *t = dynamic_cast<ValueType *>(this);
        if (!t) {
            throw ConversionError("Value::as<>", QString("Illegal type conversion from ") +
                                  typeid(*this).name() + " to " + typeid(ValueType).name());
        }
        return *t;
    }

    template <typename ValueType>
    ValueType const &as() const {
        ValueType const *t = dynamic_cast<ValueType const *>(this);
        if (!t) {
            throw ConversionError("Value::as<>", QString("Illegal const type conversion from ") +
                                  typeid(*this).name() + " to " + typeid(ValueType).name());
        }
        return *t;
    }

    /**
     * Returns the scope for any members of this value. When evaluating a member in
     * reference to this value, this will return the primary scope using which the
     * members can be found.
     *
     * For built-in types, this may return one of the Script.* records.
     */
    virtual Record *memberScope() const;

    /**
     * Determine the size of the value.  The meaning of this
     * depends on the type of the value.
     */
    virtual dsize size() const;

    /**
     * Get a specific element of the value.  This is meaningful with
     * arrays and dictionaries.
     *
     * @param index  Index of the element.
     *
     * @return  Element value (non-modifiable).
     */
    virtual Value const &element(Value const &index) const;

    Value const &element(dint index) const;

    /**
     * Get a specific element of the value.  This is meaningful with
     * arrays and dictionaries.
     *
     * @param index  Index of the element.
     *
     * @return  Element value (modifiable).
     */
    virtual Value &element(Value const &index);

    Value &element(dint index);

    /**
     * Duplicates an element of the value. This is necessary when the
     * value is immutable: one can take copies of the contained
     * elements but it is not possible to access the originals directly.
     *
     * @param index  Index of the element.
     *
     * @return  Duplicate of the element value. Caller gets ownership.
     */
    virtual Value *duplicateElement(Value const &index) const;

    /**
     * Set a specific element of the value. This is meaningful only with
     * arrays and dictionaries, which are composed out of modifiable
     * elements.
     *
     * @param index         Index of the element.
     * @param elementValue  New value for the element. This value will take
     *                      ownership of @a elementValue.
     */
    virtual void setElement(Value const &index, Value *elementValue);

    /**
     * Determines whether the value contains the equivalent of another
     * value. Defined for arrays.
     *
     * @param value Value to look for.
     *
     * @return @c true, if the value is there, otherwise @c false.
     */
    virtual bool contains(Value const &value) const;

    /**
     * Begin iteration of contained values. This is only meaningful with
     * iterable values such as arrays.
     *
     * @return  The first value. Caller gets ownership. @c NULL, if there
     *      are no items.
     */
    virtual Value *begin();

    /**
     * Iterate the next value. This is only meaningful with iterable
     * values such as arrays.
     *
     * @return @c NULL, if the iteration is over. Otherwise a new Value
     * containing the next iterated value. Caller gets ownership.
     */
    virtual Value *next();

    /**
     * Determine if the value can be thought of as a logical truth.
     */
    virtual bool isTrue() const = 0;

    /**
     * Determine if the value can be thought of as a logical falsehood.
     */
    virtual bool isFalse() const;

    /**
     * Compares this value to another.
     *
     * @param value Value to compare with.
     *
     * @return 0, if the values are equal. 1, if @a value is greater than
     *      this value. -1, if @a value is less than this value.
     */
    virtual dint compare(Value const &value) const;

    /**
     * Negate the value of this value.
     */
    virtual void negate();

    /**
     * Calculate the sum of this value and an another value, storing the
     * result in this value.
     */
    virtual void sum(Value const &value);

    /**
     * Calculate the subtraction of this value and an another value,
     * storing the result in this value.
     */
    virtual void subtract(Value const &subtrahend);

    /**
     * Calculate the division of this value divided by @a divisor, storing
     * the result in this value.
     */
    virtual void divide(Value const &divisor);

    /**
     * Calculate the multiplication of this value by @a value, storing
     * the result in this value.
     */
    virtual void multiply(Value const &value);

    /**
     * Calculate the modulo of this value divided by @a divisor, storing
     * the result in this value.
     */
    virtual void modulo(Value const &divisor);

    /**
     * Assign value. Only supported by reference values.
     *
     * @param value  Value to assign. Referenced object gets ownership.
     */
    virtual void assign(Value *value);

    /**
     * Applies the call operator on the value.
     *
     * @param process    Process where the call is made.
     * @param arguments  Arguments of the call.
     * @param self       Optional scope that becomes the value of the "self"
     *                   variable in the called function's local namespace.
     *                   Ownership taken.
     */
    virtual void call(Process &process, Value const &arguments, Value *self = 0) const;

public:
    /**
     * Construct a value by reading data from the Reader.
     * @param reader  Data for the value.
     * @return  Value. Caller gets owernship.
     */
    static Value *constructFrom(Reader &reader);

    /**
     * Construct a value by converting from a QVariant.
     * @param variant  Data for the value.
     * @return Value. Caller gets ownership.
     */
    static Value *constructFrom(QVariant const &variant);

protected:
    typedef dbyte SerialId;

    enum SerialIds {
        NONE,
        NUMBER,
        TEXT,
        ARRAY,
        DICTIONARY,
        BLOCK,
        FUNCTION,
        RECORD,
        TIME,
        URI,
        ANIMATION,
    };
};

/**
 * Utility for converting a Value with two elements into a Range. This
 * only works if the value contains a sufficient number of elements and they
 * can be converted to numbers.
 *
 * @param value  Value with multiple elements.
 *
 * @return Vector.
 */
template <typename RangeType>
RangeType rangeFromValue(Value const &value) {
    return RangeType(value.element(0).asNumber(),
                     value.element(1).asNumber());
}

} // namespace de

#endif /* LIBDENG2_VALUE_H */
