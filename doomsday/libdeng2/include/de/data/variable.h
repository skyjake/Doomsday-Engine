/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2009, 2011 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDENG2_VARIABLE_H
#define LIBDENG2_VARIABLE_H

#include "../ISerializable"
#include "../String"
#include "../Audience"

#include <QFlags>

namespace de
{
    class Value;
    
    /**
     * Stores a value and name identifier. Variables are typically stored in a Record.
     * A variable's behavior is defined by its mode flags.
     *
     * @ingroup data
     */
    class LIBDENG2_API Variable : public ISerializable
    {
    public:
        /// There was an attempt to change the value of a read-only variable. @ingroup errors
        DEFINE_ERROR(ReadOnlyError);
        
        /// An invalid value type was used. The mode flags denied using a value of the
        /// given type with the variable. @ingroup errors
        DEFINE_ERROR(InvalidError);
        
        /// Variable name contains invalid characters. @ingroup errors
        DEFINE_ERROR(NameError);
        
        /// Value could not be converted to the attempted type. @ingroup errors
        DEFINE_ERROR(TypeError);
        
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

            /// The default mode allows reading and writing all types of values,
            /// including NoneValue.
            DefaultMode = AllowNone | AllowNumber | AllowText | AllowArray |
                AllowDictionary | AllowBlock | AllowFunction | AllowRecord | AllowRef
        };
        //@}
        Q_DECLARE_FLAGS(Flags, Flag);
        
    public:
        /**
         * Constructs a new variable.
         *
         * @param name  Name for the variable. Any periods (.) are not allowed.
         * @param initial  Initial value. Variable gets ownership. If no value is given here,
         *      a NoneValue will be created for the variable.
         * @param mode  Mode flags.
         */
        Variable(const String& name = "", Value* initial = 0,
                 const Flags& _mode = DefaultMode);
            
        /**
         * Constructs a copy of another variable.
         *
         * @param other  Variable to copy.
         */
        Variable(const Variable& other);
            
        virtual ~Variable();
        
        /**
         * Returns the name of the variable.
         */
        const String& name() const { return _name; }
        
        /**
         * Sets the value of the variable.
         *
         * @param v  New value. Variable gets ownership.
         */
        void set(Value* v);
        
        /**
         * Sets the value of the variable.
         *
         * @param v  New value. Variable gets ownership.
         */
        Variable& operator = (Value* v);
        
        /**
         * Sets the value of the variable.
         *
         * @param v  New value. Variable takes a copy of this.
         */
        void set(const Value& v);
        
        /**
         * Returns the value of the variable (non-modifiable).
         */
        const Value& value() const;
        
        /**
         * Returns the value of the variable.
         */
        Value& value();

        /**
         * Returns the value of the variable.
         */
        template <typename Type>
        Type& value() {
            Type* v = dynamic_cast<Type*>(_value);
            if(!v) {
                /// @throw TypeError Casting to Type failed.
                throw TypeError("Variable::value<Type>", "Illegal type conversion");
            }
            return *v;
        }

        /**
         * Returns the value of the variable.
         */
        template <typename Type>
        const Type& value() const {
            const Type* v = dynamic_cast<const Type*>(_value);
            if(!v) {
                /// @throw TypeError Casting to Type failed.
                throw TypeError("Variable::value<Type>", "Illegal type conversion");
            }
            return *v;
        }
        
        /**
         * Returns the current mode flags of the variable.
         */
        Flags mode() const;

        /**
         * Sets the mode flags of the variable.
         *
         * @param flags  New mode flags that will replace the current ones.
         */
        void setMode(const Flags& flags);

        /**
         * Checks that a value is valid, checking what is allowed in the mode
         * flags.
         *
         * @param v  Value to check.
         *
         * @return @c true, if the value is valid. @c false otherwise.
         */
        bool isValid(const Value& v) const;
        
        /**
         * Verifies that a value is valid, checking against what is allowed in the 
         * mode flags. If not, an exception is thrown.
         *
         * @param v  Value to test.
         */
        void verifyValid(const Value& v) const;
        
        /**
         * Verifies that the variable can be assigned a new value.
         */
        void verifyWritable();
        
        /**
         * Verifies that a string is a valid name for the variable. If not,
         * an exception is thrown.
         *
         * @param s  Name to test.
         */
        static void verifyName(const String& s);
        
        // Implements ISerializable.
        void operator >> (Writer& to) const;
        void operator << (Reader& from);
        
    public:
        /**
         * The variable is about to be deleted.
         *
         * @param variable  Variable.
         */
        DEFINE_AUDIENCE(Deletion, void variableBeingDeleted(Variable& variable));

        /**
         * The value of the variable has changed.
         *
         * @param variable  Variable.
         * @param newValue  New value of the variable.
         */
        DEFINE_AUDIENCE(Change, void variableValueChanged(Variable& variable, const Value& newValue));

    private:        
        String _name;

        /// Value of the variable.
        Value* _value;

        /// Mode flags.
        Flags _mode;
    };
}

Q_DECLARE_OPERATORS_FOR_FLAGS(de::Variable::Flags);

#endif /* LIBDENG2_VARIABLE_H */
