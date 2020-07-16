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

#include "de/arrayvalue.h"

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
    RecordAccessor(const Record *rec);
    RecordAccessor(const Record &rec);

    const Record &accessedRecord() const;
    const Record *accessedRecordPtr() const;

    bool has(const String &name) const;
    const Value &get(const String &name) const;
    dint geti(const String &name) const;
    dint geti(const String &name, dint defaultValue) const;
    bool getb(const String &name) const;
    bool getb(const String &name, bool defaultValue) const;
    duint getui(const String &name) const;
    duint getui(const String &name, duint defaultValue) const;
    dfloat getf(const String &name) const;
    dfloat getf(const String &name, dfloat defaultValue) const;
    ddouble getd(const String &name) const;
    ddouble getd(const String &name, ddouble defaultValue) const;
    String gets(const String &name) const;
    String gets(const String &name, const char *defaultValue) const;
    const ArrayValue &geta(const String &name) const;
    const DictionaryValue &getdt(const String &name) const;
    const RecordValue &getr(const String &name) const;
    StringList getStringList(const String &name, StringList defaultValue = StringList()) const;

    const Record &subrecord(const String &name) const;

    template <typename ValueType>
    const ValueType &getAs(const String &name) const {
        const ValueType *v = maybeAs<ValueType>(get(name));
        if (!v) {
            throw ValueTypeError("RecordAccessor::getAs", String("Cannot cast to expected type (") +
                                 DE_TYPE_NAME(ValueType) + " const)");
        }
        return *v;
    }

protected:
    void setAccessedRecord(const Record &rec);
    void setAccessedRecord(const Record *rec);

private:
    const Record *_rec;
};

} // namespace de

#endif // LIBCORE_RECORDACCESSOR_H
