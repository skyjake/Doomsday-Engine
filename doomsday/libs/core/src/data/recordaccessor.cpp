/** @file recordaccessor.cpp  Utility class with get*() methods.
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

#include "de/recordaccessor.h"
#include "de/recordvalue.h"
#include "de/dictionaryvalue.h"

namespace de {

RecordAccessor::RecordAccessor(const Record *rec) : _rec(rec)
{}

RecordAccessor::RecordAccessor(const Record &rec) : _rec(&rec)
{}

const Record &RecordAccessor::accessedRecord() const
{
    DE_ASSERT(_rec != 0);
    return *_rec;
}

const Record *RecordAccessor::accessedRecordPtr() const
{
    return _rec;
}

bool RecordAccessor::has(const String &name) const
{
    return accessedRecord().has(name);
}

const Value &RecordAccessor::get(const String &name) const
{
    return accessedRecord()[name].value();
}

dint RecordAccessor::geti(const String &name) const
{
    return get(name).asInt();
}

dint RecordAccessor::geti(const String &name, dint defaultValue) const
{
    if (!accessedRecord().hasMember(name)) return defaultValue;
    return geti(name);
}

bool RecordAccessor::getb(const String &name) const
{
    return get(name).isTrue();
}

bool RecordAccessor::getb(const String &name, bool defaultValue) const
{
    if (!accessedRecord().hasMember(name)) return defaultValue;
    return getb(name);
}

duint RecordAccessor::getui(const String &name) const
{
    return duint(get(name).asNumber());
}

duint RecordAccessor::getui(const String &name, duint defaultValue) const
{
    if (!accessedRecord().hasMember(name)) return defaultValue;
    return getui(name);
}

dfloat RecordAccessor::getf(const de::String &name) const
{
    return dfloat(getd(name));
}

dfloat RecordAccessor::getf(const String &name, dfloat defaultValue) const
{
    if (!accessedRecord().hasMember(name)) return defaultValue;
    return getf(name);
}

ddouble RecordAccessor::getd(const String &name) const
{
    return get(name).asNumber();
}

ddouble RecordAccessor::getd(const String &name, ddouble defaultValue) const
{
    if (!accessedRecord().hasMember(name)) return defaultValue;
    return getd(name);
}

String RecordAccessor::gets(const String &name) const
{
    return get(name).asText();
}

String RecordAccessor::gets(const String &name, const char *defaultValue) const
{
    if (!accessedRecord().hasMember(name)) return defaultValue;
    return gets(name);
}

const ArrayValue &RecordAccessor::geta(const String &name) const
{
    return getAs<ArrayValue>(name);
}

const DictionaryValue &RecordAccessor::getdt(const String &name) const
{
    return getAs<DictionaryValue>(name);
}

const RecordValue &RecordAccessor::getr(const String &name) const
{
    return getAs<RecordValue>(name);
}

StringList RecordAccessor::getStringList(const String &name, StringList defaultValue) const
{
    if (!accessedRecord().has(name)) return defaultValue;
    return get(name).asStringList();
}

const Record &RecordAccessor::subrecord(const String &name) const
{
    return accessedRecord().subrecord(name);
}

void RecordAccessor::setAccessedRecord(const Record &rec)
{
    _rec = &rec;
}

void RecordAccessor::setAccessedRecord(const Record *rec)
{
    _rec = rec;
}

} // namespace de
