/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2009, 2011 Jaakko Keränen <jaakko.keranen@iki.fi>
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

namespace de
{
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
        
        enum OwnershipFlag
        {
            /// The value has ownership of the record.
            OwnsRecord = 0x1,
        };
        Q_DECLARE_FLAGS(OwnershipFlags, OwnershipFlag);
        
    public:
        /**
         * Constructs a new reference to a record.
         *
         * @param record     Record.
         * @param ownership  OWNS_RECORD, if the value is given ownership of @a record.
         */
        RecordValue(Record* record, OwnershipFlags ownership = 0);
        
        virtual ~RecordValue();
        
        /**
         * Returns the record this reference points to.
         */
        Record* record() const { return _record; }

        void verify() const;
        Record& dereference();
        const Record& dereference() const;
        
        Value* duplicate() const;
        Text asText() const;
        dsize size() const;
        Value* duplicateElement(const Value& value) const;
        bool contains(const Value& value) const;
        bool isTrue() const;
        dint compare(const Value& value) const;

        // Implements ISerializable.
        void operator >> (Writer& to) const;
        void operator << (Reader& from);
        
        // Observes Record deletion.
        void recordBeingDeleted(Record& record);
        
    public:
        Record* _record;
        OwnershipFlags _ownership;
    };
}

Q_DECLARE_OPERATORS_FOR_FLAGS(de::RecordValue::OwnershipFlags);

#endif /* LIBDENG2_RECORDVALUE_H */
