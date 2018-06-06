/** @file recordaccessor.h  Utility class with get*() methods.
 *
 * @authors Copyright (c) 2014-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBCORE_RECORDACCESSOR_H
#define LIBCORE_RECORDACCESSOR_H

#include "../ArrayValue"

namespace de {

class Record;
class RecordValue;
class DictionaryValue;

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
class DE_PUBLIC RecordAccessor
{
public:
    /// Attempted to get the value of a variable while expecting the wrong type.
    /// @ingroup errors
    DE_ERROR(ValueTypeError);

public:
    RecordAccessor(Record const *rec);
    RecordAccessor(Record const &rec);

    Record const &accessedRecord() const;
    Record const *accessedRecordPtr() const;

    bool has(const char *name) const;
    Value const &get(const char *name) const;
    dint geti(const char *name) const;
    dint geti(const char *name, dint defaultValue) const;
    bool getb(const char *name) const;
    bool getb(const char *name, bool defaultValue) const;
    duint getui(const char *name) const;
    duint getui(const char *name, duint defaultValue) const;
    dfloat getf(const char *name) const;
    dfloat getf(const char *name, dfloat defaultValue) const;
    ddouble getd(const char *name) const;
    ddouble getd(const char *name, ddouble defaultValue) const;
    String gets(const char *name) const;
    String gets(const char *name, String const &defaultValue) const;
    ArrayValue const &geta(const char *name) const;
    DictionaryValue const &getdt(const char *name) const;
    RecordValue const &getr(const char *name) const;
    StringList getStringList(const char *name, StringList defaultValue = StringList()) const;

    Record const &subrecord(const char *name) const;

    template <typename ValueType>
    ValueType const &getAs(const char *name) const {
        ValueType const *v = maybeAs<ValueType>(get(name));
        if (!v) {
            throw ValueTypeError("RecordAccessor::getAs", String("Cannot cast to expected type (") +
                                 DE_TYPE_NAME(ValueType) + " const)");
        }
        return *v;
    }

protected:
    void setAccessedRecord(Record const &rec);
    void setAccessedRecord(Record const *rec);

private:
    Record const *_rec;
};

} // namespace de

#endif // LIBCORE_RECORDACCESSOR_H
