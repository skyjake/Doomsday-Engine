/** @file loop.cpp
 *
 * @authors Copyright © 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/Loop"
#include "de/App"
#include "de/Time"
#include "de/Log"
#include "de/math.h"

#include <QCoreApplication>
#include <QTimer>

#include "../src/core/callbacktimer.h"

namespace de {

static Loop *loopSingleton = 0;

DENG2_PIMPL(Loop)
{
    TimeDelta interval;
    bool running;
    QTimer *timer;

    Impl(Public *i) : Base(i), interval(0), running(false)
    {
        DENG2_ASSERT(!loopSingleton);
        loopSingleton = i;

        timer = new QTimer(thisPublic);
        QObject::connect(timer, SIGNAL(timeout()), thisPublic, SLOT(nextLoopIteration()));
    }

    ~Impl()
    {
        loopSingleton = 0;
    }

    DENG2_PIMPL_AUDIENCE(Iteration)
};

DENG2_AUDIENCE_METHOD(Loop, Iteration)

Loop::Loop() : d(new Impl(this))
{}

void Loop::setRate(int freqHz)
{
    d->interval = 1.0 / freqHz;
    d->timer->setInterval(de::max(1, int(d->interval.asMilliSeconds())));
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

void Loop::timer(TimeDelta const &delay, std::function<void ()> func)
{
    // The timer will delete itself after it's triggered.
    internal::CallbackTimer *timer = new internal::CallbackTimer(func, qApp);
    timer->start(delay.asMilliSeconds());
}

Loop &Loop::get()
{
    DENG2_ASSERT(loopSingleton != 0);
    return *loopSingleton;
}

void Loop::nextLoopIteration()
{
    try
    {
        if (d->running)
        {
            DENG2_FOR_AUDIENCE2(Iteration, i) i->loopIteration();
        }
    }
    catch (Error const &er)
    {
        LOG_AS("Loop");

        // This is called from Qt's event loop, we mustn't let exceptions
        // out of here uncaught.
        App::app().handleUncaughtException("Uncaught exception during loop iteration:\n" + er.asText());
    }
}

// LoopCallback -------------------------------------------------------------------------

LoopCallback::LoopCallback()
{}

LoopCallback::~LoopCallback()
{}

bool LoopCallback::isEmpty() const
{
    return _funcs.isEmpty();
}

LoopCallback::operator bool() const
{
    return !isEmpty();
}

void LoopCallback::enqueue(Callback func)
{
    DENG2_GUARD(this);

    _funcs << func;

    Loop::get().audienceForIteration() += this;
}

void LoopCallback::loopIteration()
{
    QList<Callback> funcs;

    // Lock while modifying but not during the callbacks themselves.
    {
        DENG2_GUARD(this);
        Loop::get().audienceForIteration() -= this;

        // Make a copy of the list if new callbacks get enqueued in the callback.
        funcs = _funcs;
        _funcs.clear();
    }

    for (Callback const &cb : funcs)
    {
        cb();
    }
}

} // namespace de
