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
#include "de/App"
#include "de/Time"
#include "de/Log"
#include "de/math.h"

#include "callbacktimer.h"

#include <QCoreApplication>
#include <QTimer>

namespace de {

static Loop *loopSingleton = 0;

DENG2_PIMPL(Loop)
{
    TimeDelta interval;
    bool running;
    QTimer *timer;

    Instance(Public *i) : Base(i), interval(0), running(false)
    {
        DENG2_ASSERT(!loopSingleton);
        loopSingleton = i;

        timer = new QTimer(thisPublic);
        QObject::connect(timer, SIGNAL(timeout()), thisPublic, SLOT(nextLoopIteration()));
    }

    ~Instance()
    {
        loopSingleton = 0;
    }
};

Loop::Loop() : d(new Instance(this))
{}

void Loop::setRate(int freqHz)
{
    d->interval = 1.0 / freqHz;
    d->timer->setInterval(de::max(duint64(1), d->interval.asMilliSeconds()));
}

void Loop::start()
{
    d->running = true;
    d->timer->start();
}

void Loop::stop()
{
    d->running = false;
    d->timer->stop();
}

void Loop::pause()
{
    d->timer->stop();
}

void Loop::resume()
{
    d->timer->start();
}

void Loop::timer(TimeDelta const &delay, void (*func)(void))
{
    // The timer will delete itself after it's triggered.
    internal::CallbackTimer *timer = new internal::CallbackTimer(func, qApp);
    timer->start(delay.asMilliSeconds());
}

Loop &Loop::appLoop()
{
    DENG2_ASSERT(loopSingleton != 0);
    return *loopSingleton;
}

void Loop::nextLoopIteration()
{
    try
    {
        if(d->running)
        {
            DENG2_FOR_AUDIENCE(Iteration, i) i->loopIteration();
        }
    }
    catch(Error const &er)
    {
        LOG_AS("Loop");

        // This is called from Qt's event loop, we mustn't let exceptions
        // out of here uncaught.
        App::app().handleUncaughtException("Uncaught exception during loop iteration:\n" + er.asText());
    }
}

} // namespace de
