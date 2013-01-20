/** @file fi_main.cpp
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
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

// HEADER FILES ------------------------------------------------------------

#define DENG_NO_API_MACROS_INFINE

#include <de/memory.h>
#include <de/memoryzone.h>

#include "de_console.h"
#include "de_defs.h"
#include "de_graphics.h"
#include "de_resource.h"
#include "de_infine.h"
#include "de_misc.h"
#include "dd_main.h"

#include "ui/finaleinterpreter.h"
#include "network/net_main.h"

#ifdef __SERVER__
#  include "server/sv_infine.h"
#endif

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

/**
 * A Finale instance contains the high-level state of an InFine script.
 *
 * @see FinaleInterpreter (interactive script interpreter)
 *
 * @ingroup InFine
 */
typedef struct {
    /// @see finaleFlags
    int flags;
    /// Unique identifier/reference (chosen automatically).
    finaleid_t id;
    /// Interpreter for this script.
    finaleinterpreter_t* _interpreter;
    /// Interpreter is active?
    boolean active;
} finale_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

finale_t*           P_CreateFinale(void);
void                P_DestroyFinale(finale_t* f);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static boolean inited = false;

/// Scripts.
static uint finalesSize;
static finale_t* finales;

static byte scaleMode = SCALEMODE_SMART_STRETCH;

// CODE --------------------------------------------------------------------

void FI_Register(void)
{
    C_VAR_BYTE("rend-finale-stretch",  &scaleMode, 0, SCALEMODE_FIRST, SCALEMODE_LAST);
}

static finale_t* finalesById(finaleid_t id)
{
    if(id != 0)
    {
        uint i;
        for(i = 0; i < finalesSize; ++i)
        {
            finale_t* f = &finales[i];
            if(f->id == id)
                return f;
        }
    }
    return 0;
}

static void stopFinale(finale_t* f)
{
    if(!f || !f->active)
        return;
    f->active = false;
    P_DestroyFinaleInterpreter(f->_interpreter);
}

/**
 * @return  A new (unused) unique script id.
 */
static finaleid_t finalesUniqueId(void)
{
    finaleid_t id = 0;
    while(finalesById(++id));
    return id;
}

finale_t* P_CreateFinale(void)
{
    finale_t* f;
    finales = (finale_t *) Z_Realloc(finales, sizeof(*finales) * ++finalesSize, PU_APPSTATIC);
    f = &finales[finalesSize-1];
    f->id = finalesUniqueId();
    f->_interpreter = P_CreateFinaleInterpreter();
    f->_interpreter->_id = f->id;
    f->active = true;
    return f;
}

void P_DestroyFinale(finale_t* f)
{
    assert(f);
    {uint i;
    for(i = 0; i < finalesSize; ++i)
    {
        if(&finales[i] != f)
            continue;

        if(i != finalesSize-1)
            memmove(&finales[i], &finales[i+1], sizeof(*finales) * (finalesSize-i));

        if(finalesSize > 1)
        {
            finales = (finale_t *) Z_Realloc(finales, sizeof(*finales) * --finalesSize, PU_APPSTATIC);
        }
        else
        {
            Z_Free(finales); finales = 0;
            finalesSize = 0;
        }
        break;
    }}
}

boolean FI_ScriptRequestSkip(finaleid_t id)
{
    finale_t* f;
    if(!inited)
        Con_Error("FI_ScriptRequestSkip: Not initialized yet!");
    f = finalesById(id);
    if(!f)
    {
        Con_Message("FI_ScriptRequestSkip: Unknown finaleid %u.", id);
        return false;
    }
    return FinaleInterpreter_Skip(f->_interpreter);
}

int FI_ScriptFlags(finaleid_t id)
{
    finale_t* f;
    if(!inited)
        Con_Error("FI_ScriptFlags: Not initialized yet!");
    f = finalesById(id);
    if(!f)
        Con_Error("FI_ScriptFlags: Unknown finaleid %u.", id);
    return f->flags;
}

boolean FI_ScriptIsMenuTrigger(finaleid_t id)
{
    finale_t* f;
    if(!inited)
        Con_Error("FI_ScriptIsMenuTrigger: Not initialized yet!");
    f = finalesById(id);
    if(!f)
        Con_Error("FI_ScriptIsMenuTrigger: Unknown finaleid %u.", id);
    if(f->active)
    {
        DEBUG_Message(("IsMenuTrigger: %i\n", FinaleInterpreter_IsMenuTrigger(f->_interpreter)));
        return FinaleInterpreter_IsMenuTrigger(f->_interpreter);
    }
    return false;
}

boolean FI_ScriptActive(finaleid_t id)
{
    finale_t* f;
    if(!inited)
    {
#ifdef _DEBUG
        Con_Printf("FI_ScriptActive: Not initialized yet!\n");
#endif
        return false;
    }
    f = finalesById(id);
    if(!f)
        Con_Error("FI_ScriptActive: Unknown finaleid %u.", id);
    return (f->active != 0);
}

void FI_Init(void)
{
    if(inited)
        return; // Already been here.
    finales = 0; finalesSize = 0;

    B_SetContextFallbackForDDEvents("finale", (int (*)(const ddevent_t*)) gx.FinaleResponder);
    B_ActivateContext(B_ContextByName("finale"), true); // always on

    inited = true;
}

void FI_Shutdown(void)
{
    if(!inited)
        return; // Huh?

    if(finalesSize)
    {
        uint i;
        for(i = 0; i < finalesSize; ++i)
        {
            finale_t* f = &finales[i];
            P_DestroyFinaleInterpreter(f->_interpreter);
        }
        Z_Free(finales);
    }
    finales = 0; finalesSize = 0;

    B_SetContextFallbackForDDEvents("finale", NULL);
    B_ActivateContext(B_ContextByName("finale"), false);

    inited = false;
}

