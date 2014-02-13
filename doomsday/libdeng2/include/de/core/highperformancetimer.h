/** @file highperformancetimer.h  Timer for performance-critical use.
 *
 * @authors Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDENG2_HIGHPERFORMANCETIMER_H
#define LIBDENG2_HIGHPERFORMANCETIMER_H

#include "../Time"

namespace de {

/**
 * Timer for high-performance use.
 */
class DENG2_PUBLIC HighPerformanceTimer
{
public:
    HighPerformanceTimer();

    /**
     * Returns the amount of time elapsed since the creation of the timer.
     */
    TimeDelta elapsed() const;

private:
    DENG2_PRIVATE(d)
};

} // namespace de

#endif // LIBDENG2_HIGHPERFORMANCETIMER_H
