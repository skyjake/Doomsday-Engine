/*
 * The Doomsday Engine Project -- libcore
 *
 * Copyright © 2004-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/waitable.h"
#include "de/time.h"

#include <mutex>
#include <condition_variable>

namespace de {

DE_PIMPL_NOREF(Waitable)
{
    std::condition_variable cv;
    std::mutex mutex;
    int counter = 0;
};

Waitable::Waitable(dint initialValue)
    : d(new Impl)
{
    d->counter = initialValue;
}

/*
void Waitable::reset()
{
    std::lock_guard<std::mutex> grd(d->mutex);
    d->counter = 0;
}
*/

void Waitable::wait() const
{
    wait(0.0);
}

void Waitable::wait(TimeSpan timeOut) const
{
    if (!tryWait(timeOut))
    {
        /// @throw WaitError Failed to secure the resource due to an error.
        throw TimeOutError("Waitable::wait", "Timed out");
    }
}

bool Waitable::tryWait(TimeSpan timeOut) const
{
    std::unique_lock<std::mutex> mtx(d->mutex);
    for (;;)
    {
        if (d->counter == 0)
        {
            if (timeOut > 0.0)
            {
                if (d->cv.wait_for(mtx, std::chrono::microseconds(timeOut.asMicroSeconds())) ==
                    std::cv_status::timeout)
                {
                    return false;
                }
            }
            else
            {
                d->cv.wait(mtx);
            }
        }
        else
        {
            DE_ASSERT(d->counter > 0);
            d->counter--;
            break;
        }
    }
    return true;
}

void Waitable::post() const
{
    std::lock_guard<std::mutex> grd(d->mutex);
    DE_ASSERT(d->counter >= 0);
    d->counter++;
    d->cv.notify_one();
}

} // namespace de

