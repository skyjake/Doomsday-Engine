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

#include "de/numbervalue.h"
#include "de/writer.h"
#include "de/reader.h"
#include "de/math.h"

#include <sstream>

namespace de {

const NumberValue NumberValue::zero(0.0);
const NumberValue NumberValue::one(1.0);
const NumberValue NumberValue::bTrue(true);
const NumberValue NumberValue::bFalse(false);

NumberValue::NumberValue(Number initialValue, SemanticHints semantic)
    : _value(Number(initialValue)), _semantic(semantic)
{}

NumberValue::NumberValue(dint64 initialInteger)
    : _value(Number(initialInteger)), _semantic(Int)
{}

NumberValue::NumberValue(duint64 initialUnsignedInteger)
    : _value(Number(initialUnsignedInteger)), _semantic(UInt)
{}

NumberValue::NumberValue(dint32 initialInteger, SemanticHints semantic)
    : _value(Number(initialInteger)), _semantic(semantic)
{}

NumberValue::NumberValue(duint32 initialUnsignedInteger, SemanticHints semantic)
    : _value(Number(initialUnsignedInteger)), _semantic(semantic)
{}

// NumberValue::NumberValue(unsigned long initialUnsignedInteger, SemanticHints semantic)
//     : _value(Number(initialUnsignedInteger)), _semantic(semantic)
// {}

NumberValue::NumberValue(bool initialBoolean)
    : _value(initialBoolean ? True : False), _semantic(Boolean)
{}

void NumberValue::setSemanticHints(SemanticHints hints)
{
    _semantic = hints;
}

Value *NumberValue::duplicate() const
{
    return new NumberValue(_value, _semantic);
}

Value::Number NumberValue::asNumber() const
{
    return _value;
}

Value::Text NumberValue::asText() const
{
    std::ostringstream s;
    if ((_semantic & Boolean) && (roundi(_value) == True || roundi(_value) == False))
    {
        s << (isTrue()? "True" : "False");
    }
    else if (_semantic & Hex)
    {
        s << Stringf("0x%x", duint32(_value));
    }
    else if (_semantic & Int)
    {
        s << String::asText(roundi(_value));
    }
    else if (_semantic & UInt)
    {
        s << String::asText(round<duint64>(_value));
    }
    else
    {
        s << _value;
    }
    return s.str();
}

bool NumberValue::isTrue() const
{
    return !fequal(_value, 0.0);
}

dint NumberValue::compare(const Value &value) const
{
    const NumberValue *other = dynamic_cast<const NumberValue *>(&value);
    if (other)
    {
        if (fequal(_value, other->_value))
        {
            return 0;
        }
        return cmp(_value, other->_value);
    }
    return Value::compare(value);
}

void NumberValue::negate()
{
    _value = -_value;
}

void NumberValue::sum(const Value &value)
{
    const NumberValue *other = dynamic_cast<const NumberValue *>(&value);
    if (!other)
    {
        throw ArithmeticError("NumberValue::sum", "Values cannot be summed");
    }

    _value += other->_value;
}

void NumberValue::subtract(const Value &value)
{
    const NumberValue *other = dynamic_cast<const NumberValue *>(&value);
    if (!other)
    {
        throw ArithmeticError("Value::subtract", "Value cannot be subtracted from");
    }

    _value -= other->_value;
}

void NumberValue::divide(const Value &divisor)
{
    const NumberValue *other = dynamic_cast<const NumberValue *>(&divisor);
    if (!other)
    {
        throw ArithmeticError("NumberValue::divide", "Value cannot be divided");
    }

    _value /= other->_value;
}

void NumberValue::multiply(const Value &value)
{
    const NumberValue *other = dynamic_cast<const NumberValue *>(&value);
    if (!other)
    {
        throw ArithmeticError("NumberValue::multiply", "Value cannot be multiplied");
    }

    _value *= other->_value;
}

void NumberValue::modulo(const Value &divisor)
{
    const NumberValue *other = dynamic_cast<const NumberValue *>(&divisor);
    if (!other)
    {
        throw ArithmeticError("Value::modulo", "Modulo not defined");
    }

    // Modulo is done with integers.
    _value = int(_value) % int(other->_value);
}

// Flags for serialization:
static const duint8 SEMANTIC_BOOLEAN = 0x01;
static const duint8 SEMANTIC_HEX     = 0x02;
static const duint8 SEMANTIC_INT     = 0x04;
static const duint8 SEMANTIC_UINT    = 0x08;

void NumberValue::operator >> (Writer &to) const
{
    duint8 flags = (_semantic & Boolean? SEMANTIC_BOOLEAN : 0) |
                   (_semantic & Hex?     SEMANTIC_HEX     : 0) |
                   (_semantic & Int?     SEMANTIC_INT     : 0) |
                   (_semantic & UInt?    SEMANTIC_UINT    : 0);

    to << SerialId(NUMBER) << flags << _value;
}

void NumberValue::operator << (Reader &from)
{
    SerialId id;
    from >> id;
    if (id != NUMBER)
    {
        /// @throw DeserializationError The identifier that species the type of the
        /// serialized value was invalid.
        throw DeserializationError("NumberValue::operator <<", "Invalid ID");
    }
    duint8 flags;
    from >> flags >> _value;

    _semantic = SemanticHints((flags & SEMANTIC_BOOLEAN? Boolean : 0) |
                              (flags & SEMANTIC_HEX?     Hex : 0) |
                              (flags & SEMANTIC_INT?     Int : 0) |
                              (flags & SEMANTIC_UINT?    UInt : 0));
}

Value::Text NumberValue::typeId() const
{
    return "Number";
}

NumberValue::SemanticHints NumberValue::semanticHints() const
{
    return _semantic;
}

} // namespace de