boolean FI_ScriptCmdExecuted(finaleid_t id)
{
    finale_t* f;
    if(!inited)
    {
#ifdef _DEBUG
        Con_Printf("FI_ScriptCmdExecuted: Not initialized yet!\n");
#endif
        return false;
    }
    f = finalesById(id);
    if(!f)
        Con_Error("FI_ScriptCmdExecuted: Unknown finaleid %u.", id);
    return FinaleInterpreter_CommandExecuted(f->_interpreter);
}

finaleid_t FI_Execute2(const char* _script, int flags, const char* setupCmds)
{
    const char* script = _script;
    char* tempScript = 0;
    finale_t* f;

    if(!inited)
    {
#ifdef _DEBUG
        Con_Printf("FI_Execute: Not initialized yet!\n");
#endif
        return 0;
    }
    if(!script || !script[0])
    {
#ifdef _DEBUG
        Con_Printf("FI_Execute: Warning, attempt to play empty script.\n");
#endif
        return 0;
    }
    if((flags & FF_LOCAL) && isDedicated)
    {   // Dedicated servers do not play local Finales.
#ifdef _DEBUG
        Con_Printf("FI_Execute: No local finales in dedicated mode.\n");
#endif
        return 0;
    }

    if(setupCmds && setupCmds[0])
    {
        // Setup commands are included. We must prepend these to the script
        // in a special control block that will be executed immediately.
        size_t setupCmdsLen = strlen(setupCmds);
        size_t scriptLen = strlen(script);
        char* p;

        p = tempScript = (char *) M_Malloc(scriptLen + setupCmdsLen + 9 + 2 + 1);
        strcpy(p, "OnLoad {\n"); p += 9;
        memcpy(p, setupCmds, setupCmdsLen); p += setupCmdsLen;
        strcpy(p, "}\n"); p += 2;
        memcpy(p, script, scriptLen); p += scriptLen;
        *p = '\0';
        script = tempScript;
    }

    f = P_CreateFinale();
    f->flags = flags;
    FinaleInterpreter_LoadScript(f->_interpreter, script);

#ifdef __SERVER__
    if(!(flags & FF_LOCAL) && isServer)
    {
        // Instruct clients to start playing this Finale.
        Sv_Finale(f->id, FINF_BEGIN | FINF_SCRIPT, script);
    }
#endif

#ifdef _DEBUG
    Con_Printf("Finale Begin - id:%u '%.30s'\n", f->id, _script);
#endif
    if(tempScript)
    {
        M_Free(tempScript);
    }
    return f->id;
}

finaleid_t FI_Execute(const char* script, int flags)
{
    return FI_Execute2(script, flags, 0);
}

void FI_ScriptTerminate(finaleid_t id)
{
    finale_t* f;
    if(!inited)
    {
#ifdef _DEBUG
        Con_Printf("FI_ScriptTerminate: Not initialized yet!\n");
#endif
        return;
    }
    f = finalesById(id);
    if(!f)
    {
        Con_Message("FI_ScriptTerminate: Unknown finale %u.\n", id);
        return;
    }
    if(f->active)
    {
        stopFinale(f);
        P_DestroyFinale(f);
    }
}

void FI_Ticker(void)
{
    if(!DD_IsSharpTick())
        return;

    // A new 'sharp' tick has begun.

    // All finales tic unless inactive.
    { uint i;
    for(i = 0; i < finalesSize; ++i)
    {
        finale_t* f = &finales[i];
        if(!f->active)
            continue;
        if(FinaleInterpreter_RunTic(f->_interpreter))
        {   // The script has ended!
            stopFinale(f);
            P_DestroyFinale(f);
        }
    }}
}

void FI_ScriptSuspend(finaleid_t id)
{
    finale_t* f;
    if(!inited)
        Con_Error("FI_ScriptSuspend: Not initialized yet!");
    f = finalesById(id);
    if(!f)
        Con_Error("FI_ScriptSuspend: Unknown finaleid %u.", id);
    f->active = false;
    FinaleInterpreter_Suspend(f->_interpreter);
}

void FI_ScriptResume(finaleid_t id)
{
    finale_t* f;
    if(!inited)
        Con_Error("FI_ScriptResume: Not initialized yet!");
    f = finalesById(id);
    if(!f)
        Con_Error("FI_ScriptResume: Unknown finaleid %u.", id);
    f->active = true;
    FinaleInterpreter_Resume(f->_interpreter);
}

boolean FI_ScriptSuspended(finaleid_t id)
{
    finale_t* f;
    if(!inited)
        Con_Error("FI_ScriptSuspended: Not initialized yet!");
    f = finalesById(id);
    if(!f)
        Con_Error("FI_ScriptSuspended: Unknown finaleid %u.", id);
    return FinaleInterpreter_IsSuspended(f->_interpreter);
}

int FI_ScriptResponder(finaleid_t id, const void* ev)
{
    finale_t* f;
    if(!inited)
        Con_Error("FI_ScriptResponder: Not initialized yet!");
    f = finalesById(id);
    if(!f)
    {
        Con_Message("FI_ScriptResponder: Unknown finaleid %u.\n", id);
        return false;
    }
    if(f->active)
        return FinaleInterpreter_Responder(f->_interpreter, (const ddevent_t*)ev);
    return false;
}

DENG_DECLARE_API(Infine) =
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
