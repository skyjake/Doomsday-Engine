/** @file system.h  Base class for application subsystems.
 *
 * Long summary of the functionality.
 *
 * @todo Update the fields above as appropriate.
 *
 * @authors Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small> 
 */

#ifndef LIBDENG2_SYSTEM_H
#define LIBDENG2_SYSTEM_H

#include "../Clock"
#include "../Event"
#include <QFlags>

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
class DENG2_PUBLIC System : DENG2_OBSERVES(Clock, TimeChange)
{
public:
    /// Behavior of the system.
    enum Flag
    {
        ObservesTime = 0x1,         ///< System will observe clock time.
        ReceivesInputEvents = 0x2,  ///< System will be given input events.

        DefaultBehavior = ObservesTime
    };
    Q_DECLARE_FLAGS(Flags, Flag)

public:
    System(Flags const &behavior = DefaultBehavior);

    virtual ~System();

    void setBehavior(Flags const &behavior);

    Flags behavior() const;

    /**
     * Offers an event to be processed by the system. If the event is eaten
     * by the system, it will not be offered to any other systems.
     *
     * @param ev  Event to process.
     *
     * @return @c true, if the event was eaten and should not be processed by
     * others. @c false, if event was not eaten.
     */
    virtual bool processEvent(Event const &ev);

    // Observes TimeChange.
    virtual void timeChanged(Clock const &);

private:
    struct Instance;
    Instance *d;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(System::Flags)

} // namespace de

#endif // LIBDENG2_SYSTEM_H
