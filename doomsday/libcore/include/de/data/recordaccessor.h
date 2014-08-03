/** @file recordaccessor.h  Utility class with get*() methods.
 *
 * @authors Copyright (c) 2014 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDENG2_RECORDACCESSOR_H
#define LIBDENG2_RECORDACCESSOR_H

#include "../ArrayValue"

namespace de {

class Record;

/**
 * Utility class with convenient get*() methods. While Record is designed to be used
 * primarily by Doomsday Script, RecordAccessor makes it easy for native code to access
 * the values stored in a Record.
 *
 * Record is derived from RecordAccessor, which makes these methods available in all
 * Record instances, too.
 *
 * @ingroup data
 */
class DENG2_PUBLIC RecordAccessor
{
public:
    /// Attempted to get the value of a variable while expecting the wrong type.
    /// @ingroup errors
    DENG2_ERROR(ValueTypeError);

public:
    RecordAccessor(Record const *rec);
    RecordAccessor(Record const &rec);

    Record const &accessedRecord() const;
    Record const *accessedRecordPtr() const;

    bool has(String const &name) const;
    Value const &get(String const &name) const;
    dint geti(String const &name) const;
    dint geti(String const &name, dint defaultValue) const;
    bool getb(String const &name) const;
    bool getb(String const &name, bool defaultValue) const;
    duint getui(String const &name) const;
    duint getui(String const &name, duint defaultValue) const;
    dfloat getf(String const &name) const;
    dfloat getf(String const &name, dfloat defaultValue) const;
    ddouble getd(String const &name) const;
    ddouble getd(String const &name, ddouble defaultValue) const;
    String gets(String const &name) const;
    String gets(String const &name, String const &defaultValue) const;
    ArrayValue const &geta(String const &name) const;

    template <typename ValueType>
    ValueType const &getAs(String const &name) const {
        ValueType const *v = get(name).maybeAs<ValueType>();
        if(!v)
        {
            throw ValueTypeError("RecordAccessor::getAs", String("Cannot cast to expected type (") +
                                 DENG2_TYPE_NAME(ValueType) + ")");
        }
        return *v;
    }

protected:
    void setAccessedRecord(Record const &rec);

private:
    Record const *_rec;
};

} // namespace de

#endif // LIBDENG2_RECORDACCESSOR_H
