/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2010 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2010 Daniel Swanson <danij@dengine.net>
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_console.h"
#include "de_play.h"
#include "de_defs.h"
#include "de_graphics.h"
#include "de_refresh.h"
#include "de_render.h"
#include "de_network.h"
#include "de_audio.h"
#include "de_infine.h"
#include "de_misc.h"

#include "finaleinterpreter.h"

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

/// Allow stretching to fill the screen at near 4:3 aspect ratios?
static byte noStretch = false;

// CODE --------------------------------------------------------------------

void FI_Register(void)
{
    C_VAR_BYTE("finale-nostretch",  &noStretch, 0, 0, 1);
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
    finales = Z_Realloc(finales, sizeof(*finales) * ++finalesSize, PU_APPSTATIC);
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
            finales = Z_Realloc(finales, sizeof(*finales) * --finalesSize, PU_APPSTATIC);
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
        Con_Error("FI_ScriptRequestSkip: Unknown finaleid %u.", id);
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

finaleid_t FI_Execute(const char* scriptSrc, int flags)
{
    if(!inited)
    {
#ifdef _DEBUG
        Con_Printf("FI_Execute: Not initialized yet!\n");
#endif
        return 0;
    }
    if(!scriptSrc || !scriptSrc[0])
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

    {finale_t* f = P_CreateFinale();
    f->flags = flags;
    FinaleInterpreter_LoadScript(f->_interpreter, scriptSrc);
    if(!(flags & FF_LOCAL) && isServer)
    {   // Instruct clients to start playing this Finale.
        Sv_Finale(FINF_BEGIN|FINF_SCRIPT, scriptSrc);
    }
#ifdef _DEBUG
    Con_Printf("Finale Begin - id:%u '%.30s'\n", f->id, scriptSrc);
#endif
    return f->id; }
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
        Con_Error("FI_ScriptTerminate: Unknown finaleid %u.", id);
    if(f->active)
    {
        stopFinale(f);
        P_DestroyFinale(f);
    }
}

void FI_Ticker(timespan_t ticLength)
{
    if(!M_CheckTrigger(&sharedFixedTrigger, ticLength))
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
        Con_Error("FI_ScriptResponder: Unknown finaleid %u.", id);
    if(f->active)
        return FinaleInterpreter_Responder(f->_interpreter, (const ddevent_t*)ev);
    return false;
}
