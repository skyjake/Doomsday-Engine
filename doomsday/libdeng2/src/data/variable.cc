/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2009 Jaakko Keränen <jaakko.keranen@iki.fi>
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
    : mode(m), name_(name), value_(0)
{
    verifyName(name_);
    if(!initial)
    {
        initial = new NoneValue();
    }
    std::auto_ptr<Value> v(initial);
    verifyValid(*initial);
    value_ = v.release();
}

Variable::Variable(const Variable& other) 
    : mode(other.mode), name_(other.name_), value_(other.value_->duplicate())
{}

Variable::~Variable()
{
    FOR_EACH_OBSERVER(o, observers) o->variableBeingDeleted(*this);
    delete value_;
}

Variable& Variable::operator = (Value* v)
{
    set(v);
    return *this;
}

void Variable::set(Value* v)
{
    std::auto_ptr<Value> val(v);
    verifyValid(*v);
    delete value_;
    value_ = val.release();
    
    FOR_EACH_OBSERVER(o, observers) o->variableValueChanged(*this, *value_);
}

void Variable::set(const Value& v)
{
    verifyValid(v);
    delete value_;
    value_ = v.duplicate();

    FOR_EACH_OBSERVER(o, observers) o->variableValueChanged(*this, *value_);
}

const Value& Variable::value() const
{
    assert(value_ != 0);
    return *value_;
}

Value& Variable::value()
{
    assert(value_ != 0);
    return *value_;
}

bool Variable::isValid(const Value& v) const
{
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
        throw InvalidError("Variable::verifyValid", 
            "Value type is not allowed by the variable '" + name_ + "'");
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
        to << name_ << duint32(mode.to_ulong()) << *value_;
    }
}

void Variable::operator << (Reader& from)
{
    duint32 modeFlags = 0;
    from >> name_ >> modeFlags;
    mode = modeFlags;
    delete value_;
    try
    {
        value_ = Value::constructFrom(from);
    }
    catch(const Error& err)
    {
        // Always need to have a value.
        value_ = new NoneValue();
        err.raise();
    }    
}
