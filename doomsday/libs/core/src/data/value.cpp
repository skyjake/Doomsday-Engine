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

#include "de/value.h"

#include "de/animationvalue.h"
#include "de/arrayvalue.h"
#include "de/blockvalue.h"
#include "de/dictionaryvalue.h"
#include "de/scripting/functionvalue.h"
#include "de/nonevalue.h"
#include "de/numbervalue.h"
#include "de/reader.h"
#include "de/recordvalue.h"
#include "de/textvalue.h"
#include "de/timevalue.h"

namespace de {

Value::~Value()
{}

Value *Value::duplicateAsReference() const
{
    return duplicate();
}

Value::Number Value::asNumber() const
{
    /// @throw ConversionError Value cannot be converted to number.
    throw ConversionError("Value::asNumber", "Illegal conversion");
}

Value::Number Value::asSafeNumber(const Number &defaultValue) const
{
    try
    {
        return asNumber();
    }
    catch (const Error &)
    {
        return defaultValue;
    }
}

int Value::asInt() const
{
    const double num = asNumber();
    if (num > std::numeric_limits<int>::max())
    {
        return std::numeric_limits<int>::max();
    }
    return round<int>(num);
}

int Value::asUInt() const
{
    const double num = asNumber();
    if (num < 0)
    {
        throw ArithmeticError("Value::asUInt", "Cannot convert negative number to unsigned integer");
    }
    if (num > std::numeric_limits<uint32_t>::max())
    {
        throw ArithmeticError("Value::asUInt", "Value is too large to represent as uint32");
    }
    return uint32_t(num + 0.5);
}

StringList Value::asStringList() const
{
    StringList str;
    if (is<ArrayValue>(this))
    {
        for (const Value *val : as<ArrayValue>().elements())
        {
            str << val->asText();
        }
    }
    else
    {
        str << asText();
    }
    return str;
}

Record *Value::memberScope() const
{
    // By default, there are no members are thus no scope for them.
    return 0;
}

dsize Value::size() const
{
    /// @throw IllegalError Size is meaningless.
    throw IllegalError("Value::size", "Size is meaningless");
}

const Value &Value::element(const Value &/*index*/) const
{
    /// @throw IllegalError Value cannot be indexed.
    throw IllegalError(
        "Value::element",
        stringf("Value cannot be indexed (%s \"%s\")", typeid(*this).name(), asText().c_str()));
}

Value &Value::element(const Value &/*index*/)
{
    /// @throw IllegalError Value cannot be indexed.
    throw IllegalError("Value::element", "Value cannot be indexed");
}

Value *Value::duplicateElement(const Value &index) const
{
    return element(index).duplicate();
}

void Value::setElement(const Value &/*index*/, Value *)
{
    /// @throw IllegalError Value cannot be indexed.
    throw IllegalError("Value::setElement", "Value cannot be indexed");
}

bool Value::contains(const Value &/*value*/) const
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

dint Value::compare(const Value &value) const
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

void Value::sum(const Value &/*value*/)
{
    /// @throw ArithmeticError Value cannot be summed.
    throw ArithmeticError("Value::sum", "Value cannot be summed");
}

void Value::subtract(const Value &/*subtrahend*/)
{
    /// @throw ArithmeticError Value cannot be subtracted from.
    throw ArithmeticError("Value::subtract", "Value cannot be subtracted from");
}

void Value::divide(const Value &/*divisor*/)
{
    /// @throw ArithmeticError Value cannot be divided.
    throw ArithmeticError("Value::divide", "Value cannot be divided");
}

void Value::multiply(const Value &/*value*/)
{
    /// @throw ArithmeticError Value cannot be multiplied.
    throw ArithmeticError("Value::multiply", "Value cannot be multiplied");
}

void Value::modulo(const Value &/*divisor*/)
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

void Value::call(Process &, const Value &, Value *) const
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

    std::unique_ptr<Value> result;
    switch (id)
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

    case TIME:
        result.reset(new TimeValue);
        break;

    case ANIMATION:
        result.reset(new AnimationValue);
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

/*
Value *Value::constructFrom(const Value &value)
{
    switch (variant.type())
    {
    case QVariant::Invalid:
        return new NoneValue;

    case QVariant::Bool:
        return new NumberValue(variant.toBool());

    case QVariant::Double:
        return new NumberValue(variant.toDouble());

    default:
        break;
    }

    if (variant.canConvert<QVariantList>())
    {
        std::unique_ptr<ArrayValue> array(new ArrayValue);
        foreach (QVariant value, variant.toList())
        {
            *array << constructFrom(value);
        }
        return array.release();
    }

    if (variant.canConvert<QVariantMap>())
    {
        const auto map = variant.toMap();
        if (map.contains(DE_STR("__obj__")) &&
            map[DE_STR("__obj__")] == DE_STR("Record"))
        {
            std::unique_ptr<Record> rec(new Record);
            foreach (QString key, map.keys())
            {
                std::unique_ptr<Value> v(constructFrom(map[key]));
                rec->add(new Variable(key, v.release()));
            }
            return new RecordValue(rec.release(), RecordValue::OwnsRecord);
        }
        else
        {
            std::unique_ptr<DictionaryValue> dict(new DictionaryValue);
            foreach (QString key, map.keys())
            {
                std::unique_ptr<Value> value(constructFrom(map[key]));
                dict->add(new TextValue(key), value.release());
            }
            return dict.release();
        }
    }

    return new TextValue(variant.toString());
}
*/

const Value &Value::element(dint index) const
{
    return element(NumberValue(index));
}

Value &Value::element(dint index)
{
    return element(NumberValue(index));
}

} // namespace de
