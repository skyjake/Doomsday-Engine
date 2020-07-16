/*
 * The Doomsday Engine Project -- libcore
 *
 * Copyright © 2009-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBCORE_VARIABLE_H
#define LIBCORE_VARIABLE_H

#include "de/iserializable.h"
#include "de/string.h"
#include "de/observers.h"
#include "de/deletable.h"
#include "de/cstring.h"

namespace de {

class Value;
class ArrayValue;
class Record;

/**
 * Stores a value and name identifier. Variables are typically stored in a Record.
 * A variable's behavior is defined by its mode flags.
 *
 * @ingroup data
 */
class DE_PUBLIC Variable : public Deletable, public ISerializable
{
public:
    /// There was an attempt to change the value of a read-only variable. @ingroup errors
    DE_ERROR(ReadOnlyError);

    /// An invalid value type was used. The mode flags denied using a value of the
    /// given type with the variable. @ingroup errors
    DE_ERROR(InvalidError);

    /// Variable name contains invalid characters. @ingroup errors
    DE_ERROR(NameError);

    /// Value could not be converted to the attempted type. @ingroup errors
    DE_ERROR(TypeError);

    /** @name Mode Flags */
    //@{
    enum Flag
    {
        /// Variable's value cannot change.
        ReadOnly = 0x1,

        /// Variable cannot be serialized.
        NoSerialize = 0x2,

        /// NoneValue allowed as value.
        AllowNone = 0x4,

        /// NumberValue allowed as value.
        AllowNumber = 0x8,

        /// TextValue allowed as value.
        AllowText = 0x10,

        /// ArrayValue allowed as value.
        AllowArray = 0x20,

        /// DictionaryValue allowed as value.
        AllowDictionary = 0x40,

        /// BlockValue allowed as value.
        AllowBlock = 0x80,

        /// FunctionValue allowed as value.
        AllowFunction = 0x100,

        /// RecordValue allowed as value.
        AllowRecord = 0x200,

        /// RefValue allowed as value.
        AllowRef = 0x400,

        /// TimeValue allowed as value.
        AllowTime = 0x800,

        /// Automatically set when the variable's value is changed.
        ValueHasChanged = 0x10000000,

        /// The default mode allows reading and writing all types of values,
        /// including NoneValue.
        DefaultMode = AllowNone | AllowNumber | AllowText | AllowArray |
            AllowDictionary | AllowBlock | AllowFunction | AllowRecord |
            AllowRef | AllowTime
    };
    //@}

public:
    /**
     * Constructs a new variable.
     *
     * @param name  Name for the variable. Any periods (.) are not allowed.
     * @param initial  Initial value. Variable gets ownership. If no value is given here,
     *      a NoneValue will be created for the variable.
     * @param varMode  Mode flags.
     */
    Variable(const String &name = {}, Value *initial = nullptr,
             const Flags &varMode = DefaultMode);

    /**
     * Constructs a copy of another variable.
     *
     * @param other  Variable to copy.
     */
    Variable(const Variable &other);

    virtual ~Variable();

    /**
     * Returns the name of the variable.
     */
    const String &name() const;

    /**
     * Sets the value of the variable.
     *
     * @param v  New value. Variable gets ownership. Cannot be NULL.
     */
    Variable &set(Value *v);

    /**
     * Sets the value of the variable.
     *
     * @param v  New value. Variable gets ownership. Cannot be NULL.
     */
    Variable &operator=(Value *v);

    /**
     * Sets the value of the variable.
     *
     * @param textValue  Text string. A new TextValue is created.
     */
    Variable &operator=(const String &textValue);

    /**
     * Sets the value of the variable.
     *
     * @param v  New value. Variable takes a copy of this.
     */
    Variable &set(const Value &v);

    /**
     * Returns the value of the variable (non-modifiable).
     */
    const Value &value() const;

    /**
     * Returns the value of the variable.
     */
    Value &value();

    Value *valuePtr();
    const Value *valuePtr() const;

    bool operator==(const String &text) const;

    /**
     * Returns the value of the variable.
     */
    template <typename Type>
    Type &value() {
        Type *v = dynamic_cast<Type *>(valuePtr());
        if (!v) {
            /// @throw TypeError Casting to Type failed.
            throw TypeError("Variable::value",
                            String("Illegal type conversion to ") + typeid(Type).name());
        }
        return *v;
    }

    /**
     * Returns the value of the variable.
     */
    template <typename Type>
    const Type &value() const {
        const Type *v = dynamic_cast<const Type *>(valuePtr());
        if (!v) {
            /// @throw TypeError Casting to Type failed.
            throw TypeError("Variable::value",
                            String("Illegal type conversion to ") + typeid(Type).name());
        }
        return *v;
    }

    /**
     * Returns the Record that the variable references. If the variable does
     * not have a RecordValue, an exception is thrown.
     *
     * @return Referenced Record.
     */
    const Record &valueAsRecord() const;

    Record &valueAsRecord();

    /**
     * Returns the value of the variable as an ArrayValue.
     */
    const ArrayValue &array() const;

    ArrayValue &array();

    operator Record & ();
    operator const Record & () const;

    // Automatic conversion to native primitive types.
    operator String () const;
    operator ddouble () const;

    /**
     * Returns the current mode flags of the variable.
     */
    Flags flags() const;

    /**
     * Sets the mode flags of the variable.
     *
     * @param flags      New mode flags.
     * @param operation  What to do with @a flags.
     */
    void setFlags(const Flags &flags, FlagOpArg operation = ReplaceFlags);

    /**
     * Makes the variable read-only.
     *
     * @return Reference to this variable.
     */
    Variable &setReadOnly();

    /**
     * Checks that a value is valid, checking what is allowed in the mode
     * flags.
     *
     * @param v  Value to check.
     *
     * @return @c true, if the value is valid. @c false otherwise.
     */
    bool isValid(const Value &v) const;

    /**
     * Verifies that a value is valid, checking against what is allowed in the
     * mode flags. If not, an exception is thrown.
     *
     * @param v  Value to test.
     */
    void verifyValid(const Value &v) const;

    /**
     * Verifies that the variable can be assigned a new value.
     *
     * @param attemptedNewValue  The new value that is being assigned.
     */
    void verifyWritable(const Value &attemptedNewValue);

    /**
     * Verifies that a string is a valid name for the variable. If not,
     * an exception is thrown.
     *
     * @param s  Name to test.
     */
    static void verifyName(const String &s);

    // Implements ISerializable.
    void operator >> (Writer &to) const;
    void operator << (Reader &from);

public:
    /**
     * The variable is about to be deleted.
     *
     * @param variable  Variable.
     */
    DE_AUDIENCE(Deletion, void variableBeingDeleted(Variable &variable))

    /**
     * The value of the variable has changed.
     *
     * @param variable  Variable.
     * @param newValue  New value of the variable.
     */
    DE_AUDIENCE(Change, void variableValueChanged(Variable &variable, const Value &newValue))

    DE_AUDIENCE(ChangeFrom, void variableValueChangedFrom(Variable &variable, const Value &oldValue,
                                                                     const Value &newValue))

private:
    DE_PRIVATE(d)
};

} // namespace de

#endif /* LIBCORE_VARIABLE_H */
