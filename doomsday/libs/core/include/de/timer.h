/** @file timer.h  Simple timer.
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

#ifndef LIBCORE_TIMER_H
#define LIBCORE_TIMER_H

#include "de/time.h"
#include "de/observers.h"

namespace de {

/**
 * Simple timer.
 *
 * The default timer has an interval of 1.0 seconds and is not single-shot.
 */
class DE_PUBLIC Timer
{
public:
    Timer();
    virtual ~Timer();

    void setInterval(TimeSpan interval);
    void setSingleShot(bool singleshot);
    void start();
    void stop();

    inline void start(TimeSpan interval)
    {
        setInterval(interval);
        start();
    }

    bool     isActive() const;
    bool     isSingleShot() const;
    TimeSpan interval() const;

    void trigger();
    void post();

    /**
     * Adds a new trigger callback. This is the same as adding the callback to the Trigger
     * audience.
     *
     * @param callback  Function to call when the timer is triggered.
     */
    Timer &operator+=(const std::function<void()> &callback);

    DE_AUDIENCE(Trigger, void triggered(Timer &))

private:
    DE_PRIVATE(d)
};

} // namespace de

#endif // LIBCORE_TIMER_H
