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

#include "de/RecordValue"
#include "de/TextValue"
#include "de/RefValue"
#include "de/NoneValue"
#include "de/Variable"
#include "de/Writer"
#include "de/Reader"
#include "de/math.h"

namespace de {

DENG2_PIMPL_NOREF(RecordValue)
{
    Record *record;
    OwnershipFlags ownership;
    OwnershipFlags oldOwnership; // prior to serialization

    Instance() : record(0) {}
};

RecordValue::RecordValue(Record *record, OwnershipFlags o) : d(new Instance)
{
    d->record = record;
    d->ownership = o;
    d->oldOwnership = o;

    DENG2_ASSERT(d->record != NULL);

    if(!d->ownership.testFlag(OwnsRecord))
    {
        // If we don't own it, someone may delete the record.
        d->record->audienceForDeletion() += this;
    }
}

RecordValue::RecordValue(Record &record) : d(new Instance)
{
    d->record = &record;

    // Someone may delete the record.
    d->record->audienceForDeletion() += this;
}

RecordValue::~RecordValue()
{
    setRecord(0);
}

bool RecordValue::hasOwnership() const
{
    return d->ownership.testFlag(OwnsRecord);
}

bool RecordValue::usedToHaveOwnership() const
{
    return d->oldOwnership.testFlag(OwnsRecord);
}

Record *RecordValue::record() const
{
    return d->record;
}

void RecordValue::setRecord(Record *record, OwnershipFlags ownership)
{
    if(record == d->record) return; // Got it already.

    if(hasOwnership())
    {
        DENG2_ASSERT(!d->record->audienceForDeletion().contains(this));

        delete d->record;
    }
    else if(d->record)
    {
        DENG2_ASSERT(d->record->audienceForDeletion().contains(this));

        d->record->audienceForDeletion() -= this;
    }

    d->record = record;
    d->ownership = ownership;

    if(d->record && !d->ownership.testFlag(OwnsRecord))
    {
        // Since we don't own it, someone may delete the record.
        d->record->audienceForDeletion() += this;
    }
}

Record *RecordValue::takeRecord()
{
    verify();
    if(!hasOwnership())
    {
        /// @throw OwnershipError Cannot give away ownership of a record that is not owned.
        throw OwnershipError("RecordValue::takeRecord", "Value does not own the record");
    }
    Record *rec = d->record;
    d->record = 0;
    d->ownership = 0;
    return rec;
}

void RecordValue::verify() const
{
    if(!d->record)
    {
        /// @throw NullError The value no longer points to a record.
        throw NullError("RecordValue::verify", "Value no longer references a record");
    }
}

Record &RecordValue::dereference()
{
    verify();
    return *d->record;
}

Record const &RecordValue::dereference() const
{
    verify();
    return *d->record;
}

Value *RecordValue::duplicate() const
{
    verify();
    if(hasOwnership())
    {
        // Make a complete duplicate using a new record.
        return new RecordValue(new Record(*d->record), OwnsRecord);
    }
    return new RecordValue(d->record);
}

Value *RecordValue::duplicateAsReference() const
{
    verify();
    return new RecordValue(d->record);
}

Value::Text RecordValue::asText() const
{
    return dereference().asText();
}

dsize RecordValue::size() const
{
    return dereference().members().size();
}

void RecordValue::setElement(Value const &index, Value *elementValue)
{
    // We're expecting text.
    TextValue const *text = dynamic_cast<TextValue const *>(&index);
    if(!text)
    {
        throw IllegalIndexError("RecordValue::setElement",
                                "Records must be indexed with text values");
    }
    dereference().add(new Variable(text->asText(), elementValue));
}

Value *RecordValue::duplicateElement(Value const &value) const
{
    // We're expecting text.
    TextValue const *text = dynamic_cast<TextValue const *>(&value);
    if(!text)
    {
        throw IllegalIndexError("RecordValue::duplicateElement", 
                                "Records must be indexed with text values");
    }
    if(dereference().hasMember(*text))
    {
        return dereference()[*text].value().duplicateAsReference();
    }
    throw NotFoundError("RecordValue::duplicateElement",
                        "'" + text->asText() + "' does not exist in the record");
}

bool RecordValue::contains(Value const &value) const
{
    // We're expecting text.
    TextValue const *text = dynamic_cast<TextValue const *>(&value);
    if(!text)
    {
        throw IllegalIndexError("RecordValue::contains", 
                                "Records must be indexed with text values");
    }
    return dereference().has(*text);
}

bool RecordValue::isTrue() const
{
    return size() > 0;
}

dint RecordValue::compare(Value const &value) const
{
    RecordValue const *recValue = dynamic_cast<RecordValue const *>(&value);
    if(!recValue)
    {
        // Can't be the same.
        return cmp(reinterpret_cast<void const *>(this), 
                   reinterpret_cast<void const *>(&value));
    }
    return cmp(recValue->d->record, d->record);
}

// Flags for serialization:
static duint8 const OWNS_RECORD = 0x1;

void RecordValue::operator >> (Writer &to) const
{
    duint8 flags = 0;
    if(hasOwnership()) flags |= OWNS_RECORD;
    to << SerialId(RECORD) << flags << dereference();
}

void RecordValue::operator << (Reader &from)
{
    SerialId id;
    from >> id;
    if(id != RECORD)
    {
        /// @throw DeserializationError The identifier that species the type of the 
        /// serialized value was invalid.
        throw DeserializationError("RecordValue::operator <<", "Invalid ID");
    }

    // Old flags.
    duint8 flags = 0;
    from >> flags;
    d->oldOwnership = OwnershipFlags(flags & OWNS_RECORD? OwnsRecord : 0);

    from >> dereference();
}

void RecordValue::recordBeingDeleted(Record &DENG2_DEBUG_ONLY(record))
{
    if(!d->record) return; // Not associated with a record any more.

    DENG2_ASSERT(d->record == &record);
    DENG2_ASSERT(!d->ownership.testFlag(OwnsRecord));
    d->record = 0;
}

} // namespace de
