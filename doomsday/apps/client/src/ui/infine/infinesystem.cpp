/** @file infinesystem.cpp  Interactive animation sequence system.
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

#define DE_NO_API_MACROS_INFINE

#include <de/log.h>
#include <doomsday/console/var.h>

#include "de_base.h"
#include "ui/infine/infinesystem.h"
#include "ui/bindcontext.h"
#include "ui/infine/finale.h"
#include "ui/infine/finaleinterpreter.h"

#ifdef __CLIENT__
#  include "ui/inputsystem.h"
#  include "clientapp.h"
#endif

using namespace de;

DE_PIMPL_NOREF(InFineSystem)
, DE_OBSERVES(Finale, Deletion)
{
    Finales finales;

    ~Impl() { deleteAll(finales); }

    Finale *finaleForId(finaleid_t id)
    {
        if (id != 0)
        {
            for (const Finale *f : finales)
            {
                if (f->id() == id) return const_cast<Finale *>(f);
            }
        }
        return nullptr;
    }

    finaleid_t nextUnusedId()
    {
        finaleid_t id = 0;
        while (finaleForId(++id)) {}
        return id;
    }

    void finaleBeingDeleted(const Finale &finale)
    {
        finales.removeOne(const_cast<Finale *>(&finale));
    }
};

InFineSystem::InFineSystem() : d(new Impl)
{}

void InFineSystem::reset()
{
    LOG_AS("InFineSystem");

    while (!d->finales.isEmpty())
    {
        std::unique_ptr<Finale> finale(d->finales.takeFirst());
        finale->terminate();
    }
}

bool InFineSystem::finaleInProgess() const
{
    for (Finale *finale : d->finales)
    {
        if (finale->isActive() || finale->isSuspended())
            return true;
    }
    return false;
}

void InFineSystem::runTicks(timespan_t timeDelta)
{
    LOG_AS("InFineSystem");
    for (int i = 0; i < d->finales.count(); ++i)
    {
        Finale *finale = d->finales[i];
        if (finale->runTicks(timeDelta))
        {
            // The script has terminated.
            delete finale;
        }
    }
}

Finale &InFineSystem::newFinale(int flags, String script, const String &setupCmds)
{
    LOG_AS("InFineSystem");

    if (!setupCmds.isEmpty())
    {
        // Setup commands are included. We must prepend these to the script
        // in a special control block that will be executed immediately.
        script.prepend("OnLoad {\n" + setupCmds + "}\n");
    }

    d->finales << new Finale(flags, d->nextUnusedId(), script);
    auto *finale = d->finales.last();
    finale->audienceForDeletion() += d;
    return *finale;
}

bool InFineSystem::hasFinale(finaleid_t id) const
{
    return d->finaleForId(id) != nullptr;
}

Finale &InFineSystem::finale(finaleid_t id)
{
    Finale *finale = d->finaleForId(id);
    if (finale) return *finale;
    /// @throw MissingFinaleError The given id does not reference a Finale
    throw MissingFinaleError("finale", "No Finale known by id:" + String::asText(id));
}

const InFineSystem::Finales &InFineSystem::finales() const
{
    return d->finales;
}

#ifdef __CLIENT__
static bool inited;

void InFineSystem::initBindingContext() // static
{
    // Already been here?
    if (inited) return;

    inited = true;
    BindContext &context = ClientApp::input().context("finale");
    context.setDDFallbackResponder(de::function_cast<int (*)(const ddevent_t *)>(gx.FinaleResponder));
    context.activate(); // always on
}

void InFineSystem::deinitBindingContext() // static
{
    // Not yet initialized?
    if (!inited) return;

    BindContext &context = ClientApp::input().context("finale");
    context.setDDFallbackResponder(nullptr);
    context.deactivate();
    inited = false;
}
#endif // __CLIENT__

namespace {
byte scaleMode = SCALEMODE_SMART_STRETCH;
}

void InFineSystem::consoleRegister() // static
{
    C_VAR_BYTE("rend-finale-stretch", &scaleMode, 0, SCALEMODE_FIRST, SCALEMODE_LAST);
}

// Public API (C Wrapper) ---------------------------------------------------------------

#undef FI_Execute2
finaleid_t FI_Execute2(const char *script, int flags, const char *setupCmds)
{
    LOG_AS("InFine.Execute");

    if (!script || !script[0])
    {
        LOGDEV_SCR_WARNING("Attempted to play an empty script");
        return 0;
    }
    if ((flags & FF_LOCAL) && isDedicated)
    {
        // Dedicated servers do not play local Finales.
        LOGDEV_SCR_WARNING("No local finales in dedicated mode");
        return 0;
    }

    return App_InFineSystem().newFinale(flags, script, setupCmds).id();
}

#undef FI_Execute
finaleid_t FI_Execute(const char *script, int flags)
{
    return FI_Execute2(script, flags, 0);
}

#undef FI_ScriptTerminate
void FI_ScriptTerminate(finaleid_t id)
{
    LOG_AS("InFine.ScriptTerminate");
    if (App_InFineSystem().hasFinale(id))
    {
        Finale &finale = App_InFineSystem().finale(id);
        if (finale.terminate())
        {
            delete &finale;
        }
        return;
    }
    LOGDEV_SCR_WARNING("Unknown finaleid %i") << id;
}

#undef FI_ScriptActive
dd_bool FI_ScriptActive(finaleid_t id)
{
    LOG_AS("InFine.ScriptActive");
    if (App_InFineSystem().hasFinale(id))
    {
        return App_InFineSystem().finale(id).isActive();
    }
    LOGDEV_SCR_WARNING("Unknown finaleid %i") << id;
    return false;
}

#undef FI_ScriptSuspend
void FI_ScriptSuspend(finaleid_t id)
{
    LOG_AS("InFine.ScriptSuspend");
    if (App_InFineSystem().hasFinale(id))
    {
        App_InFineSystem().finale(id).suspend();
        return;
    }
    LOGDEV_SCR_WARNING("Unknown finaleid %i") << id;
}

#undef FI_ScriptResume
void FI_ScriptResume(finaleid_t id)
{
    LOG_AS("InFine.ScriptResume");
    if (App_InFineSystem().hasFinale(id))
    {
        App_InFineSystem().finale(id).resume();
        return;
    }
    LOGDEV_SCR_WARNING("Unknown finaleid %i") << id;
}

#undef FI_ScriptSuspended
dd_bool FI_ScriptSuspended(finaleid_t id)
{
    LOG_AS("InFine.ScriptSuspended");
    if (App_InFineSystem().hasFinale(id))
    {
        return App_InFineSystem().finale(id).interpreter().isSuspended();
    }
    LOGDEV_SCR_WARNING("Unknown finaleid %i") << id;
    return false;
}

#undef FI_ScriptFlags
int FI_ScriptFlags(finaleid_t id)
{
    LOG_AS("InFine.ScriptFlags");
    if (App_InFineSystem().hasFinale(id))
    {
        return App_InFineSystem().finale(id).flags();
    }
    LOGDEV_SCR_WARNING("Unknown finaleid %i") << id;
    return 0;
}

#undef FI_ScriptResponder
int FI_ScriptResponder(finaleid_t id, const void *ev)
{
    DE_ASSERT(ev);
    LOG_AS("InFine.ScriptResponder");
    if (App_InFineSystem().hasFinale(id))
    {
        return App_InFineSystem().finale(id).handleEvent(*static_cast<const ddevent_t *>(ev));
    }
    LOGDEV_SCR_WARNING("Unknown finaleid %i") << id;
    return false;
}

#undef FI_ScriptCmdExecuted
dd_bool FI_ScriptCmdExecuted(finaleid_t id)
{
    LOG_AS("InFine.CmdExecuted");
    if (App_InFineSystem().hasFinale(id))
    {
        return App_InFineSystem().finale(id).interpreter().commandExecuted();
    }
    LOGDEV_SCR_WARNING("Unknown finaleid %i") << id;
    return false;
}

#undef FI_ScriptRequestSkip
dd_bool FI_ScriptRequestSkip(finaleid_t id)
{
    LOG_AS("InFine.ScriptRequestSkip");
    if (App_InFineSystem().hasFinale(id))
    {
        return App_InFineSystem().finale(id).requestSkip();
    }
    LOGDEV_SCR_WARNING("Unknown finaleid %i") << id;
    return false;
}

#undef FI_ScriptIsMenuTrigger
dd_bool FI_ScriptIsMenuTrigger(finaleid_t id)
{
    LOG_AS("InFine.ScriptIsMenuTrigger");
    if (App_InFineSystem().hasFinale(id))
    {
        return App_InFineSystem().finale(id).isMenuTrigger();
    }
    LOGDEV_SCR_WARNING("Unknown finaleid %i") << id;
    return false;
}

DE_DECLARE_API(Infine) =
{
    { DE_API_INFINE },
    FI_Execute2,
    FI_Execute,
    FI_ScriptActive,
    FI_ScriptFlags,
    FI_ScriptTerminate,
    FI_ScriptSuspend,
    FI_ScriptResume,
    FI_ScriptSuspended,
    FI_ScriptRequestSkip,
    FI_ScriptCmdExecuted,
    FI_ScriptIsMenuTrigger,
    FI_ScriptResponder
};
