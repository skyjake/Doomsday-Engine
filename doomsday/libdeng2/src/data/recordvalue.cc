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

#include "de/RecordValue"
#include "de/TextValue"
#include "de/RefValue"
#include "de/NoneValue"
#include "de/Writer"
#include "de/Reader"
#include "de/math.h"

using namespace de;

RecordValue::RecordValue(Record* record, const Ownership& o) : record_(record), ownership_(o)
{
    assert(record_ != NULL);
    if(!ownership_[OWNS_RECORD_BIT])
    {
        // If we don't own it, someone may delete the record.
        record_->audienceForDeletion.add(this);
    }
}

RecordValue::~RecordValue()
{
    if(ownership_[OWNS_RECORD_BIT])
    {
        delete record_;
    }
    else if(record_)
    {
        record_->audienceForDeletion.remove(this);
    }
    
}

void RecordValue::verify() const
{
    if(!record_)
    {
        /// @throw NullError The value no longer points to a record.
        throw NullError("RecordValue::verify", "Value no longer references a record");
    }
}

Record& RecordValue::dereference()
{
    verify();
    return *record_;
}

const Record& RecordValue::dereference() const
{
    verify();
    return *record_;
}

Value* RecordValue::duplicate() const
{
    verify();
    return new RecordValue(record_);
}

Value::Text RecordValue::asText() const
{
    return dereference().asText();
}

dsize RecordValue::size() const
{
    return dereference().members().size() + dereference().subrecords().size();
}

Value* RecordValue::duplicateElement(const Value& value) const
{
    // We're expecting text.
    const TextValue* text = dynamic_cast<const TextValue*>(&value);
    if(!text)
    {
        throw IllegalIndexError("RecordValue::duplicateElement", 
            "Records must be indexed with text values");
    }
    if(dereference().hasMember(*text))
    {
        return new RefValue(const_cast<Variable*>(&dereference()[*text]));
    }
    if(dereference().hasSubrecord(*text))
    {
        return new RecordValue(const_cast<Record*>(&dereference().subrecord(*text)));
    }
    throw NotFoundError("RecordValue::duplicateElement",
        "'" + text->asText() + "' does not exist in the record");
}

bool RecordValue::contains(const Value& value) const
{
    // We're expecting text.
    const TextValue* text = dynamic_cast<const TextValue*>(&value);
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

dint RecordValue::compare(const Value& value) const
{
    const RecordValue* recValue = dynamic_cast<const RecordValue*>(&value);
    if(!recValue)
    {
        // Can't be the same.
        return cmp(reinterpret_cast<const void*>(this), 
                   reinterpret_cast<const void*>(&value));
    }
    return cmp(recValue->record_, record_);
}

void RecordValue::operator >> (Writer& to) const
{
    to << SerialId(RECORD) << dereference();
}

void RecordValue::operator << (Reader& from)
{
    SerialId id;
    from >> id;
    if(id != RECORD)
    {
        /// @throw DeserializationError The identifier that species the type of the 
        /// serialized value was invalid.
        throw DeserializationError("RecordValue::operator <<", "Invalid ID");
    }
    from >> dereference();
}

void RecordValue::recordBeingDeleted(Record& record)
{
    assert(record_ == &record);
    assert(!ownership_[OWNS_RECORD_BIT]);
    record_ = 0;
}
