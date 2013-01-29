/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2009-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDENG2_CLOCK_H
#define LIBDENG2_CLOCK_H

#include <QObject>
#include "../Time"

namespace de {

/**
 * Time source.
 * @ingroup core
 */
class Clock : public QObject
{
    Q_OBJECT

public:
    Clock();

    virtual ~Clock();

    virtual void setTime(Time const &currentTime);

    void advanceTime(TimeDelta const &span);

    /**
     * Returns the amount of time elapsed since the clock was created.
     * @return Elapsed time.
     */
    TimeDelta elapsed() const;

    /**
     * Returns a reference to the current time.
     *
     * @return Current time.
     */
    Time const &time() const;

    static void setAppClock(Clock *c);
    static Clock &appClock();

signals:
    void timeChanged();

private:
    Time _startedAt;
    Time _time;

    static Clock *_appClock;
};

} // namespace de

#endif /* LIBDENG2_ICLOCK_H */
