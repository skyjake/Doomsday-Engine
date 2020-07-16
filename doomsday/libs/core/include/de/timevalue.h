/** @file timevalue.h Value that holds a Time.
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

#ifndef LIBCORE_TIMEVALUE_H
#define LIBCORE_TIMEVALUE_H

#include "de/value.h"
#include "de/time.h"

namespace de {

/**
 * Value that holds a Time.
 * @ingroup data
 */
class DE_PUBLIC TimeValue : public Value
{
public:
    TimeValue(const Time &time = Time());

    /// Returns the time of the value.
    Time time() const { return _time; }

    Text typeId() const;
    Value *duplicate() const;
    Text asText() const;
    bool isTrue() const;
    dint compare(const Value &value) const;
    void sum(const Value &value);
    void subtract(const Value &subtrahend);

    // Implements ISerializable.
    void operator >> (Writer &to) const;
    void operator << (Reader &from);

private:
    Time _time;
};

} // namespace de

#endif // LIBCORE_TIMEVALUE_H
