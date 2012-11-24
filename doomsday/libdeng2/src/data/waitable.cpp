/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2004-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
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

using namespace de;

#define WAITABLE_TIMEOUT     10

Waitable::Waitable(duint initialValue) : _semaphore(initialValue)
{}
    
Waitable::~Waitable()
{}

void Waitable::wait()
{
    wait(WAITABLE_TIMEOUT);
}

void Waitable::wait(Time::Delta const &timeOut)
{
    // Wait until the resource becomes available.
    if(!_semaphore.tryAcquire(1, int(timeOut.asMilliSeconds())))
    {
        /// @throw WaitError Failed to secure the resource due to an error.
        throw WaitError("Waitable::wait", "Timed out");
    }
}

void Waitable::post()
{
    _semaphore.release();
}
