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

#include "de/RecordValue"
#include "de/TextValue"
#include "de/RefValue"
#include "de/NoneValue"
#include "de/Variable"
#include "de/Writer"
#include "de/Reader"
#include "de/math.h"

using namespace de;

RecordValue::RecordValue(Record *record, OwnershipFlags o)
    : _record(record), _ownership(o), _oldOwnership(o)
{
    DENG2_ASSERT(_record != NULL);

    if(!_ownership.testFlag(OwnsRecord))
    {
        // If we don't own it, someone may delete the record.
        _record->audienceForDeletion += this;
    }
}

RecordValue::~RecordValue()
{
    setRecord(0);
}

bool RecordValue::hasOwnership() const
{
    return _ownership.testFlag(OwnsRecord);
}

bool RecordValue::usedToHaveOwnership() const
{
    return _oldOwnership.testFlag(OwnsRecord);
}

void RecordValue::setRecord(Record *record)
{
    if(record == _record) return; // Got it already.

    if(hasOwnership())
    {
        delete _record;
    }
    else if(_record)
    {
        _record->audienceForDeletion -= this;
    }

    _record = record;
    _ownership = 0;

    if(_record)
    {
        // Since we don't own it, someone may delete the record.
        _record->audienceForDeletion += this;
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
    Record *rec = _record;
    _record = 0;
    _ownership = 0;
    return rec;
}

void RecordValue::verify() const
{
    if(!_record)
    {
        /// @throw NullError The value no longer points to a record.
        throw NullError("RecordValue::verify", "Value no longer references a record");
    }
}

Record &RecordValue::dereference()
{
    verify();
    return *_record;
}

Record const &RecordValue::dereference() const
{
    verify();
    return *_record;
}

Value *RecordValue::duplicate() const
{
    verify();
    /// The return duplicated value does not own the record, just references it.
    return new RecordValue(_record);
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
        return dereference()[*text].value().duplicate();
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
    return cmp(recValue->_record, _record);
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
    _oldOwnership = OwnershipFlags(flags & OWNS_RECORD? OwnsRecord : 0);

    from >> dereference();
}

void RecordValue::recordBeingDeleted(Record &DENG2_DEBUG_ONLY(record))
{
    DENG2_ASSERT(_record == &record);
    DENG2_ASSERT(!_ownership.testFlag(OwnsRecord));
    _record = 0;
}
