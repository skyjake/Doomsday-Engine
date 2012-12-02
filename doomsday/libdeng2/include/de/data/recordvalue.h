/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2009-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDENG2_RECORDVALUE_H
#define LIBDENG2_RECORDVALUE_H

#include "../Value"
#include "../Record"

#include <QFlags>

namespace de {

/**
 * References a Record. Operations done on a RecordValue are actually performed on
 * the record.
 *
 * @ingroup data
 */
class DENG2_PUBLIC RecordValue : public Value, DENG2_OBSERVES(Record, Deletion)
{
public:
    /// Attempt to access the record after it has been deleted. @ingroup errors
    DENG2_ERROR(NullError);

    /// An identifier that does not exist in the record was accessed. @ingroup errors
    DENG2_ERROR(NotFoundError);

    /// The index used for accessing the record is of the wrong type. @ingroup errors
    DENG2_ERROR(IllegalIndexError);

    /// The value does not own a record when expected to. @ingroup errors
    DENG2_ERROR(OwnershipError);

    enum OwnershipFlag
    {
        /// The value has ownership of the record.
        OwnsRecord = 0x1
    };
    Q_DECLARE_FLAGS(OwnershipFlags, OwnershipFlag)

public:
    /**
     * Constructs a new reference to a record.
     *
     * @param record     Record.
     * @param ownership  OwnsRecord, if the value is given ownership of @a record.
     */
    RecordValue(Record *record, OwnershipFlags ownership = 0);

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
    Record *record() const { return _record; }

    /**
     * Sets the record that the value is referencing.
     *
     * @param record  Record to reference. Ownership is not given.
     */
    void setRecord(Record *record);

    /**
     * Gives away ownership of the record, if the value owns the record.
     */
    Record *takeRecord();

    void verify() const;
    Record &dereference();
    Record const &dereference() const;

    Value *duplicate() const;
    Text asText() const;
    dsize size() const;
    Value *duplicateElement(Value const &value) const;
    bool contains(Value const &value) const;
    bool isTrue() const;
    dint compare(Value const &value) const;

    // Implements ISerializable.
    void operator >> (Writer &to) const;
    void operator << (Reader &from);

    // Observes Record deletion.
    void recordBeingDeleted(Record &record);

public:
    Record *_record;
    OwnershipFlags _ownership;
    OwnershipFlags _oldOwnership;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(RecordValue::OwnershipFlags)

} // namespace de

#endif /* LIBDENG2_RECORDVALUE_H */
