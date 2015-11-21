/** @file scheduler.h  Script scheduling utility.
 *
 * @authors Copyright (c) 2015 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDENG2_SCHEDULER_H
#define LIBDENG2_SCHEDULER_H

#include "../libcore.h"
#include "../Time"
#include "../String"

namespace de {

class Script;
class Record;

/**
 * Script scheduling utility.
 *
 * Scheduler owns the parsed scripts, but does not execute them. Use Scheduler::Clock
 * to execute scripts. There can be any number of Scheduler::Clock instances operating
 * on a single schedule.
 *
 * @ingroup script
 */
class DENG2_PUBLIC Scheduler
{
public:
    Scheduler();

    void clear();

    /**
     * Sets the execution context, i.e., global namespace for the scripts. All
     * scripts of the scheduler run in the same context.
     *
     * @param context  Global namespace.
     */
    void setContext(Record &context);

    Record *context() const;

    /**
     * Adds a new script to the scheduler.
     *
     * @param at          Point in time when the script is to be executed.
     * @param source      Script source. This will be parsed before the method returns.
     * @param sourcePath  Path where the source comes from.
     *
     * @return Scheduled Script (owned by Scheduler).
     */
    Script &addScript(TimeDelta at, String const &source, String const &sourcePath = "");

    void addFromInfo(Record const &timelineRecord);

    class DENG2_PUBLIC Clock
    {
    public:
        Clock(Scheduler const &schedule, Record *context = nullptr);

        /**
         * Returns the current time of the clock.
         */
        TimeDelta at() const;

        /**
         * Rewinds the clock back to zero.
         *
         * @param toTime  Rewind destination time.
         */
        void rewind(TimeDelta const &toTime = 0.0);

        /**
         * Advances the current time of the clock and executes any scripts whose
         * execution time has arrived.
         *
         * @param elapsed  Time elapsed since the previous call.
         */
        void advanceTime(TimeDelta const &elapsed);

        /**
         * Checks if there are no more scheduled sheduler is out of scheduled scripts.
         * @return
         */
        bool isFinished() const;

    private:
        DENG2_PRIVATE(d)
    };

private:
    DENG2_PRIVATE(d)
};

} // namespace de

#endif // LIBDENG2_SCHEDULER_H

