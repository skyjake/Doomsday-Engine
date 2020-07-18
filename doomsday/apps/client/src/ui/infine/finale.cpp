/** @file finale.cpp  InFine animation system, Finale script.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2014 Daniel Swanson <danij@dengine.net>
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

#include "ui/infine/finale.h"
#include "ui/bindcontext.h"
#include "ui/infine/finaleinterpreter.h"
#include "network/net_main.h"
#ifdef __SERVER__
#  include "server/sv_infine.h"
#endif

#include <doomsday/net.h>
#include <de/logbuffer.h>

using namespace de;

DE_PIMPL(Finale)
{
    bool active;
    int flags;  ///< @ref finaleFlags
    finaleid_t id;
    FinaleInterpreter interpreter;

    Impl(Public *i, int flags, finaleid_t id)
        : Base(i)
        , active     (false)
        , flags      (flags)
        , id         (id)
        , interpreter(id)
    {}

    ~Impl()
    {
        DE_NOTIFY_PUBLIC(Deletion, i) i->finaleBeingDeleted(self());
    }

    void loadScript(const String &script)
    {
        if (script.isEmpty()) return;

        LOGDEV_SCR_MSG("Begin Finale - id:%i '%.30s'") << id << script;
        interpreter.loadScript(script);

#ifdef __SERVER__
        if (!(flags & FF_LOCAL) && netState.isServer)
        {
            // Instruct clients to start playing this Finale.
            Sv_Finale(id, FINF_BEGIN | FINF_SCRIPT, script);
        }
#endif

        active = true;
    }

    DE_PIMPL_AUDIENCE(Deletion)
};

DE_AUDIENCE_METHOD(Finale, Deletion)

Finale::Finale(int flags, finaleid_t id, const String &script)
    : d(new Impl(this, flags, id))
{
    d->loadScript(script);
}

int Finale::flags() const
{
    return d->flags;
}

finaleid_t Finale::id() const
{
    return d->id;
}

bool Finale::isActive() const
{
    return d->active;
}

bool Finale::isSuspended() const
{
    return d->interpreter.isSuspended();
}

void Finale::resume()
{
    d->active = true;
    d->interpreter.resume();
}

void Finale::suspend()
{
    d->active = false;
    d->interpreter.suspend();
}

bool Finale::terminate()
{
    if (!d->active) return false;

    LOGDEV_SCR_VERBOSE("Terminating finaleid %i") << d->id;
    d->active = false;
    d->interpreter.terminate();
    return true;
}

bool Finale::runTicks(timespan_t timeDelta)
{
    if (d->interpreter.runTicks(timeDelta, d->active && DD_IsSharpTick()))
    {
        // The script has ended!
        terminate();
        return true;
    }
    return false;
}

int Finale::handleEvent(const ddevent_t &ev)
{
    if (!d->active) return false;
    return d->interpreter.handleEvent(ev);
}

bool Finale::requestSkip()
{
    if (!d->active) return false;
    return d->interpreter.skip();
}

bool Finale::isMenuTrigger() const
{
    if (!d->active) return false;
    LOG_SCR_XVERBOSE("IsMenuTrigger: %i", d->interpreter.isMenuTrigger());
    return d->interpreter.isMenuTrigger();
}

const FinaleInterpreter &Finale::interpreter() const
{
    return d->interpreter;
}
