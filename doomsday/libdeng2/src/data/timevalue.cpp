/** @file timevalue.cpp Value that holds a Time.
 * @ingroup data
 *
 * @authors Copyright © 2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#include "de/TimeValue"
#include "de/NumberValue"
#include "de/Reader"
#include "de/Writer"
#include "de/Log"

namespace de {

TimeValue::TimeValue(Time const &time) : _time(time)
{}

Value *TimeValue::duplicate() const
{
    return new TimeValue(_time);
}

Value::Text TimeValue::asText() const
{
    if(!_time.isValid()) return "(undefined Time)";
    return _time.asText();
}

bool TimeValue::isTrue() const
{
    return _time.isValid();
}

dint TimeValue::compare(Value const &value) const
{
    TimeValue const *other = dynamic_cast<TimeValue const *>(&value);
    if(other)
    {
        if(other->_time > _time) return 1;
        if(other->_time < _time) return -1;
        return 0;
    }
    return Value::compare(value);
}

void TimeValue::sum(Value const &value)
{
    _time += Time::Delta(value.asNumber());
}

void TimeValue::subtract(Value const &subtrahend)
{
    _time -= Time::Delta(subtrahend.asNumber());
}

void TimeValue::operator >> (Writer &to) const
{
    to << SerialId(TIME) << _time;
}

void TimeValue::operator << (Reader &from)
{
    SerialId id;
    from >> id;
    if(id != TIME)
    {
        throw DeserializationError("TimeValue::operator <<", "Invalid ID");
    }
    from >> _time;
}

} // namespace de
