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

#include "de/DictionaryValue"
#include "de/ArrayValue"
#include "de/Writer"
#include "de/Reader"

#include <sstream>
#include <string>

using namespace de;

DictionaryValue::DictionaryValue() : iteration_(0), validIteration_(false)
{}

DictionaryValue::DictionaryValue(const DictionaryValue& other) : iteration_(0), validIteration_(false)
{
    for(Elements::const_iterator i = other.elements_.begin(); i != other.elements_.end(); ++i)
    {
        Value* value = i->second->duplicate();
        elements_[ValueRef(i->first.value->duplicate())] = value;
    }
}

DictionaryValue::~DictionaryValue()
{
    clear();
}

void DictionaryValue::clear()
{
    for(Elements::iterator i = elements_.begin(); i != elements_.end(); ++i)
    {
        delete i->first.value;
        delete i->second;
    }
    elements_.clear();
}

void DictionaryValue::add(Value* key, Value* value)
{
    Elements::iterator existing = elements_.find(ValueRef(key));
    
    if(existing != elements_.end())
    {
        // Found it. Replace old value.
        delete existing->second;
        existing->second = value;
        
        // We already have the key, so the new one is unnecessary.
        delete key;
    }
    else
    {
        // Set new value.
        elements_[ValueRef(key)] = value;
    }
}

Value* DictionaryValue::duplicate() const
{
    return new DictionaryValue(*this);
}

Value::Text DictionaryValue::asText() const
{
    std::ostringstream s;
    s << "{";

    bool isFirst = true;

    // Compose a textual representation of the array elements.
    for(Elements::const_iterator i = elements_.begin(); i != elements_.end(); ++i)
    {
        if(!isFirst)
        {
            s << ",";
        }
        s << " " << i->first.value->asText() << ": " << i->second->asText();
        isFirst = false;
    }
    
    s << " }";
    return s.str();
}

dsize DictionaryValue::size() const
{
    return elements_.size();
}

const Value* DictionaryValue::element(const Value& index) const
{
    Elements::const_iterator i = elements_.find(ValueRef(&index));
    if(i == elements_.end())
    {
        throw KeyError("DictionaryValue::element",
            "Key '" + index.asText() + "' does not exist in the dictionary");
    }
    return i->second;
}

Value* DictionaryValue::element(const Value& index)
{
    return const_cast<Value*>(const_cast<const DictionaryValue*>(this)->element(index));
}

void DictionaryValue::setElement(const Value& index, Value* value)
{
    Elements::iterator i = elements_.find(ValueRef(&index));
    if(i == elements_.end())
    {
        // Add it to the dictionary.
        elements_[ValueRef(index.duplicate())] = value;
    }
    else
    {
        delete i->second;
        i->second = value;
    }
}

bool DictionaryValue::contains(const Value& value) const
{
    return elements_.find(ValueRef(&value)) != elements_.end();
}

Value* DictionaryValue::begin()
{
    validIteration_ = false;
    return next();
}

Value* DictionaryValue::next()
{
    if(!validIteration_)
    {
        iteration_ = elements_.begin();
        validIteration_ = true;
    }
    else if(iteration_ == elements_.end())
    {
        return 0;
    }
    ArrayValue* pair = new ArrayValue;
    pair->add(iteration_->first.value->duplicate());
    pair->add(iteration_->second->duplicate());
    ++iteration_;
    return pair;
}

bool DictionaryValue::isTrue() const
{
    return size() > 0;
}

dint DictionaryValue::compare(const Value& value) const
{
    const DictionaryValue* other = dynamic_cast<const DictionaryValue*>(&value);
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
            dint result = mine->first.value->compare(*theirs->first.value);
            if(result) return result;

            result = mine->second->compare(*theirs->second);
            if(result) return result;
        }
        // These appear identical.
        return 0;
    }
    return Value::compare(value);
}

void DictionaryValue::sum(const Value& value)
{
    const DictionaryValue* other = dynamic_cast<const DictionaryValue*>(&value);
    if(!other)
    {
        throw ArithmeticError("DictionaryValue::sum", "Values cannot be summed");
    }
    
    for(Elements::const_iterator i = other->elements_.begin();
        i != other->elements_.end(); ++i)
    {
        add(i->first.value->duplicate(), i->second->duplicate());
    }
}

void DictionaryValue::subtract(const Value& subtrahend)
{
    Elements::iterator i = elements_.find(ValueRef(&subtrahend));
    if(i == elements_.end())
    {
        throw KeyError("DictionaryValue::subtract",
            "Key '" + subtrahend.asText() + "' does not exist in the dictionary");
    }
    delete i->second;
    elements_.erase(i);
}

void DictionaryValue::operator >> (Writer& to) const
{
    to << SerialId(DICTIONARY) << duint(elements_.size());

    if(!elements_.empty())
    {
        for(Elements::const_iterator i = elements_.begin(); i != elements_.end(); ++i)
        {
            to << *i->first.value << *i->second;
        }
    }
}

void DictionaryValue::operator << (Reader& from)
{
    SerialId id;
    from >> id;
    if(id != DICTIONARY)
    {
        /// @throw DeserializationError The identifier that species the type of the 
        /// serialized value was invalid.
        throw DeserializationError("DictionaryValue::operator <<", "Invalid ID");
    }
    duint count = 0;
    from >> count;
    clear();
    while(count-- > 0)
    {
        add(Value::constructFrom(from), Value::constructFrom(from));
    }
}
