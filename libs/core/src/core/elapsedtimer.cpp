/** @file elapsedtimer.cpp
 *
 * @authors Copyright (c) 2018 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/elapsedtimer.h"

namespace de {

ElapsedTimer::ElapsedTimer()
{}

bool ElapsedTimer::isValid() const
{
    return _startedAt.time_since_epoch() == Clock::duration::zero();
}

void ElapsedTimer::start()
{
    _startedAt = Clock::now();
}

ddouble ElapsedTimer::restart()
{
    const ddouble elapsed = elapsedSeconds();
    start();
    return elapsed;
}

ddouble ElapsedTimer::elapsedSeconds() const
{
    using namespace std::chrono;
    const auto delta = Clock::now() - _startedAt;
    const microseconds us = duration_cast<microseconds>(delta);
    return double(us.count()) / 1.0e6;
}

} // namespace de
