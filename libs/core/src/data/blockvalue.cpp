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

#include "de/blockvalue.h"
#include "de/writer.h"
#include "de/reader.h"

#include <sstream>

namespace de {

BlockValue::BlockValue()
{}

BlockValue::BlockValue(const Block &block) : _value(block)
{}

Block &BlockValue::block()
{
    return _value;
}

const Block &BlockValue::block() const
{
    return _value;
}

BlockValue::operator const IByteArray &() const
{
    return _value;
}

BlockValue::operator IByteArray &()
{
    return _value;
}

void BlockValue::clear()
{
    _value.clear();
}

Value *BlockValue::duplicate() const
{
    return new BlockValue(_value);
}

Value::Text BlockValue::asText() const
{
    return "(Block of " + String::asText(_value.size()) + " bytes)";
}

dsize BlockValue::size() const
{
    return _value.size();
}

bool BlockValue::isTrue() const
{
    return _value.size() > 0;
}

void BlockValue::sum(const Value &value)
{
    const BlockValue *other = dynamic_cast<const BlockValue *>(&value);
    if (!other)
    {
        /// @throw ArithmeticError @a value was not a BlockValue. BlockValue can only be
        /// summed with another BlockValue.
        throw ArithmeticError("BlockValue::sum", "Value cannot be summed");
    }
    _value += other->_value;
}

void BlockValue::operator >> (Writer &to) const
{
    to << SerialId(BLOCK) << _value;
}

void BlockValue::operator << (Reader &from)
{
    SerialId id;
    from >> id;
    if (id != BLOCK)
    {
        /// @throw DeserializationError The identifier that species the type of the
        /// serialized value was invalid.
        throw DeserializationError("BlockValue::operator <<", "Invalid ID");
    }
    _value.clear();
    from >> _value;
}

Value::Text BlockValue::typeId() const
{
    return "Block";
}

} // namespace de
