/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2004-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/BlockValue"
#include "de/Writer"
#include "de/Reader"

#include <sstream>

using namespace de;

BlockValue::BlockValue()
{}

BlockValue::BlockValue(const Block& block) : value_(block) 
{}

BlockValue::operator const IByteArray& () const
{
    return value_;
}

BlockValue::operator IByteArray& ()
{
    return value_;
}

void BlockValue::clear()
{
    value_.clear();
}

Value* BlockValue::duplicate() const
{
    return new BlockValue(value_);
}

Value::Text BlockValue::asText() const
{
    std::ostringstream os;
    os << "[Block of " << value_.size() << " bytes]";
    return os.str();
}

dsize BlockValue::size() const
{
    return value_.size();
}

bool BlockValue::isTrue() const
{
    return value_.size() > 0;
}

void BlockValue::sum(const Value& value)
{
    const BlockValue* other = dynamic_cast<const BlockValue*>(&value);
    if(!other)
    {
        /// @throw ArithmeticError @a value was not a BlockValue. BlockValue can only be
        /// summed with another BlockValue.
        throw ArithmeticError("BlockValue::sum", "Value cannot be summed");
    }    
    value_ += other->value_;
}

void BlockValue::operator >> (Writer& to) const
{
    to << SerialId(BLOCK) << value_;
}

void BlockValue::operator << (Reader& from)
{
    SerialId id;
    from >> id;
    if(id != BLOCK)
    {
        /// @throw DeserializationError The identifier that species the type of the 
        /// serialized value was invalid.
        throw DeserializationError("BlockValue::operator <<", "Invalid ID");
    }
    value_.clear();
    from >> value_;
}
