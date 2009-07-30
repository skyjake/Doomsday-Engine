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

#include "de/ArrayValue"
#include "de/NumberValue"
#include "de/TextValue"
#include "de/Writer"
#include "de/Reader"

#include <sstream>
#include <algorithm>

using namespace de;

ArrayValue::ArrayValue() : Value(), iteration_(0)
{}

ArrayValue::ArrayValue(const ArrayValue& other)
{
    for(Elements::const_iterator i = other.elements_.begin();
        i != other.elements_.end(); ++i)
    {
        elements_.push_back((*i)->duplicate());
    }
    iteration_ = 0;
}

ArrayValue::~ArrayValue()
{
    clear();
}

Value* ArrayValue::duplicate() const
{
    return new ArrayValue(*this);
}

Value::Text ArrayValue::asText() const
{
    std::ostringstream s;
    s << "[";

    bool isFirst = true;

    // Compose a textual representation of the array elements.
    for(Elements::const_iterator i = elements_.begin();
        i != elements_.end(); ++i)
    {
        if(!isFirst)
        {
            s << ",";
        }
        s << " " << (*i)->asText();
        isFirst = false;
    }
    
    s << " ]";
    return s.str();
}

dsize ArrayValue::size() const
{
    return elements_.size();
}

const Value* ArrayValue::element(const Value& indexValue) const
{
    const NumberValue* v = dynamic_cast<const NumberValue*>(&indexValue);
    if(!v)
    {
        /// @throw IllegalIndexError @a indexValue is not a NumberValue.
        throw IllegalIndexError("ArrayValue::element", "Array index must be a number");
    }
    dint index = v->as<dint>();
    Elements::const_iterator elem = indexToIterator(index);
    return *elem;
}

Value* ArrayValue::element(const Value& index)
{
    return const_cast<Value*>(const_cast<const ArrayValue*>(this)->element(index));
}

void ArrayValue::setElement(const Value& indexValue, Value* value)
{
    const NumberValue* v = dynamic_cast<const NumberValue*>(&indexValue);
    if(!v)
    {
        /// @throw IllegalIndexError @a indexValue is not a NumberValue.
        throw IllegalIndexError("ArrayValue::setElement", "Array index must be a number");
    }
    replace(v->as<dint>(), value);
}

bool ArrayValue::contains(const Value& value) const
{
    for(Elements::const_iterator i = elements_.begin(); i != elements_.end(); ++i)
    {
        if(!(*i)->compare(value))
        {
            return true;
        }
    }
    return false;
}

Value* ArrayValue::begin()
{
    iteration_ = 0;
    return next();
}

Value* ArrayValue::next()
{
    if(iteration_ < dint(elements_.size()))
    {
        return elements_[iteration_++]->duplicate();
    }
    return 0;
}

bool ArrayValue::isTrue() const
{
    return elements_.size() > 0;
}

dint ArrayValue::compare(const Value& value) const
{
    const ArrayValue* other = dynamic_cast<const ArrayValue*>(&value);
    if(other)
    {
        if(size() < other->size())
        {
            return -1;
        }
        if(size() > other->size())
        {
            return 1;
        }
        // If all the keys and values compare equal, these are equal.
        Elements::const_iterator mine = elements_.begin();
        Elements::const_iterator theirs = other->elements_.begin();
        for(; mine != elements_.end() && theirs != other->elements_.end(); ++mine, ++theirs)
        {
            dint result = (*mine)->compare(**theirs);
            if(result) return result;
        }
        // These appear identical.
        return 0;
    }
    return Value::compare(value);    
}

void ArrayValue::sum(const Value& value)
{
    const ArrayValue* array = dynamic_cast<const ArrayValue*>(&value);
    if(!array)
    {
        /// @throw ArithmeticError @a value was not an Array. ArrayValue can only be summed
        /// with another ArrayValue.
        throw ArithmeticError("ArrayValue::sum", "Array cannot be summed with value");
    }
    
    for(Elements::const_iterator i = array->elements_.begin(); i != array->elements_.end(); ++i)
    {
        elements_.push_back((*i)->duplicate());
    }
}

void ArrayValue::add(Value* value)
{
    elements_.push_back(value);
}

void ArrayValue::add(const String& text)
{
    add(new TextValue(text));
}

const Value& ArrayValue::at(dint index) const
{
    return **indexToIterator(index);
}

ArrayValue::Elements::iterator ArrayValue::indexToIterator(dint index)
{
    if(index >= 0 && index < dint(size()))
    {
        return elements_.begin() + index;
    }
    else if(index < 0 && index > -dint(size()))
    {
        return elements_.begin() + size() + index;
    }
    else
    {
        /// @throw OutOfBoundsError @a index is out of bounds.
        throw OutOfBoundsError("ArrayValue::indexToIterator", "Index is out of bounds");
    }    
}

ArrayValue::Elements::const_iterator ArrayValue::indexToIterator(dint index) const
{
    if(index >= 0 && index < dint(size()))
    {
        return elements_.begin() + index;
    }
    else if(index < 0 && index >= -dint(size()))
    {
        return elements_.begin() + size() + index;
    }
    else
    {
        /// @throw OutOfBoundsError @a index is out of bounds.
        throw OutOfBoundsError("ArrayValue::indexToIterator", "Index is out of bounds");
    }    
}

void ArrayValue::insert(dint index, Value* value)
{
    if(index == dint(size()))
    {
        add(value);
    }
    else 
    {
        elements_.insert(indexToIterator(index), value);
    }
}

void ArrayValue::replace(dint index, Value* value)
{
    Elements::iterator elem = indexToIterator(index);
    delete *elem;
    *elem = value;
}
    
void ArrayValue::remove(dint index)
{
    Elements::iterator elem = indexToIterator(index);
    delete *elem;
    elements_.erase(elem);
}
    
Value* ArrayValue::pop()
{
    assert(size() > 0);
    Value* popped = elements_.back();
    elements_.pop_back();
    return popped;
}

void ArrayValue::reverse()
{
    std::reverse(elements_.begin(), elements_.end());
}

void ArrayValue::clear()
{
    // Delete the values.
    for(Elements::iterator i = elements_.begin(); i != elements_.end(); ++i)
    {
        delete *i;
    }
    elements_.clear();
}

void ArrayValue::operator >> (Writer& to) const
{
    to << SerialId(ARRAY) << duint(elements_.size());
    for(Elements::const_iterator i = elements_.begin(); i != elements_.end(); ++i)
    {
        to << **i;
    }
}

void ArrayValue::operator << (Reader& from)
{
    SerialId id;
    from >> id;
    if(id != ARRAY)
    {
        /// @throw DeserializationError The identifier that species the type of the 
        /// serialized value was invalid.
        throw DeserializationError("ArrayValue::operator <<", "Invalid ID");
    }
    // Number of elements.
    duint count = 0;
    from >> count;
    clear();
    while(count-- > 0)
    {
        add(Value::constructFrom(from));
    }
}
