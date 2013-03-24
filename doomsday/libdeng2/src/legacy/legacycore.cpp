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
#include <QDebug>
#include <string>

namespace de {

LegacyCore *LegacyCore::_appCore;

DENG2_PIMPL_NOREF(LegacyCore)
{
    App *app;

    /// Pointer returned to callers, see LegacyCore::logFileName().
    std::string logName;

    /// Current log output line being formed.
    std::string currentLogLine;

    Instance() : app(0) {}
    ~Instance() {}
};

LegacyCore::LegacyCore() : d(new Instance)
{
    _appCore = this;

    // Construct a new core application (must have one for the event loop).
    d->app = DENG2_APP;
}

LegacyCore::~LegacyCore()
{
    stop();

    _appCore = 0;
}

LegacyCore &LegacyCore::instance()
{
    DENG2_ASSERT(_appCore != 0);
    DENG2_ASSERT(_appCore->d.get() != 0);
    return *_appCore;
}

void LegacyCore::stop(int exitCode)
{
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

} // namespace de
