/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2011-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "de/LegacyCore"
#include "de/LogBuffer"
#include "de/App"

#include "../core/callbacktimer.h"

#include <QCoreApplication>
#include <QList>
#include <QTimer>
#include <QDebug>
#include <string>

namespace de {

LegacyCore *LegacyCore::_appCore;

struct LegacyCore::Instance
{
    /*
    struct Loop {
        int interval;
        bool paused;
        void (*func)(void);
        Loop() : interval(1), paused(false), func(0) {}
    };
    QList<Loop> loopStack;
    */

    App *app;
    //QTimer *loopTimer;
    //Loop loop;

    /// Pointer returned to callers, see LegacyCore::logFileName().
    std::string logName;

    /// Current log output line being formed.
    std::string currentLogLine;

    Instance() : app(0) {}
    ~Instance() {}
};

LegacyCore::LegacyCore()
{
    _appCore = this;
    d = new Instance;

    // Construct a new core application (must have one for the event loop).
    d->app = DENG2_APP;

    /*
    // This will trigger loop callbacks.
    d->loopTimer = new QTimer(this);
    connect(d->loopTimer, SIGNAL(timeout()), this, SLOT(callback()));
    */
}

LegacyCore::~LegacyCore()
{
    stop();

    delete d;   
    _appCore = 0;
}

LegacyCore &LegacyCore::instance()
{
    DENG2_ASSERT(_appCore != 0);
    DENG2_ASSERT(_appCore->d != 0);
    return *_appCore;
}

/*
void LegacyCore::setLoopFunc(void (*func)(void))
{
    LOG_DEBUG("Loop function changed from %p set to %p.") << dintptr(d->loop.func) << dintptr(func);

    d->loopTimer->stop();

    // Set up a timer to periodically call the provided callback function.
    d->loop.func = func;

    // Auto-resume.
    d->loop.paused = false;

    if(func)
    {
        // Start the periodic callback calls.
        d->loopTimer->start(d->loop.interval);
    }
}

void LegacyCore::pushLoop()
{
    d->loopStack.append(d->loop);
}

void LegacyCore::popLoop()
{
    if(d->loopStack.isEmpty())
    {
        LOG_CRITICAL("Pop from empty loop stack.");
        return;
    }

    d->loop = d->loopStack.last();
    d->loopStack.removeLast();

    LOG_DEBUG("Loop function popped, now %p.") << dintptr(d->loop.func);

    d->loop.paused = true; // Force resume.
    resumeLoop();
}

void LegacyCore::pauseLoop()
{
    if(d->loop.paused) return;

    d->loop.paused = true;
    d->loopTimer->stop();
}

void LegacyCore::resumeLoop()
{
    if(!d->loop.paused) return;

    d->loop.paused = false;
    if(d->loop.func)
    {
        // Start the periodic callback calls.
        d->loopTimer->start(d->loop.interval);
    }
}
*/

int LegacyCore::runEventLoop()
{
    LOG_MSG("Starting LegacyCore event loop...");

    // Run the Qt event loop. In the future this will be replaced by the
    // application's main Qt event loop, where deng2 will hook into.
    int code = d->app->execLoop();

    LOG_MSG("Event loop exited with code %i.") << code;
    return code;
}

/*
void LegacyCore::setLoopRate(int freqHz)
{
    int const oldInterval = d->loop.interval;
    d->loop.interval = qMax(1, 1000/freqHz);

    if(oldInterval != d->loop.interval)
    {
        LOG_DEBUG("Loop interval changed to %i ms.") << d->loop.interval;
        if(!d->loop.paused)
        {
            d->loopTimer->stop();
            d->loopTimer->start(d->loop.interval);
        }
    }
}
*/

void LegacyCore::stop(int exitCode)
{
    //d->loopTimer->stop();
    d->app->stopLoop(exitCode);
}

void LegacyCore::timer(duint32 milliseconds, void (*func)(void))
{
    // The timer will delete itself after it's triggered.
    internal::CallbackTimer *timer = new internal::CallbackTimer(func, this);
    timer->start(milliseconds);
}

void LegacyCore::setLogFileName(char const *filePath)
{
    String p = String("/home") / filePath;
    d->logName = p.toStdString();
    LogBuffer::appBuffer().setOutputFile(p);
}

char const *LegacyCore::logFileName() const
{
    return d->logName.c_str();
}

void LegacyCore::printLogFragment(char const *text, LogEntry::Level level)
{
    d->currentLogLine += text;

    std::string::size_type pos;
    while((pos = d->currentLogLine.find('\n')) != std::string::npos)
    {
        LOG().enter(level, d->currentLogLine.substr(0, pos).c_str());
        d->currentLogLine.erase(0, pos + 1);
    }
}

/*
void LegacyCore::callback()
{
    // Update the application clock.
    //Clock::appClock().setTime(Time());

    if(d->loop.func)
    {
        d->loop.func();
    }
}
*/

} // namespace de
