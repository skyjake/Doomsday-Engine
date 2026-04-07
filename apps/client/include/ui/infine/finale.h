/** @file finale.h  InFine animation systems, Finale script.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2014 Daniel Swanson <danij@dengine.net>
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

#ifndef DE_UI_INFINE_FINALE_H
#define DE_UI_INFINE_FINALE_H

#include <de/observers.h>
#include <de/string.h>
#include "../ddevent.h"
#include "api_infine.h"  // finaleid_t

class FinaleInterpreter;

#define FINF_BEGIN          0x01
#define FINF_END            0x02
#define FINF_SCRIPT         0x04 // Script included.
#define FINF_SKIP           0x10

/**
 * A Finale instance contains the high-level state of an InFine script.
 *
 * @see FinaleInterpreter (interactive script interpreter)
 *
 * @ingroup infine
 */
class Finale
{
public:
    /// Notified when the finale is about to be deleted.
    DE_AUDIENCE(Deletion, void finaleBeingDeleted(const Finale &finale))

public:
    /**
     * @param flags   @ref finaleFlags
     * @param id      Unique identifier for the script.
     * @param script  The InFine script to be interpreted (a copy is made).
     */
    Finale(int flags, finaleid_t id, const de::String &script);

    int flags() const;
    finaleid_t id() const;

    bool isActive() const;
    bool isSuspended() const;

    void resume();
    void suspend();
    bool terminate();

    /**
     * @return  @c true if the end of the script was reached.
     */
    bool runTicks(timespan_t timeDelta);

    int handleEvent(const ddevent_t &ev);
    bool requestSkip();
    bool isMenuTrigger() const;

    /**
     * Provides access to the script interpreter. Mainly for debug purposes.
     */
    const FinaleInterpreter &interpreter() const;

private:
    DE_PRIVATE(d)
};

#endif // DE_UI_INFINE_FINALE_H
