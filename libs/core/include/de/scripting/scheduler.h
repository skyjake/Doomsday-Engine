/** @file scheduler.h  Scheduler for scripts and timelines.
 *
 * @authors Copyright (c) 2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBCORE_SCHEDULER_H
#define LIBCORE_SCHEDULER_H

#include "de/scripting/timeline.h"

namespace de {

/**
 * Scheduler for scripts and timelines.
 */
class DE_PUBLIC Scheduler
{
public:
    Scheduler();

    void clear();

    /**
     * Starts executing a timeline. The timeline object will be deleted when the
     * finished. The timeline's context is used for execution.
     *
     * @param timeline  Timeline object. Scheduler takes ownership.
     * @param name      Name for the started timeline. If empty, a unique name will
     *                  be generated.
     * @return Name of the started timeline.
     */
    String start(Timeline *timeline, const String &name = String());

    /**
     * Starts executing a shared timeline.
     *
     * @param sharedTimeline  Timeline object. Scheduler does not take ownership.
     * @param context         Context where to execute the timeline.
     * @param name            Name for the started timeline instance. If empty, a unique
     *                        name will be generated.
     * @return Name of the started timeline.
     */
    String start(const Timeline &sharedTimeline, Record *context, const String &name = String());

    /**
     * Stops a running timeline.
     *
     * @param name  Timeline name.
     */
    void stop(const String &name);

    void advanceTime(TimeSpan elapsed);

private:
    DE_PRIVATE(d)
};

} // namespace de

#endif // LIBCORE_SCHEDULER_H
