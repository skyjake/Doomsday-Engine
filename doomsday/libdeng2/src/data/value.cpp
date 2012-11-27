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

#include "de/Value"
#include "de/NoneValue"
#include "de/NumberValue"
#include "de/TextValue"
#include "de/ArrayValue"
#include "de/DictionaryValue"
#include "de/BlockValue"
#include "de/FunctionValue"
#include "de/RecordValue"
#include "de/Reader"

using namespace de;

Value::~Value()
{}

Value::Number Value::asNumber() const
{
    /// @throw ConversionError Value cannot be converted to number.
    throw ConversionError("Value::asNumber", "Illegal conversion");
}

Value::Number Value::asSafeNumber(Number const &defaultValue) const
{
    try
    {
        return asNumber();
    }
    catch(Error const &)
    {
        return defaultValue;
    }
}

dsize Value::size() const
{
    /// @throw IllegalError Size is meaningless.
    throw IllegalError("Value::size", "Size is meaningless");
}

Value const &Value::element(Value const &/*index*/) const
{
    /// @throw IllegalError Value cannot be indexed.
    throw IllegalError("Value::element", "Value cannot be indexed");
}

Value &Value::element(Value const &/*index*/)
{
    /// @throw IllegalError Value cannot be indexed.
    throw IllegalError("Value::element", "Value cannot be indexed");
}

Value *Value::duplicateElement(Value const &index) const
{
    return element(index).duplicate();
}

void Value::setElement(Value const &/*index*/, Value *)
{
    /// @throw IllegalError Value cannot be indexed.
    throw IllegalError("Value::setElement", "Value cannot be indexed");
}

bool Value::contains(Value const &/*value*/) const
{
    /// @throw IllegalError Value cannot contain other values.
    throw IllegalError("Value::contains", "Value is not a container");
}

Value *Value::begin()
{
    /// @throw IllegalError Value is not iterable.
    throw IllegalError("Value::begin", "Value is not iterable");
}

Value *Value::next()
{
    /// @throw IllegalError Value is not iterable.
    throw IllegalError("Value::next", "Value is not iterable");
}

bool Value::isFalse() const
{ 
    // Default implementation -- some values may be neither true nor false.
    return !isTrue();
}

dint Value::compare(Value const &value) const
{
    // Default to a generic text-based comparison.
    dint result = asText().compare(value.asText());
    return (result < 0? -1 : result > 0? 1 : 0);
}

void Value::negate()
{
    /// @throw ArithmeticError Value cannot be negated.
    throw ArithmeticError("Value::negate", "Value cannot be negated");
}
 
void Value::sum(Value const &/*value*/)
{
    /// @throw ArithmeticError Value cannot be summed.
    throw ArithmeticError("Value::sum", "Value cannot be summed");
}

void Value::subtract(Value const &/*subtrahend*/)
{
    /// @throw ArithmeticError Value cannot be subtracted from.
    throw ArithmeticError("Value::subtract", "Value cannot be subtracted from");    
}
 
void Value::divide(Value const &/*divisor*/)
{
    /// @throw ArithmeticError Value cannot be divided.
    throw ArithmeticError("Value::divide", "Value cannot be divided");
}
 
void Value::multiply(Value const &/*value*/)
{
    /// @throw ArithmeticError Value cannot be multiplied.
    throw ArithmeticError("Value::multiply", "Value cannot be multiplied");
}
 
void Value::modulo(Value const &/*divisor*/)
{
    /// @throw ArithmeticError Module operation is not defined for the value.
    throw ArithmeticError("Value::modulo", "Modulo not defined");
}

void Value::assign(Value *value)
{
    delete value;
    /// @throw IllegalError Cannot assign to value.
    throw IllegalError("Value::assign", "Cannot assign to value");
}

void Value::call(Process &/*process*/, Value const &/*arguments*/) const
{
    /// @throw IllegalError Value cannot be called.
    throw IllegalError("Value::call", "Value cannot be called");
}

Value *Value::constructFrom(Reader &reader)
{
    SerialId id;
    reader.mark();
    reader >> id;
    reader.rewind();
    
    std::auto_ptr<Value> result;
    switch(id)
    {
    case NONE:
        result.reset(new NoneValue);
        break;
        
    case NUMBER:
        result.reset(new NumberValue);
        break;
        
    case TEXT:
        result.reset(new TextValue);
        break;
        
    case ARRAY:
        result.reset(new ArrayValue);
        break;
        
    case DICTIONARY:
        result.reset(new DictionaryValue);
        break;
        
    case BLOCK:
        result.reset(new BlockValue);
        break;
        
    case FUNCTION:
        result.reset(new FunctionValue);
        break;

    case RECORD:
        result.reset(new RecordValue(new Record, RecordValue::OwnsRecord));
        break;
        
    default:
        /// @throw DeserializationError The identifier that species the type of the 
        /// serialized value was invalid.
        throw DeserializationError("Value::constructFrom", "Invalid value identifier");
    }

    // Deserialize it.
    reader >> *result.get();
    return result.release();
}
