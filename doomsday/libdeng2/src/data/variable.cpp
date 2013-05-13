/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2009-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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
#include "de/TimeValue"
#include "de/RecordValue"
#include "de/Reader"
#include "de/Writer"
#include "de/Log"

using namespace de;

Variable::Variable(String const &name, Value *initial, Flags const &m)
    : _name(name), _value(0), _mode(m)
{
    std::auto_ptr<Value> v(initial);
    if(!initial)
    {
        v.reset(new NoneValue);
    }
    verifyName(_name);
    verifyValid(*v);
    _value = v.release();
}

Variable::Variable(Variable const &other) 
    : ISerializable(), _name(other._name), _value(other._value->duplicate()), _mode(other._mode)
{}

Variable::~Variable()
{
    DENG2_FOR_AUDIENCE(Deletion, i) i->variableBeingDeleted(*this);
    delete _value;
}

Variable &Variable::operator = (Value *v)
{
    set(v);
    return *this;
}

void Variable::set(Value *v)
{
    QScopedPointer<Value> val(v);
    // If the value would change, must check if this is allowed.
    verifyWritable(*v);
    verifyValid(*v);
    delete _value;
    _value = val.take();
    
    DENG2_FOR_AUDIENCE(Change, i) i->variableValueChanged(*this, *_value);
}

void Variable::set(Value const &v)
{
    verifyWritable(v);
    verifyValid(v);
    delete _value;
    _value = v.duplicate();

    DENG2_FOR_AUDIENCE(Change, i) i->variableValueChanged(*this, *_value);
}

Value const &Variable::value() const
{
    DENG2_ASSERT(_value != 0);
    return *_value;
}

Value &Variable::value()
{
    DENG2_ASSERT(_value != 0);
    return *_value;
}

Record const &Variable::valueAsRecord() const
{
    return value<RecordValue>().dereference();
}

Variable::operator Record const & () const
{
    return valueAsRecord();
}

Variable::operator String () const
{
    return value().asText();
}

Variable::operator QString () const
{
    return value().asText();
}

Variable::operator ddouble () const
{
    return value().asNumber();
}

Variable::Flags Variable::mode() const
{
    return _mode;
}

void Variable::setMode(Flags const &flags, FlagOp operation)
{
    applyFlagOperation(_mode, flags, operation);
}

Variable &Variable::setReadOnly()
{
    _mode |= ReadOnly;
    return *this;
}

bool Variable::isValid(Value const &v) const
{
    /// @todo  Make sure this actually works and add func, record, ref.
    if((dynamic_cast<NoneValue const *>(&v)       && !_mode.testFlag(AllowNone)) ||
       (dynamic_cast<NumberValue const *>(&v)     && !_mode.testFlag(AllowNumber)) ||
       (dynamic_cast<TextValue const *>(&v)       && !_mode.testFlag(AllowText)) ||
       (dynamic_cast<ArrayValue const *>(&v)      && !_mode.testFlag(AllowArray)) ||
       (dynamic_cast<DictionaryValue const *>(&v) && !_mode.testFlag(AllowDictionary)) ||
       (dynamic_cast<BlockValue const *>(&v)      && !_mode.testFlag(AllowBlock)) ||
       (dynamic_cast<TimeValue const *>(&v)       && !_mode.testFlag(AllowTime)))
    {
        return false;
    }
    // It's ok.
    return true;
}

void Variable::verifyValid(Value const &v) const
{
    if(!isValid(v))
    {
        /// @throw InvalidError  Value @a v is not allowed by the variable.
        throw InvalidError("Variable::verifyValid", 
            "Value type is not allowed by the variable '" + _name + "'");
    }
}

void Variable::verifyWritable(Value const &attemptedNewValue)
{
    if(_mode & ReadOnly)
    {
        if(_value && typeid(*_value) == typeid(attemptedNewValue) &&
           !_value->compare(attemptedNewValue))
        {
            // This is ok: the value doesn't change.
            return;
        }

        /// @throw ReadOnlyError  The variable is in read-only mode.
        throw ReadOnlyError("Variable::verifyWritable", 
            "Variable '" + _name + "' is in read-only mode");
    }
}

void Variable::verifyName(String const &s)
{
    if(s.indexOf('.') != String::npos)
    {
        /// @throw NameError The name cannot contain periods '.'.
        throw NameError("Variable::verifyName", "Name contains '.': " + s);
    }
}

void Variable::operator >> (Writer &to) const
{
    if(!_mode.testFlag(NoSerialize))
    {
        to << _name << duint32(_mode) << *_value;
    }
}

void Variable::operator << (Reader &from)
{
    duint32 modeFlags = 0;
    from >> _name >> modeFlags;
    _mode = Flags(modeFlags);
    delete _value;
    try
    {
        _value = Value::constructFrom(from);
    }
    catch(Error const &)
    {
        // Always need to have a value.
        _value = new NoneValue();
        throw;
    }    
}
