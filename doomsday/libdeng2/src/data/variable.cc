/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2009, 2010 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/Variable"
#include "de/Value"
#include "de/NoneValue"
#include "de/NumberValue"
#include "de/TextValue"
#include "de/ArrayValue"
#include "de/DictionaryValue"
#include "de/BlockValue"
#include "de/Reader"
#include "de/Writer"

using namespace de;

Variable::Variable(const String& name, Value* initial, const Mode& m)
    : mode(m), _name(name), _value(0)
{
    verifyName(_name);
    if(!initial)
    {
        initial = new NoneValue();
    }
    std::auto_ptr<Value> v(initial);
    verifyValid(*initial);
    _value = v.release();
}

Variable::Variable(const Variable& other) 
    : mode(other.mode), _name(other._name), _value(other._value->duplicate())
{}

Variable::~Variable()
{
    FOR_AUDIENCE(Deletion, i) i->variableBeingDeleted(*this);
    delete _value;
}

Variable& Variable::operator = (Value* v)
{
    set(v);
    return *this;
}

void Variable::set(Value* v)
{
    std::auto_ptr<Value> val(v);
    verifyWritable();
    verifyValid(*v);
    delete _value;
    _value = val.release();
    
    FOR_AUDIENCE(Change, i) i->variableValueChanged(*this, *_value);
}

void Variable::set(const Value& v)
{
    verifyWritable();
    verifyValid(v);
    delete _value;
    _value = v.duplicate();

    FOR_AUDIENCE(Change, i) i->variableValueChanged(*this, *_value);
}

const Value& Variable::value() const
{
    assert(_value != 0);
    return *_value;
}

Value& Variable::value()
{
    assert(_value != 0);
    return *_value;
}

bool Variable::isValid(const Value& v) const
{
    /// @todo  Make sure this actually works and add func, record, ref.
    if((dynamic_cast<const NoneValue*>(&v) && !mode[NONE_BIT]) ||
        (dynamic_cast<const NumberValue*>(&v) && !mode[NUMBER_BIT]) ||
        (dynamic_cast<const TextValue*>(&v) && !mode[TEXT_BIT]) ||
        (dynamic_cast<const ArrayValue*>(&v) && !mode[ARRAY_BIT]) ||
        (dynamic_cast<const DictionaryValue*>(&v) && !mode[DICTIONARY_BIT]) ||
        (dynamic_cast<const BlockValue*>(&v) && !mode[BLOCK_BIT]))
    {
        return false;
    }
    // It's ok.
    return true;
}

void Variable::verifyValid(const Value& v) const
{
    if(!isValid(v))
    {
        /// @throw InvalidError  Value @a v is not allowed by the variable.
        throw InvalidError("Variable::verifyValid", 
            "Value type is not allowed by the variable '" + _name + "'");
    }
}

void Variable::verifyWritable()
{
    if(mode[READ_ONLY_BIT])
    {
        /// @throw ReadOnlyError  The variable is in read-only mode.
        throw ReadOnlyError("Variable::verifyWritable", 
            "Variable '" + _name + "' is in read-only mode");
    }
}

void Variable::verifyName(const String& s)
{
    if(s.find('.') != String::npos)
    {
        /// @throw NameError The name cannot contain periods '.'.
        throw NameError("Variable::verifyName", "Name contains '.': " + s);
    }
}

void Variable::operator >> (Writer& to) const
{
    if(!mode[NO_SERIALIZE_BIT])
    {
        to << _name << duint32(mode.to_ulong()) << *_value;
    }
}

void Variable::operator << (Reader& from)
{
    duint32 modeFlags = 0;
    from >> _name >> modeFlags;
    mode = modeFlags;
    delete _value;
    try
    {
        _value = Value::constructFrom(from);
    }
    catch(const Error& err)
    {
        // Always need to have a value.
        _value = new NoneValue();
        err.raise();
    }    
}
