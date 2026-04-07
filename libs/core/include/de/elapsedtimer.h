/** @file elapsedtimer.h
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

#ifndef LIBCORE_ELAPSEDTIMER_H
#define LIBCORE_ELAPSEDTIMER_H

#include "de/libcore.h"
#include <chrono>

namespace de {

class DE_PUBLIC ElapsedTimer
{
public:
    ElapsedTimer();

    bool isValid() const;
    void start();
    ddouble restart();
    ddouble elapsedSeconds() const;

private:
    using Clock = std::chrono::high_resolution_clock;
    Clock::time_point _startedAt;
};

} // namespace de

#endif // LIBCORE_ELAPSEDTIMER_H
