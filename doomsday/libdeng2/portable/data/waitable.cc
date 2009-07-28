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

#include "de/Waitable"
#include "de/Time"
#include "../sdl.h"

#include <SDL_thread.h>

using namespace de;

Waitable::Waitable(duint initialValue) : semaphore_(0)
{
    semaphore_ = SDL_CreateSemaphore(initialValue);
}
    
Waitable::~Waitable()
{
    SDL_sem* sem = static_cast<SDL_sem*>(semaphore_);

    // The semaphore will be destroyed.  This'll make sure no new waits can begin.
    semaphore_ = 0;
    
    if(sem)
    {
        const unsigned SAFE_POST = 50;

        // Let's first make sure that there will be no threads left
        // waiting on the semaphore.
        for(unsigned i = 0; i < SAFE_POST; ++i)
        {
            SDL_SemPost(sem);
        }

        SDL_DestroySemaphore(sem);
    }
}

void Waitable::wait()
{
    // Wait until the resource becomes available.
    if(SDL_SemWait(static_cast<SDL_sem*>(semaphore_)) < 0)
    {
        /// @throw WaitError Failed to secure the resource due to an error.
        throw WaitError("Waitable::wait", SDL_GetError());
    }
}

void Waitable::wait(const Time::Delta& timeOut)
{
    int result = SDL_SemWaitTimeout(static_cast<SDL_sem*>(semaphore_),
                                    duint32(timeOut.asMilliSeconds()));

    if(result == SDL_MUTEX_TIMEDOUT)
    {
        /// @throw TimeOutError Timeout expired before the resource was secured.
        throw TimeOutError("Waitable::wait", "timed out");
    }
    if(result < 0)
    {
        /// @throw TimeOutError Failed to secure the resource due to an error.
        throw WaitError("Waitable::wait", SDL_GetError());
    }
}

void Waitable::post()
{
    SDL_SemPost(static_cast<SDL_sem*>(semaphore_));
}
