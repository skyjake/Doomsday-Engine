/*
 * The Doomsday Engine Project -- libcore
 *
 * Copyright © 2009-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/variable.h"
#include "de/value.h"
#include "de/nonevalue.h"
#include "de/numbervalue.h"
#include "de/textvalue.h"
#include "de/arrayvalue.h"
#include "de/dictionaryvalue.h"
#include "de/blockvalue.h"
#include "de/timevalue.h"
#include "de/recordvalue.h"
#include "de/reader.h"
#include "de/writer.h"
#include "de/log.h"

namespace de {

DE_PIMPL_NOREF(Variable)
{
    String name;

    /// Value of the variable.
    Value *value = nullptr;

    /// Mode flags.
    Flags flags;

    Impl() = default;

    Impl(const Impl &other)
        : de::IPrivate()
        , name (other.name)
        , value(other.value->duplicate())
        , flags(other.flags)
    {}

    ~Impl()
    {
        delete value;
    }

    DE_PIMPL_AUDIENCE(Deletion)
    DE_PIMPL_AUDIENCE(Change)
    DE_PIMPL_AUDIENCE(ChangeFrom)
};

DE_AUDIENCE_METHOD(Variable, Deletion)
DE_AUDIENCE_METHOD(Variable, Change)
DE_AUDIENCE_METHOD(Variable, ChangeFrom)

Variable::Variable(const String &name, Value *initial, const Flags &m)
    : d(new Impl)
{
    std::unique_ptr<Value> v(initial);
    d->name = name;
    d->flags = m;
    verifyName(d->name);
    if (initial)
    {
        verifyValid(*v);
        d->value = v.release();
    }
}

Variable::Variable(const Variable &other)
    : d(new Impl(*other.d))
{}

Variable::~Variable()
{
    DE_NOTIFY(Deletion, i) i->variableBeingDeleted(*this);
}

const String &Variable::name() const
{
    return d->name;
}

Variable &Variable::operator = (Value *v)
{
    set(v);
    return *this;
}

Variable &Variable::operator = (const String &textValue)
{
    set(new TextValue(textValue));
    return *this;
}

Variable &Variable::set(Value *v)
{
    DE_ASSERT(v);

    std::unique_ptr<Value> val(v);

    // If the value would change, must check if this is allowed.
    verifyWritable(*v);
    verifyValid(*v);

    std::unique_ptr<Value> oldValue(d->value); // old value deleted afterwards
    d->value = val.release();
    d->flags |= ValueHasChanged;

    // We'll only determine if actual change occurred if someone is interested.
    if (!audienceForChange().isEmpty() || !audienceForChangeFrom().isEmpty())
    {
        bool notify;
        try
        {
            // Did it actually change? Let's compare...
            notify = !oldValue || oldValue->compare(*v);
        }
        catch (const Error &)
        {
            // Perhaps the values weren't comparable?
            notify = true;
        }

        if (notify)
        {
            DE_NOTIFY(Change,     i) i->variableValueChanged(*this, *d->value);
            DE_NOTIFY(ChangeFrom, i)
            {
                i->variableValueChangedFrom(
                    *this,
                    oldValue ? *oldValue : static_cast<const Value &>(NoneValue::none()),
                    *d->value);
            }
        }
    }
    return *this;
}

Variable &Variable::set(const Value &v)
{
    set(v.duplicate());
    return *this;
}

const Value &Variable::value() const
{
    if (!d->value) return NoneValue::none();
    return *d->value;
}

Value &Variable::value()
{
    return *valuePtr();
}

Value *Variable::valuePtr()
{
    if (!d->value)
    {
        d->value = new NoneValue;
    }
    return d->value;
}

const Value *Variable::valuePtr() const
{
    if (!d->value)
    {
        return &NoneValue::none();
    }
    return d->value;
}

bool Variable::operator==(const String &text) const
{
    return d->value && d->value->compare(TextValue(text)) == 0;
}

Record &Variable::valueAsRecord()
{
    return value<RecordValue>().dereference();
}

const Record &Variable::valueAsRecord() const
{
    return value<RecordValue>().dereference();
}

const ArrayValue &Variable::array() const
{
    return value<ArrayValue>();
}

ArrayValue &Variable::array()
{
    return value<ArrayValue>();
}

Variable::operator Record & ()
{
    return valueAsRecord();
}

Variable::operator const Record & () const
{
    return valueAsRecord();
}

Variable::operator String () const
{
    return value().asText();
}

Variable::operator ddouble () const
{
    return value().asNumber();
}

Flags Variable::flags() const
{
    return d->flags;
}

void Variable::setFlags(const Flags &flags, FlagOpArg operation)
{
    applyFlagOperation(d->flags, flags, operation);
}

Variable &Variable::setReadOnly()
{
    d->flags |= ReadOnly;
    return *this;
}

bool Variable::isValid(const Value &v) const
{
    /// @todo  Make sure this actually works and add func, record, ref.
    if ((!d->flags.testFlag(AllowNone)       && dynamic_cast<const NoneValue *>(&v)      ) ||
        (!d->flags.testFlag(AllowNumber)     && dynamic_cast<const NumberValue *>(&v)    ) ||
        (!d->flags.testFlag(AllowText)       && dynamic_cast<const TextValue *>(&v)      ) ||
        (!d->flags.testFlag(AllowArray)      && dynamic_cast<const ArrayValue *>(&v)     ) ||
        (!d->flags.testFlag(AllowDictionary) && dynamic_cast<const DictionaryValue *>(&v)) ||
        (!d->flags.testFlag(AllowBlock)      && dynamic_cast<const BlockValue *>(&v)     ) ||
        (!d->flags.testFlag(AllowTime)       && dynamic_cast<const TimeValue *>(&v)      )  )
    {
        return false;
    }
    // It's ok.
    return true;
}

void Variable::verifyValid(const Value &v) const
{
    if (!isValid(v))
    {
        /// @throw InvalidError  Value @a v is not allowed by the variable.
        throw InvalidError("Variable::verifyValid",
            "Value type is not allowed by the variable '" + d->name + "'");
    }
}

void Variable::verifyWritable(const Value &attemptedNewValue)
{
    if (d->flags & ReadOnly)
    {
        const Value *currentValue = d->value;
        if (d->value && typeid(currentValue) == typeid(attemptedNewValue) &&
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

void Variable::verifyName(const String &s)
{
    if (s.indexOf('.'))
    {
        /// @throw NameError The name cannot contain periods '.'.
        throw NameError("Variable::verifyName", "Name contains '.': " + s);
    }
}

void Variable::operator >> (Writer &to) const
{
    if (!(d->flags & NoSerialize))
    {
        to << d->name << duint32(d->flags) << value();
    }
}

void Variable::operator << (Reader &from)
{
    duint32 modeFlags = 0;
    from >> d->name >> modeFlags;
    d->flags = Flags(modeFlags);
    delete d->value;
    try
    {
        d->value = Value::constructFrom(from);
    }
    catch (...)
    {
        d->value = nullptr;
        throw;
    }
}

} // namespace de
