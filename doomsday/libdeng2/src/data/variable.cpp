/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2009, 2011 Jaakko Keränen <jaakko.keranen@iki.fi>
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

Variable::Variable(const String& name, Value* initial, const Flags& m)
    : _name(name), _value(0), _mode(m)
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
    : ISerializable(), _name(other._name), _value(other._value->duplicate()), _mode(other._mode)
{}

Variable::~Variable()
{
    DENG2_FOR_AUDIENCE(Deletion, i) i->variableBeingDeleted(*this);
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
    
    DENG2_FOR_AUDIENCE(Change, i) i->variableValueChanged(*this, *_value);
}

void Variable::set(const Value& v)
{
    verifyWritable();
    verifyValid(v);
    delete _value;
    _value = v.duplicate();

    DENG2_FOR_AUDIENCE(Change, i) i->variableValueChanged(*this, *_value);
}

const Value& Variable::value() const
{
    DENG2_ASSERT(_value != 0);
    return *_value;
}

Value& Variable::value()
{
    DENG2_ASSERT(_value != 0);
    return *_value;
}

Variable::Flags Variable::mode() const
{
    return _mode;
}

void Variable::setMode(const Flags& flags)
{
    _mode = flags;
}

bool Variable::isValid(const Value& v) const
{
    /// @todo  Make sure this actually works and add func, record, ref.
    if((dynamic_cast<const NoneValue*>(&v) && !_mode.testFlag(AllowNone)) ||
        (dynamic_cast<const NumberValue*>(&v) && !_mode.testFlag(AllowNumber)) ||
        (dynamic_cast<const TextValue*>(&v) && !_mode.testFlag(AllowText)) ||
        (dynamic_cast<const ArrayValue*>(&v) && !_mode.testFlag(AllowArray)) ||
        (dynamic_cast<const DictionaryValue*>(&v) && !_mode.testFlag(AllowDictionary)) ||
        (dynamic_cast<const BlockValue*>(&v) && !_mode.testFlag(AllowBlock)))
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
    if(_mode & ReadOnly)
    {
        /// @throw ReadOnlyError  The variable is in read-only mode.
        throw ReadOnlyError("Variable::verifyWritable", 
            "Variable '" + _name + "' is in read-only mode");
    }
}

void Variable::verifyName(const String& s)
{
    if(s.indexOf('.') != String::npos)
    {
        /// @throw NameError The name cannot contain periods '.'.
        throw NameError("Variable::verifyName", "Name contains '.': " + s);
    }
}

void Variable::operator >> (Writer& to) const
{
    if(!_mode.testFlag(NoSerialize))
    {
        to << _name << duint32(_mode) << *_value;
    }
}

void Variable::operator << (Reader& from)
{
    duint32 modeFlags = 0;
    from >> _name >> modeFlags;
    _mode = Flags(modeFlags);
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
