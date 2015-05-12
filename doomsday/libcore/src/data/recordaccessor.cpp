/** @file recordaccessor.cpp  Utility class with get*() methods.
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

#include "de/RecordAccessor"
#include "de/Record"

namespace de {

RecordAccessor::RecordAccessor(Record const *rec) : _rec(rec)
{}

RecordAccessor::RecordAccessor(Record const &rec) : _rec(&rec)
{}

Record const &RecordAccessor::accessedRecord() const
{
    DENG2_ASSERT(_rec != 0);
    return *_rec;
}

Record const *RecordAccessor::accessedRecordPtr() const
{
    return _rec;
}

bool RecordAccessor::has(String const &name) const
{
    return accessedRecord().has(name);
}

Value const &RecordAccessor::get(String const &name) const
{
    return accessedRecord()[name].value();
}

dint RecordAccessor::geti(String const &name) const
{
    return get(name).asInt();
}

dint RecordAccessor::geti(String const &name, dint defaultValue) const
{
    if(!accessedRecord().hasMember(name)) return defaultValue;
    return geti(name);
}

bool RecordAccessor::getb(String const &name) const
{
    return get(name).isTrue();
}

bool RecordAccessor::getb(String const &name, bool defaultValue) const
{
    if(!accessedRecord().hasMember(name)) return defaultValue;
    return getb(name);
}

duint RecordAccessor::getui(String const &name) const
{
    return duint(get(name).asNumber());
}

duint RecordAccessor::getui(String const &name, duint defaultValue) const
{
    if(!accessedRecord().hasMember(name)) return defaultValue;
    return getui(name);
}

dfloat RecordAccessor::getf(String const &name) const
{
    return dfloat(getd(name));
}

dfloat RecordAccessor::getf(String const &name, dfloat defaultValue) const
{
    if(!accessedRecord().hasMember(name)) return defaultValue;
    return getf(name);
}

ddouble RecordAccessor::getd(String const &name) const
{
    return get(name).asNumber();
}

ddouble RecordAccessor::getd(String const &name, ddouble defaultValue) const
{
    if(!accessedRecord().hasMember(name)) return defaultValue;
    return getd(name);
}

String RecordAccessor::gets(String const &name) const
{
    return get(name).asText();
}

String RecordAccessor::gets(String const &name, String const &defaultValue) const
{
    if(!accessedRecord().hasMember(name)) return defaultValue;
    return gets(name);
}

ArrayValue const &RecordAccessor::geta(String const &name) const
{
    return getAs<ArrayValue>(name);
}

Record const &RecordAccessor::subrecord(String const &name) const
{
    return accessedRecord().subrecord(name);
}

void RecordAccessor::setAccessedRecord(Record const &rec)
{
    _rec = &rec;
}

void RecordAccessor::setAccessedRecord(Record const *rec)
{
    _rec = rec;
}

} // namespace de
