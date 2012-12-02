/** @file timevalue.h Value that holds a Time.
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

#ifndef LIBDENG2_TIMEVALUE_H
#define LIBDENG2_TIMEVALUE_H

#include "../Value"
#include "../Time"

namespace de {

/**
 * Value that holds a Time.
 */
class DENG2_PUBLIC TimeValue : public Value
{
public:
    TimeValue(Time const &time = Time());

    /// Returns the time of the value.
    Time time() const { return _time; }

    Value *duplicate() const;
    Text asText() const;
    bool isTrue() const;
    dint compare(Value const &value) const;
    void sum(Value const &value);
    void subtract(Value const &subtrahend);

    // Implements ISerializable.
    void operator >> (Writer &to) const;
    void operator << (Reader &from);

private:
    Time _time;
};

} // namespace de

#endif // LIBDENG2_TIMEVALUE_H
