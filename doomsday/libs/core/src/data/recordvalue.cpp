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

#include "de/recordvalue.h"
#include "de/textvalue.h"
#include "de/refvalue.h"
#include "de/nonevalue.h"
#include "de/scripting/functionvalue.h"
#include "de/scripting/process.h"
#include "de/scripting/context.h"
#include "de/scripting/evaluator.h"
#include "de/scripting/scopestatement.h"
#include "de/variable.h"
#include "de/writer.h"
#include "de/reader.h"
#include "de/math.h"

namespace de {

DE_PIMPL(RecordValue)
, DE_OBSERVES(Record, Deletion)
{
    Record *record = nullptr;
    OwnershipFlags ownership;
    OwnershipFlags oldOwnership; // prior to serialization

    Impl(Public *i) : Base(i) {}

    // Observes Record deletion.
    void recordBeingDeleted(Record &DE_DEBUG_ONLY(deleted))
    {
        if (!record) return; // Not associated with a record any more.

        DE_ASSERT(record == &deleted);
        DE_ASSERT(!ownership.testFlag(OwnsRecord));
        record = nullptr;
        self().setAccessedRecord(nullptr);
    }
};

RecordValue::RecordValue(Record *record, OwnershipFlags o)
    : RecordAccessor(record)
    , d(new Impl(this))
{
    d->record = record;
    d->ownership = o;
    d->oldOwnership = o;

    DE_ASSERT(d->record != nullptr);

    if (!d->ownership.testFlag(OwnsRecord) &&
        !d->record->flags().testFlag(Record::WontBeDeleted))
    {
        // If we don't own it, someone may delete the record.
        d->record->audienceForDeletion() += d;
    }
}

RecordValue::RecordValue(const Record &record)
    : RecordAccessor(record)
    , d(new Impl(this))
{
    d->record = const_cast<Record *>(&record);

    if (!record.flags().testFlag(Record::WontBeDeleted))
    {
        // Someone may delete the record.
        d->record->audienceForDeletion() += d;
    }
}

RecordValue::RecordValue(const IObject &object)
    : RecordAccessor(object.objectNamespace())
    , d(new Impl(this))
{
    d->record = const_cast<Record *>(&object.objectNamespace());

    if (!d->record->flags().testFlag(Record::WontBeDeleted))
    {
        // Someone may delete the record.
        d->record->audienceForDeletion() += d;
    }
}

RecordValue::~RecordValue()
{
    setRecord(nullptr);
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
    if (record == d->record) return; // Got it already.

    if (hasOwnership())
    {
        delete d->record;
    }
    else if (d->record && !d->record->flags().testFlag(Record::WontBeDeleted))
    {
        d->record->audienceForDeletion() -= d;
    }

    d->record = record;
    d->ownership = ownership;
    setAccessedRecord(d->record);

    if (d->record && !d->ownership.testFlag(OwnsRecord) &&
        !d->record->flags().testFlag(Record::WontBeDeleted))
    {
        // Since we don't own it, someone may delete the record.
        d->record->audienceForDeletion() += d;
    }
}

Record *RecordValue::takeRecord()
{
    verify();
    if (!hasOwnership())
    {
        /// @throw OwnershipError Cannot give away ownership of a record that is not owned.
        throw OwnershipError("RecordValue::takeRecord", "Value does not own the record");
    }
    Record *rec = d->record;
    DE_ASSERT(!d->record->audienceForDeletion().contains(d));
    d->record = nullptr;
    d->ownership = RecordNotOwned;
    setAccessedRecord(nullptr);
    return rec;
}

void RecordValue::verify() const
{
    if (!d->record)
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

const Record &RecordValue::dereference() const
{
    verify();
    return *d->record;
}

Value::Text RecordValue::typeId() const
{
    static String id("Record"); return id;
}

Value *RecordValue::duplicate() const
{
    verify();
    if (hasOwnership())
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
    if (d->record)
    {
        return d->record->asText();
    }
    return "(null)";
}

Record *RecordValue::memberScope() const
{
    verify();
    return d->record;
}

dsize RecordValue::size() const
{
    return dereference().members().size();
}

void RecordValue::setElement(const Value &index, Value *elementValue)
{
    // We're expecting text.
    const TextValue *text = dynamic_cast<const TextValue *>(&index);
    if (!text)
    {
        throw IllegalIndexError("RecordValue::setElement",
                                "Records must be indexed with text values");
    }
    dereference().add(new Variable(text->asText(), elementValue));
}

Value *RecordValue::duplicateElement(const Value &value) const
{
    // We're expecting text.
    const TextValue *text = dynamic_cast<const TextValue *>(&value);
    if (!text)
    {
        throw IllegalIndexError("RecordValue::duplicateElement",
                                "Records must be indexed with text values");
    }
    if (dereference().hasMember(*text))
    {
        return dereference()[*text].value().duplicateAsReference();
    }
    throw NotFoundError("RecordValue::duplicateElement",
                        "'" + text->asText() + "' does not exist in the record");
}

bool RecordValue::contains(const Value &value) const
{
    // We're expecting text.
    const TextValue *text = dynamic_cast<const TextValue *>(&value);
    if (!text)
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

dint RecordValue::compare(const Value &value) const
{
    const RecordValue *recValue = dynamic_cast<const RecordValue *>(&value);
    if (!recValue)
    {
        // Can't be the same.
        return cmp(reinterpret_cast<const void *>(this),
                   reinterpret_cast<const void *>(&value));
    }
    return cmp(recValue->d->record, d->record);
}

void RecordValue::call(Process &process, const Value &arguments, Value *) const
{
    verify();

    // Calling a record causes it to be treated as a class and a new record is
    // initialized as a member of the class.
    std::unique_ptr<RecordValue> instance(RecordValue::takeRecord(new Record));

    instance->record()->addSuperRecord(*d->record);

    // If there is an initializer method, call it now.
    if (dereference().hasMember(Record::VAR_INIT))
    {
        process.call(dereference().function(Record::VAR_INIT), arguments.as<ArrayValue>(),
                     instance->duplicateAsReference());

        // Discard the return value from the init function.
        delete process.context().evaluator().popResult();
    }

    process.context().evaluator().pushResult(instance.release());
}

// Flags for serialization:
static const duint8 OWNS_RECORD = 0x1;

void RecordValue::operator >> (Writer &to) const
{
    duint8 flags = 0;
    if (hasOwnership()) flags |= OWNS_RECORD;
    to << SerialId(RECORD) << flags << dereference();
}

void RecordValue::operator << (Reader &from)
{
    SerialId id;
    from >> id;
    if (id != RECORD)
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

RecordValue *RecordValue::takeRecord(Record *record)
{
    return new RecordValue(record, OwnsRecord);
}

RecordValue *RecordValue::takeRecord(Record &&record)
{
    return new RecordValue(new Record(record), OwnsRecord);
}

} // namespace de
