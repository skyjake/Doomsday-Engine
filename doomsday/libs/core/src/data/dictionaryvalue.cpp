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

#include "de/dictionaryvalue.h"
#include "de/arrayvalue.h"
#include "de/recordvalue.h"
#include "de/scripting/scriptsystem.h"
#include "de/writer.h"
#include "de/reader.h"

#include <sstream>

namespace de {

DictionaryValue::DictionaryValue() : /*_iteration(0),*/ _validIteration(false)
{}

DictionaryValue::DictionaryValue(const DictionaryValue &other)
    : Value(), /*_iteration(0),*/ _validIteration(false)
{
    for (Elements::const_iterator i = other._elements.begin(); i != other._elements.end(); ++i)
    {
        Value *value = i->second->duplicate();
        _elements[ValueRef(i->first.value->duplicate())] = value;
    }
}

DictionaryValue::~DictionaryValue()
{
    clear();
}

void DictionaryValue::clear()
{
    for (Elements::iterator i = _elements.begin(); i != _elements.end(); ++i)
    {
        delete i->first.value;
        delete i->second;
    }
    _elements.clear();
}

void DictionaryValue::add(Value *key, Value *value)
{
    Elements::iterator existing = _elements.find(ValueRef(key));

    if (existing != _elements.end())
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
        _elements[ValueRef(key)] = value;
    }
}

void DictionaryValue::remove(const Value &key)
{
    Elements::iterator i = _elements.find(ValueRef(&key));
    if (i != _elements.end())
    {
        remove(i);
    }
}

void DictionaryValue::remove(const Elements::iterator &pos)
{
    delete pos->first.value;
    delete pos->second;
    _elements.erase(pos);
}

const Value *DictionaryValue::find(const Value &key) const
{
    auto i = _elements.find(ValueRef(&key));
    if (i != _elements.end())
    {
        return i->second;
    }
    return nullptr;
}

ArrayValue *DictionaryValue::contentsAsArray(ContentSelection selection) const
{
    std::unique_ptr<ArrayValue> array(new ArrayValue);
    DE_FOR_EACH_CONST(Elements, i, elements())
    {
        if (selection == Keys)
        {
            array->add(i->first.value->duplicateAsReference());
        }
        else
        {
            array->add(i->second->duplicateAsReference());
        }
    }
    return array.release();
}

Record DictionaryValue::toRecord() const
{
    Record rec;
    for (auto i = _elements.begin(); i != _elements.end(); ++i)
    {
        Variable &var = rec.add(i->first.value->asText());
        var.set(i->second->duplicate());
    }
    return rec;
}

Value::Text DictionaryValue::typeId() const
{
    return "Dictionary";
}

Value *DictionaryValue::duplicate() const
{
    return new DictionaryValue(*this);
}

Value::Text DictionaryValue::asText() const
{
    std::ostringstream s;
    s << "{";

    bool isFirst = true;
    bool hadNewline = false;

    // Compose a textual representation of the array elements.
    for (auto i = _elements.begin(); i != _elements.end(); ++i)
    {
        const String label = i->first.value->asText() + ": ";
        String content = i->second->asText();
        const bool multiline = content.contains("\n");
        if (!isFirst)
        {
            if (hadNewline || multiline) s << "\n";
            s << ",";
        }
        hadNewline = multiline;
        s << " " << label << content.replace("\n", "\n" + String(label.size() + 2, ' '));
        isFirst = false;
    }

    s << " }";
    return s.str();
}

Record *DictionaryValue::memberScope() const
{
    return &ScriptSystem::builtInClass("Dictionary");
}

dsize DictionaryValue::size() const
{
    return _elements.size();
}

const Value &DictionaryValue::element(const Value &index) const
{
    Elements::const_iterator i = _elements.find(ValueRef(&index));
    if (i == _elements.end())
    {
        throw KeyError("DictionaryValue::element",
                       "Key '" + index.asText() + "' does not exist in the dictionary");
    }
    return *i->second;
}

Value &DictionaryValue::element(const Value &index)
{
    return const_cast<Value &>(const_cast<const DictionaryValue *>(this)->element(index));
}

void DictionaryValue::setElement(const Value &index, Value *value)
{
    Elements::iterator i = _elements.find(ValueRef(&index));
    if (i == _elements.end())
    {
        // Add it to the dictionary.
        _elements[ValueRef(index.duplicate())] = value;
    }
    else
    {
        delete i->second;
        i->second = value;
    }
}

bool DictionaryValue::contains(const Value &value) const
{
    return _elements.find(ValueRef(&value)) != _elements.end();
}

Value *DictionaryValue::begin()
{
    _validIteration = false;
    return next();
}

Value *DictionaryValue::next()
{
    if (!_validIteration)
    {
        _iteration = _elements.begin();
        _validIteration = true;
    }
    else if (_iteration == _elements.end())
    {
        return 0;
    }
    ArrayValue *pair = new ArrayValue;
    pair->add(_iteration->first.value->duplicate());
    pair->add(_iteration->second->duplicate());
    ++_iteration;
    return pair;
}

bool DictionaryValue::isTrue() const
{
    return size() > 0;
}

dint DictionaryValue::compare(const Value &value) const
{
    const DictionaryValue *other = dynamic_cast<const DictionaryValue *>(&value);
    if (other)
    {
        if (size() < other->size())
        {
            return -1;
        }
        if (size() > other->size())
        {
            return 1;
        }
        // If all the keys and values compare equal, these are equal.
        auto mine = _elements.begin();
        auto theirs = other->_elements.begin();
        for (; mine != _elements.end() && theirs != other->_elements.end(); ++mine, ++theirs)
        {
            dint result = mine->first.value->compare(*theirs->first.value);
            if (result) return result;

            result = mine->second->compare(*theirs->second);
            if (result) return result;
        }
        // These appear identical.
        return 0;
    }
    return Value::compare(value);
}

void DictionaryValue::sum(const Value &value)
{
    const DictionaryValue *other = dynamic_cast<const DictionaryValue *>(&value);
    if (!other)
    {
        throw ArithmeticError("DictionaryValue::sum", "Values cannot be summed");
    }

    for (auto i = other->_elements.begin(); i != other->_elements.end(); ++i)
    {
        add(i->first.value->duplicate(), i->second->duplicate());
    }
}

void DictionaryValue::subtract(const Value &subtrahend)
{
    Elements::iterator i = _elements.find(ValueRef(&subtrahend));
    if (i == _elements.end())
    {
        throw KeyError("DictionaryValue::subtract",
            "Key '" + subtrahend.asText() + "' does not exist in the dictionary");
    }
    remove(i);
}

void DictionaryValue::operator >> (Writer &to) const
{
    to << SerialId(DICTIONARY) << duint(_elements.size());

    if (!_elements.empty())
    {
        for (auto i = _elements.begin(); i != _elements.end(); ++i)
        {
            to << *i->first.value << *i->second;
        }
    }
}

void DictionaryValue::operator << (Reader &from)
{
    SerialId id;
    from >> id;
    if (id != DICTIONARY)
    {
        /// @throw DeserializationError The identifier that species the type of the
        /// serialized value was invalid.
        throw DeserializationError("DictionaryValue::operator <<", "Invalid ID");
    }
    duint count = 0;
    from >> count;
    clear();
    while (count--)
    {
        std::unique_ptr<Value> key(Value::constructFrom(from));
        std::unique_ptr<Value> value(Value::constructFrom(from));
        add(key.release(), value.release());
    }
}

} // namespace de
