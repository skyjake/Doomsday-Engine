/*
 * The Doomsday Engine Project -- libcore
 *
 * Copyright © 2009-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBCORE_CLOCK_H
#define LIBCORE_CLOCK_H

#include "de/time.h"
#include "de/observers.h"

namespace de {

/**
 * Time source.
 * @ingroup core
 */
class DE_PUBLIC Clock
{
public:
    /**
     * Notified whenever the time of the clock changes. The audience members
     * will be notified in unspecified order.
     */
    DE_AUDIENCE(TimeChange, void timeChanged(const Clock &))

    /**
     * Notified whenever the time of the clock changes. The entire priority
     * audience is notified before the regular TimeChange audience.
     */
    typedef Observers<DE_AUDIENCE_INTERFACE(TimeChange)> PriorityTimeChangeAudience;
    PriorityTimeChangeAudience audienceForPriorityTimeChange;

public:
    Clock();

    virtual ~Clock();

    virtual void setTime(const Time &currentTime);

    void advanceTime(TimeSpan span);

    /**
     * Returns the amount of time elapsed since the clock was created.
     * @return Elapsed time.
     */
    TimeSpan elapsed() const;

    /**
     * Returns a reference to the current time.
     *
     * @return Current time.
     */
    const Time &time() const;

    /**
     * Number of times the time has changed. Every time the clock time is changed,
     * its tick count is incremented.
     */
    duint32 tickCount() const;

public:
    static void setAppClock(Clock *c);
    static Clock &get();
    static const Time &appTime();

private:
    DE_PRIVATE(d)

    static Clock *_appClock;
};

} // namespace de

#endif /* LIBCORE_ICLOCK_H */
