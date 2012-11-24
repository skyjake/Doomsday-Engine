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

#include "de/ArrayValue"
#include "de/NumberValue"
#include "de/TextValue"
#include "de/Writer"
#include "de/Reader"

#include <algorithm>
#include <QTextStream>

using namespace de;

ArrayValue::ArrayValue() : Value(), _iteration(0)
{}

ArrayValue::ArrayValue(ArrayValue const &other) : Value()
{
    for(Elements::const_iterator i = other._elements.begin();
        i != other._elements.end(); ++i)
    {
        _elements.push_back((*i)->duplicate());
    }
    _iteration = 0;
}

ArrayValue::~ArrayValue()
{
    clear();
}

Value *ArrayValue::duplicate() const
{
    return new ArrayValue(*this);
}

Value::Text ArrayValue::asText() const
{
    Text result;
    QTextStream s(&result);
    s << "[";

    bool isFirst = true;

    // Compose a textual representation of the array elements.
    for(Elements::const_iterator i = _elements.begin();
        i != _elements.end(); ++i)
    {
        if(!isFirst)
        {
            s << ",";
        }
        s << " " << (*i)->asText();
        isFirst = false;
    }
    
    s << " ]";
    return result;
}

dsize ArrayValue::size() const
{
    return _elements.size();
}

Value const &ArrayValue::element(Value const &indexValue) const
{
    NumberValue const *v = dynamic_cast<NumberValue const *>(&indexValue);
    if(!v)
    {
        /// @throw IllegalIndexError @a indexValue is not a NumberValue.
        throw IllegalIndexError("ArrayValue::element", "Array index must be a number");
    }
    dint index = v->as<dint>();
    Elements::const_iterator elem = indexToIterator(index);
    return **elem;
}

Value &ArrayValue::element(Value const &index)
{
    return const_cast<Value &>(const_cast<ArrayValue const *>(this)->element(index));
}

void ArrayValue::setElement(Value const &indexValue, Value *value)
{
    NumberValue const *v = dynamic_cast<NumberValue const *>(&indexValue);
    if(!v)
    {
        /// @throw IllegalIndexError @a indexValue is not a NumberValue.
        throw IllegalIndexError("ArrayValue::setElement", "Array index must be a number");
    }
    replace(v->as<dint>(), value);
}

bool ArrayValue::contains(Value const &value) const
{
    for(Elements::const_iterator i = _elements.begin(); i != _elements.end(); ++i)
    {
        if(!(*i)->compare(value))
        {
            return true;
        }
    }
    return false;
}

Value *ArrayValue::begin()
{
    _iteration = 0;
    return next();
}

Value *ArrayValue::next()
{
    if(_iteration < dint(_elements.size()))
    {
        return _elements[_iteration++]->duplicate();
    }
    return 0;
}

bool ArrayValue::isTrue() const
{
    return _elements.size() > 0;
}

dint ArrayValue::compare(Value const &value) const
{
    ArrayValue const *other = dynamic_cast<ArrayValue const *>(&value);
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
        Elements::const_iterator mine = _elements.begin();
        Elements::const_iterator theirs = other->_elements.begin();
        for(; mine != _elements.end() && theirs != other->_elements.end(); ++mine, ++theirs)
        {
            dint result = (*mine)->compare(**theirs);
            if(result) return result;
        }
        // These appear identical.
        return 0;
    }
    return Value::compare(value);    
}

void ArrayValue::sum(Value const &value)
{
    ArrayValue const *array = dynamic_cast<ArrayValue const *>(&value);
    if(!array)
    {
        /// @throw ArithmeticError @a value was not an Array. ArrayValue can only be summed
        /// with another ArrayValue.
        throw ArithmeticError("ArrayValue::sum", "Array cannot be summed with value");
    }
    
    for(Elements::const_iterator i = array->_elements.begin(); i != array->_elements.end(); ++i)
    {
        _elements.push_back((*i)->duplicate());
    }
}

void ArrayValue::add(Value *value)
{
    _elements.push_back(value);
}

void ArrayValue::add(String const &text)
{
    add(new TextValue(text));
}

Value const &ArrayValue::at(dint index) const
{
    return **indexToIterator(index);
}

ArrayValue::Elements::iterator ArrayValue::indexToIterator(dint index)
{
    if(index >= 0 && index < dint(size()))
    {
        return _elements.begin() + index;
    }
    else if(index < 0 && index > -dint(size()))
    {
        return _elements.begin() + size() + index;
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
        return _elements.begin() + index;
    }
    else if(index < 0 && index >= -dint(size()))
    {
        return _elements.begin() + size() + index;
    }
    else
    {
        /// @throw OutOfBoundsError @a index is out of bounds.
        throw OutOfBoundsError("ArrayValue::indexToIterator", "Index is out of bounds");
    }    
}

void ArrayValue::insert(dint index, Value *value)
{
    if(index == dint(size()))
    {
        add(value);
    }
    else 
    {
        _elements.insert(indexToIterator(index), value);
    }
}

void ArrayValue::replace(dint index, Value *value)
{
    Elements::iterator elem = indexToIterator(index);
    delete *elem;
    *elem = value;
}
    
void ArrayValue::remove(dint index)
{
    Elements::iterator elem = indexToIterator(index);
    delete *elem;
    _elements.erase(elem);
}
    
Value *ArrayValue::pop()
{
    DENG2_ASSERT(size() > 0);
    Value *popped = _elements.back();
    _elements.pop_back();
    return popped;
}

void ArrayValue::reverse()
{
    std::reverse(_elements.begin(), _elements.end());
}

void ArrayValue::clear()
{
    // Delete the values.
    for(Elements::iterator i = _elements.begin(); i != _elements.end(); ++i)
    {
        delete *i;
    }
    _elements.clear();
}

void ArrayValue::operator >> (Writer &to) const
{
    to << SerialId(ARRAY) << duint(_elements.size());
    for(Elements::const_iterator i = _elements.begin(); i != _elements.end(); ++i)
    {
        to << **i;
    }
}

void ArrayValue::operator << (Reader &from)
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
    while(count--)
    {
        add(Value::constructFrom(from));
    }
}
