/** @file timeline.h  Script scheduling utility.
 *
 * @authors Copyright (c) 2015-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBCORE_TIMELINE_H
#define LIBCORE_TIMELINE_H

#include "../libcore.h"
#include "de/time.h"
#include "de/string.h"

namespace de {

class Script;
class Record;

/**
 * Collection of scripts to be run at specified points in time.
 *
 * Timeline owns the parsed scripts, but does not execute them. Use Timeline::Clock
 * to execute scripts. There can be any number of Timeline::Clock instances operating
 * on a single timeline.
 *
 * @ingroup script
 */
class DE_PUBLIC Timeline
{
public:
    Timeline();

    void clear();

    /**
     * Sets the execution context, i.e., global namespace for the scripts. All
     * scripts of the timeline run in the same context.
     *
     * @param context  Global namespace.
     */
    void setContext(Record &context);

    Record *context() const;

    /**
     * Adds a new script to the timeline.
     *
     * @param at          Point in time when the script is to be executed.
     * @param source      Script source. This will be parsed before the method returns.
     * @param sourcePath  Path where the source comes from.
     *
     * @return Scheduled Script (owned by Timeline).
     */
    Script &addScript(TimeSpan at, const String &source, const String &sourcePath = "");

    void addFromInfo(const Record &timelineRecord);

    /**
     * Clock for executing a timeline.
     */
    class DE_PUBLIC Clock
    {
    public:
        Clock(const Timeline &timeline, Record *context = nullptr);

        /**
         * Returns the current time of the clock.
         */
        TimeSpan at() const;

        /**
         * Rewinds the clock back to zero.
         *
         * @param toTime  Rewind destination time.
         */
        void rewind(TimeSpan toTime = 0.0);

        /**
         * Advances the current time of the clock and executes any scripts whose
         * execution time has arrived.
         *
         * @param elapsed  Time elapsed since the previous call.
         */
        void advanceTime(TimeSpan elapsed);

        /**
         * Checks if there are no more scheduled sheduler is out of scheduled scripts.
         * @return
         */
        bool isFinished() const;

    private:
        DE_PRIVATE(d)
    };

private:
    DE_PRIVATE(d)
};

} // namespace de

#endif // LIBCORE_TIMELINE_H

