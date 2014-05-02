/*
 * The Doomsday Engine Project -- libcore
 *
 * Copyright © 2009-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

namespace de {

DENG2_PIMPL_NOREF(Variable)
{
    String name;

    /// Value of the variable.
    Value *value;

    /// Mode flags.
    Flags mode;

    Instance() : value(0) {}

    Instance(Instance const &other)
        : de::IPrivate()
        , name (other.name)
        , value(other.value->duplicate())
        , mode (other.mode)
    {}

    ~Instance()
    {
        delete value;
    }

    DENG2_PIMPL_AUDIENCE(Deletion)
    DENG2_PIMPL_AUDIENCE(Change)
};

DENG2_AUDIENCE_METHOD(Variable, Deletion)
DENG2_AUDIENCE_METHOD(Variable, Change)

Variable::Variable(String const &name, Value *initial, Flags const &m)
    : d(new Instance)
{
    d->name = name;
    d->mode = m;

    std::auto_ptr<Value> v(initial);
    if(!initial)
    {
        v.reset(new NoneValue);
    }
    verifyName(d->name);
    verifyValid(*v);
    d->value = v.release();
}

Variable::Variable(Variable const &other) 
    : ISerializable()
    , d(new Instance(*other.d))
{}

Variable::~Variable()
{
    DENG2_FOR_AUDIENCE2(Deletion, i) i->variableBeingDeleted(*this);
}

String const &Variable::name() const
{
    return d->name;
}

Variable &Variable::operator = (Value *v)
{
    set(v);
    return *this;
}

Variable &Variable::set(Value *v)
{
    DENG2_ASSERT(v != 0);

    QScopedPointer<Value> val(v);

    // If the value would change, must check if this is allowed.
    verifyWritable(*v);
    verifyValid(*v);

    QScopedPointer<Value> oldValue(d->value); // old value deleted afterwards
    d->value = val.take();
    
    // We'll only determine if actual change occurred if someone is interested.
    if(!audienceForChange().isEmpty())
    {
        bool notify = true;
        try
        {
            // Did it actually change? Let's compare...
            notify = oldValue.isNull() || oldValue->compare(*v);
        }
        catch(Error const &)
        {
            // Perhaps the values weren't comparable?
        }

        if(notify)
        {
            DENG2_FOR_AUDIENCE2(Change, i) i->variableValueChanged(*this, *d->value);
        }
    }
    return *this;
}

Variable &Variable::set(Value const &v)
{
    set(v.duplicate());
    return *this;
}

Value const &Variable::value() const
{
    DENG2_ASSERT(d->value != 0);
    return *d->value;
}

Value &Variable::value()
{
    DENG2_ASSERT(d->value != 0);
    return *d->value;
}

Value *Variable::valuePtr()
{
    return d->value;
}

Value const *Variable::valuePtr() const
{
    return d->value;
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
    return d->mode;
}

void Variable::setMode(Flags const &flags, FlagOp operation)
{
    applyFlagOperation(d->mode, flags, operation);
}

Variable &Variable::setReadOnly()
{
    d->mode |= ReadOnly;
    return *this;
}

bool Variable::isValid(Value const &v) const
{
    /// @todo  Make sure this actually works and add func, record, ref.
    if((dynamic_cast<NoneValue const *>(&v)       && !d->mode.testFlag(AllowNone)) ||
       (dynamic_cast<NumberValue const *>(&v)     && !d->mode.testFlag(AllowNumber)) ||
       (dynamic_cast<TextValue const *>(&v)       && !d->mode.testFlag(AllowText)) ||
       (dynamic_cast<ArrayValue const *>(&v)      && !d->mode.testFlag(AllowArray)) ||
       (dynamic_cast<DictionaryValue const *>(&v) && !d->mode.testFlag(AllowDictionary)) ||
       (dynamic_cast<BlockValue const *>(&v)      && !d->mode.testFlag(AllowBlock)) ||
       (dynamic_cast<TimeValue const *>(&v)       && !d->mode.testFlag(AllowTime)))
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
            "Value type is not allowed by the variable '" + d->name + "'");
    }
}

void Variable::verifyWritable(Value const &attemptedNewValue)
{
    if(d->mode & ReadOnly)
    {
        if(d->value && typeid(*d->value) == typeid(attemptedNewValue) &&
           !d->value->compare(attemptedNewValue))
        {
            // This is ok: the value doesn't change.
            return;
        }

        /// @throw ReadOnlyError  The variable is in read-only mode.
        throw ReadOnlyError("Variable::verifyWritable", 
            "Variable '" + d->name + "' is in read-only mode");
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
    if(!d->mode.testFlag(NoSerialize))
    {
        to << d->name << duint32(d->mode) << *d->value;
    }
}

void Variable::operator << (Reader &from)
{
    duint32 modeFlags = 0;
    from >> d->name >> modeFlags;
    d->mode = Flags(modeFlags);
    delete d->value;
    try
    {
        d->value = Value::constructFrom(from);
    }
    catch(Error const &)
    {
        // Always need to have a value.
        d->value = new NoneValue();
        throw;
    }    
}

} // namespace de
