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
#include "../sdl.h"

#include <SDL_thread.h>

using namespace de;

/**
 * Call the Thread instance's run() method.  Called by SDL when the
 * thread is created.
 */
int Thread::runner(void* owner)
{
    Thread* self = static_cast<Thread*>(owner);
    
    self->run();
    self->_thread = NULL;
    self->_endOfThread.post();

    Log::disposeThreadLog();

    // The return value is not used at the moment.
    return 0;
}

Thread::Thread() : _stopNow(false), _thread(0)
{}

Thread::~Thread()
{
    join(4);
}

void Thread::start()
{
    assert(_thread == NULL);

    _stopNow = false;
    _thread = SDL_CreateThread(runner, this);
}

void Thread::stop()
{
    // We'll rely on run() to notice this and exit as soon as possible.
    _stopNow = true;
}

void Thread::join(const Time::Delta& timeOut)
{
    stop();
    if(_thread)
    {
        _endOfThread.wait(timeOut);
        //assert(_thread == NULL);
    }
}

bool Thread::shouldStopNow() const
{
    return _stopNow;
}

bool Thread::isRunning() const
{
    return _thread != 0;
}
