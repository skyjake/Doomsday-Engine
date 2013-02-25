/** @file loop.cpp
 *
 * @authors Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/Loop"
#include "de/Time"
#include "de/Log"
#include "de/math.h"

#include <QTimer>

namespace de {

DENG2_PIMPL(Loop)
{
    TimeDelta interval;
    bool running;

    Instance(Public *i) : Base(i), interval(0), running(false) {}

    void schedule()
    {
        QTimer::singleShot(de::max(duint64(1), interval.asMilliSeconds()),
                           thisPublic, SLOT(nextLoopIteration()));
    }
};

Loop::Loop() : d(new Instance(this))
{}

Loop::~Loop()
{
    delete d;
}

void Loop::setRate(int freqHz)
{
    d->interval = 1.0 / freqHz;
}

void Loop::start()
{
    d->running = true;
    d->schedule();
}

void Loop::stop()
{
    d->running = false;
}

void Loop::nextLoopIteration()
{
    if(d->running)
    {
        d->schedule();
        DENG2_FOR_AUDIENCE(Iteration, i) i->loopIteration();
    }
}

} // namespace de
