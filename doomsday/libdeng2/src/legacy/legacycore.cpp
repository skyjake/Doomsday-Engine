/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2011 Jaakko Keränen <jaakko.keranen@iki.fi>
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
#include "de/LegacyNetwork"
#include "de/LogBuffer"

#include <QCoreApplication>
#include <QTimer>
#include <QDebug>

using namespace de;

LegacyCore* LegacyCore::_appCore;

/**
 * @internal Private instance data for LegacyCore.
 */
struct LegacyCore::Instance
{
    App* app;
    LegacyNetwork network;
    void (*loopFunc)(void);
    LogBuffer logBuffer;
    int loopInterval;

    Instance() : app(0), loopFunc(0), loopInterval(1) {}
    ~Instance() {}
};

LegacyCore::LegacyCore(App* dengApp)
{
    _appCore = this;
    d = new Instance;

    // Construct a new core application (must have one for the event loop).
    d->app = dengApp;

    // The global log buffer will be available for the entire runtime of deng2.
    LogBuffer::setAppBuffer(d->logBuffer);
#ifdef DENG2_DEBUG
    d->logBuffer.enable(Log::DEBUG);
#endif
}

LegacyCore::~LegacyCore()
{
    stop();

    delete d;   
    _appCore = 0;
}

LegacyCore& LegacyCore::instance()
{
    DENG2_ASSERT(_appCore != 0);
    DENG2_ASSERT(_appCore->d != 0);
    return *_appCore;
}

LegacyNetwork& LegacyCore::network()
{
    return instance().d->network;
}

void LegacyCore::setLoopFunc(void (*func)(void))
{
    LOG_DEBUG("Loop function changed from %p set to %p.") << dintptr(d->loopFunc) << dintptr(func);

    // Set up a timer to periodically call the provided callback function.
    d->loopFunc = func;

    if(func)
    {
        // Start the periodic callback calls.
        QTimer::singleShot(d->loopInterval, this, SLOT(callback()));
    }
}

int LegacyCore::runEventLoop()
{
    LOG_MSG("Starting LegacyCore event loop...");

    // Run the Qt event loop. In the future this will be replaced by the
    // application's main Qt event loop, where deng2 will hook into.
    int code = d->app->exec();

    LOG_MSG("Event loop exited with code %i.") << code;
    return code;
}

void LegacyCore::setLoopRate(int freqHz)
{
    d->loopInterval = qMax(1, 1000/freqHz);
}

void LegacyCore::stop(int exitCode)
{
    d->app->exit(exitCode);
}

void LegacyCore::callback()
{
    if(d->loopFunc)
    {
        d->loopFunc();
        QTimer::singleShot(d->loopInterval, this, SLOT(callback()));
    }
}
