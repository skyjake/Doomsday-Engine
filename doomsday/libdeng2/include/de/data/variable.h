/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2009 Jaakko Keränen <jaakko.keranen@iki.fi>
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
#include "../Flag"

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
        /// Variable's value cannot change.
        DEFINE_FLAG(READ_ONLY, 0);
        /// Variable cannot be serialized.
        DEFINE_FLAG(NO_SERIALIZE, 1);
        /// NoneValue allowed as value.
        DEFINE_FLAG(NONE, 2);
        /// NumberValue allowed as value.
        DEFINE_FLAG(NUMBER, 3);
        /// TextValue allowed as value.
        DEFINE_FLAG(TEXT, 4);
        /// ArrayValue allowed as value.
        DEFINE_FLAG(ARRAY, 5);
        /// DictionaryValue allowed as value.
        DEFINE_FLAG(DICTIONARY, 6);
        /// BlockValue allowed as value.
        DEFINE_FLAG(BLOCK, 7);
        /// FunctionValue allowed as value.
        DEFINE_FLAG(FUNCTION, 8);
        /// RecordValue allowed as value.
        DEFINE_FLAG(RECORD, 9);
        /// RefValue allowed as value.
        DEFINE_FINAL_FLAG(REF, 10, Mode);
        //@}
        
        /// The default mode allows reading and writing all types of values, 
        /// including NoneValue.
        static const Flag DEFAULT_MODE = NONE | NUMBER | TEXT | ARRAY | DICTIONARY | BLOCK |
            FUNCTION | RECORD | REF;
        
    public:
        /**
         * Constructs a new variable.
         *
         * @param name  Name for the variable. Any periods (.) are not allowed.
         * @param initial  Initial value. Variable gets ownership. If no value is given here,
         *      a NoneValue will be created for the variable.
         * @param mode  Mode flags.
         */
        Variable(const String& name = "", Value* initial = 0, const Mode& mode = DEFAULT_MODE);
            
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
        const String& name() const { return name_; }
        
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
            Type* v = dynamic_cast<Type*>(value_);
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
            const Type* v = dynamic_cast<const Type*>(value_);
            if(!v) {
                /// @throw TypeError Casting to Type failed.
                throw TypeError("Variable::value<Type>", "Illegal type conversion");
            }
            return *v;
        }
        
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

        /// Mode flags.        
        Mode mode;
        
    private:        
        String name_;

        /// Value of the variable.
        Value* value_;
    };
}

#endif /* LIBDENG2_VARIABLE_H */
