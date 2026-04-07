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

#ifndef LIBCORE_RECORDVALUE_H
#define LIBCORE_RECORDVALUE_H

#include "de/value.h"
#include "de/record.h"
#include "de/scripting/iobject.h"

namespace de {

/**
 * References a Record. Operations done on a RecordValue are actually performed on
 * the record.
 *
 * @ingroup data
 */
class DE_PUBLIC RecordValue
        : public Value
        , public RecordAccessor
{
public:
    /// Attempt to access the record after it has been deleted. @ingroup errors
    DE_ERROR(NullError);

    /// An identifier that does not exist in the record was accessed. @ingroup errors
    DE_ERROR(NotFoundError);

    /// The index used for accessing the record is of the wrong type. @ingroup errors
    DE_ERROR(IllegalIndexError);

    /// The value does not own a record when expected to. @ingroup errors
    DE_ERROR(OwnershipError);

    enum OwnershipFlag
    {
        /// The value has ownership of the record.
        OwnsRecord = 0x1,

        RecordNotOwned = 0
    };
    using OwnershipFlags = Flags;

public:
    /**
     * Constructs a new reference to a record.
     *
     * @param record     Record.
     * @param ownership  OwnsRecord, if the value is given ownership of @a record.
     */
    RecordValue(Record *record, OwnershipFlags ownership = 0);

    /**
     * Constructs a new (unowned) reference to a record.
     *
     * @param record     Record.
     */
    RecordValue(const Record &record);

    RecordValue(const IObject &object);

    virtual ~RecordValue();

    bool hasOwnership() const;

    /**
     * Determines if the value had ownership of the record prior to
     * serialization and deserialization.
     */
    bool usedToHaveOwnership() const;

    /**
     * Returns the record this reference points to.
     */
    Record *record() const;

    /**
     * Sets the record that the value is referencing.
     *
     * @param record     Record to reference. Ownership is not given.
     * @param ownership  OwnsRecord, if the value is given ownership of @a record.
     */
    void setRecord(Record *record, OwnershipFlags ownership = 0);

    /**
     * Gives away ownership of the record, if the value owns the record.
     */
    Record *takeRecord();

    void verify() const;
    Record &dereference();
    const Record &dereference() const;

    Text typeId() const;
    Value *duplicate() const;
    Value *duplicateAsReference() const;
    Text asText() const;
    Record *memberScope() const;
    dsize size() const;
    void setElement(const Value &index, Value *elementValue);
    Value *duplicateElement(const Value &value) const;
    bool contains(const Value &value) const;
    bool isTrue() const;
    dint compare(const Value &value) const;
    void call(Process &process, const Value &arguments, Value *self = 0) const;

    // Implements ISerializable.
    void operator >> (Writer &to) const;
    void operator << (Reader &from);

    RecordValue *duplicateUnowned() const;

    static RecordValue *takeRecord(Record *record);
    static RecordValue *takeRecord(Record &&record);

public:
    DE_PRIVATE(d)
};

} // namespace de

#endif /* LIBCORE_RECORDVALUE_H */
