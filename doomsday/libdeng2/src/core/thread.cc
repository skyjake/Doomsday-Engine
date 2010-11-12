/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2004-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/Thread"
#include "de/Time"
#include "de/Log"

using namespace de;

// Milliseconds to wait until the thread stops.
#define THREAD_QUIT_TIME 3000

void Thread::Runner::run()
{
    _owner->run();

    // Log messages from the thread can also be disposed.
    Log::disposeThreadLog();
}

Thread::Thread() : _runner(0)
{
    _runner = new Runner(this);
}

Thread::~Thread()
{
    Q_ASSERT(_runner == NULL);

    // Stop the thread.
    _runner->quit();
    _runner->wait(THREAD_QUIT_TIME);
    delete _runner;
}

void Thread::start()
{
    Q_ASSERT(_runner == NULL);

    _runner->start();
}

void Thread::stop()
{
    // The thread will stop as soon as it can.
    _runner->quit();
}

void Thread::join(const Time::Delta& timeOut)
{
    stop();
    _runner->wait(timeOut.asMilliSeconds());
}

bool Thread::isRunning() const
{
    return _runner->isRunning();
}
