/** @file timevalue.cpp Value that holds a Time.
 *
 * @authors Copyright © 2012-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/timevalue.h"
#include "de/numbervalue.h"
#include "de/reader.h"
#include "de/writer.h"
#include "de/log.h"

namespace de {

TimeValue::TimeValue(const Time &time) : _time(time)
{}

Value *TimeValue::duplicate() const
{
    return new TimeValue(_time);
}

Value::Text TimeValue::asText() const
{
    if (!_time.isValid()) return "(undefined Time)";
    return _time.asText();
}

bool TimeValue::isTrue() const
{
    return _time.isValid();
}

dint TimeValue::compare(const Value &value) const
{
    const TimeValue *other = dynamic_cast<const TimeValue *>(&value);
    if (other)
    {
        if (other->_time > _time) return 1;
        if (other->_time < _time) return -1;
        return 0;
    }
    return Value::compare(value);
}

void TimeValue::sum(const Value &value)
{
    _time += TimeSpan(value.asNumber());
}

void TimeValue::subtract(const Value &subtrahend)
{
    _time -= TimeSpan(subtrahend.asNumber());
}

void TimeValue::operator >> (Writer &to) const
{
    to << SerialId(TIME) << _time;
}

void TimeValue::operator << (Reader &from)
{
    SerialId id;
    from >> id;
    if (id != TIME)
    {
        throw DeserializationError("TimeValue::operator <<", "Invalid ID");
    }
    from >> _time;
}

Value::Text TimeValue::typeId() const
{
    return "Time";
}

} // namespace de
