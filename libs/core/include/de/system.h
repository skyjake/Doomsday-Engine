/** @file system.h  Base class for application subsystems.
 *
 * @authors Copyright © 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBCORE_SYSTEM_H
#define LIBCORE_SYSTEM_H

#include "de/clock.h"
#include "de/event.h"

namespace de {

/**
 * Base class for application subsystems.
 *
 * System instances observe progress of time and may receive and process input
 * events. In other words, using traditional DOOM terminology, they have a
 * ticker and a responder.
 *
 * @ingroup core
 */
class DE_PUBLIC System : DE_OBSERVES(Clock, TimeChange)
{
public:
    /// Behavior of the system.
    enum Flag
    {
        ObservesTime = 0x1,         ///< System will observe clock time.
        // ReceivesInputEvents = 0x2,  ///< System will be given input events.

        DefaultBehavior = ObservesTime
    };

public:
    System(const Flags &behavior = DefaultBehavior);

    void setBehavior(const Flags &behavior, FlagOp operation = SetFlags);

    Flags behavior() const;

    /*
     * Offers an event to be processed by the system. If the event is eaten
     * by the system, it will not be offered to any other systems.
     *
     * @param ev  Event to process.
     *
     * @return @c true, if the event was eaten and should not be processed by
     * others. @c false, if event was not eaten.
     */
//    virtual bool processEvent(const Event &ev);

    // Observes TimeChange.
    virtual void timeChanged(const Clock &);

private:
    DE_PRIVATE(d)
};

} // namespace de

#endif // LIBCORE_SYSTEM_H
