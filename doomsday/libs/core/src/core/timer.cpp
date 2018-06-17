/** @file timer.cpp  Simple timer.
 *
 * @authors Copyright (c) 2018 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/Timer"

namespace de {

DE_PIMPL_NOREF(Timer)
{
    TimeSpan interval     = 1;
    bool     isSingleShot = false;
    bool     isActive     = false;

    DE_PIMPL_AUDIENCE(Trigger)
};

DE_AUDIENCE_METHOD(Timer, Trigger)

Timer::Timer()
    : d(new Impl)
{}

void Timer::setInterval(const TimeSpan &interval)
{
    d->interval = interval;
}

void Timer::setSingleShot(bool singleshot)
{
    d->isSingleShot = singleshot;
}

void Timer::start()
{
    d->isActive = true;

    /// @todo Use app's event loop to register timer events.

    auto trigger = [this]()
    {
        if (d->isSingleShot)
        {
            stop();
        }
        if (d->isActive)
        {
            // Register for the next event.

        }
        DE_FOR_AUDIENCE2(Trigger, i) { i->triggered(*this); }
    };
}

void Timer::stop()
{
    d->isActive = false;
}

bool Timer::isActive() const
{
    return d->isActive;
}

} // namespace de
