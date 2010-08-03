/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2006-2010 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2010 Daniel Swanson <danij@dengine.net>
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

/**
 * Helper routines and LIFO "script stack" functionality for use with
 * Doomsday's InFine API.
 */

// HEADER FILES ------------------------------------------------------------

#include <string.h>

#if __JDOOM__
#  include "jdoom.h"
#elif __JDOOM64__
#  include "jdoom64.h"
#elif __JHERETIC__
#  include "jheretic.h"
#elif __JHEXEN__
#  include "jhexen.h"
#endif

#include "p_tick.h"
#include "hu_log.h"
#include "am_map.h"
#include "g_common.h"

#include "fi_lib.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

typedef struct {
    char secret:1;
    char leave_hub:1;
} fi_state_conditions_t;

typedef struct {
    finaleid_t finaleId;
    finale_mode_t mode;
    fi_state_conditions_t conditions;
    /// Gamestate before the finale began.
    gamestate_t initialGamestate;
} fi_state_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

DEFCC(StartFinale);
DEFCC(StopFinale);

int Hook_FinaleScriptStop(int hookType, int finaleId, void* paramaters);
int Hook_FinaleScriptTicker(int hookType, int finalId, void* paramaters);
int Hook_FinaleScriptEvalIf(int hookType, int finaleId, void* paramaters);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

/// Script state stack.
static boolean finaleStackInited = false;
static uint finaleStackSize = 0;
static fi_state_t* finaleStack = 0;

// Console commands for this library:
static ccmd_t ccmds[] = {
    { "startfinale",    "s",    StartFinale },
    { "startinf",       "s",    StartFinale },
    { "stopfinale",     "",     StopFinale },
    { "stopinf",        "",     StopFinale },
    { NULL }
};

// CODE --------------------------------------------------------------------

void FI_Register(void)
{
    int i;
    for(i = 0; ccmds[i].name; ++i)
        Con_AddCommand(ccmds + i);
}

static void initStateConditions(fi_state_t* s)
{
    // Only the server is able to figure out the truth values of all the conditions.
    if(!IS_SERVER)
    {   // Set the presets.
        s->conditions.secret = false;
        s->conditions.leave_hub = false;
        return;
    }

#if __JHEXEN__
    s->conditions.secret = false;

    // Current hub has been completed?
    s->conditions.leave_hub = (P_GetMapCluster(gameMap) != P_GetMapCluster(nextMap));
#else
    s->conditions.secret = secretExit;
    // Only Hexen has hubs.
    s->conditions.leave_hub = false;
#endif
}

static fi_state_t* stateForFinaleId(finaleid_t id)
{
    if(finaleStackInited)
    {
        uint i;
        for(i = 0; i < finaleStackSize; ++i)
        {
            fi_state_t* s = &finaleStack[i];
            if(s->finaleId == id)
                return s;
        }
    }
    return 0;
}

static __inline fi_state_t* stackTop(void)
{
    return (finaleStackSize == 0? 0 : &finaleStack[finaleStackSize-1]);
}

static fi_state_t* stackPush(finaleid_t finaleId, finale_mode_t mode, gamestate_t prevGamestate)
{
    fi_state_t* s;
    finaleStack = Z_Realloc(finaleStack, sizeof(*finaleStack) * ++finaleStackSize, PU_STATIC);
    s = &finaleStack[finaleStackSize-1];
    s->finaleId = finaleId;
    s->mode = mode;
    s->initialGamestate = prevGamestate;
    initStateConditions(s);
    return s;
}

static void Sv_SendFinaleState(fi_state_t* s)
{
#define BUF_SIZE            4

    byte buffer[BUF_SIZE], *ptr = buffer;

    // First the flags.
    *ptr++ = (s->mode == FIMODE_AFTER ? FINF_AFTER : (s->mode == FIMODE_OVERLAY) ? FINF_OVERLAY : 0);

    // Then the conditions.
    *ptr++ = 2; // Number of conditions.
    *ptr++ = s->conditions.secret;
    *ptr++ = s->conditions.leave_hub;

    Net_SendPacket(DDSP_ALL_PLAYERS | DDSP_ORDERED, 85 /*GPT_FINALE2*/, buffer, BUF_SIZE);

#undef BUF_SIZE
}

void FI_StackInit(void)
{
    if(finaleStackInited) return;
    finaleStack = 0; finaleStackSize = 0;

    Plug_AddHook(HOOK_FINALE_SCRIPT_STOP, Hook_FinaleScriptStop);
    Plug_AddHook(HOOK_FINALE_SCRIPT_TICKER, Hook_FinaleScriptTicker);
    Plug_AddHook(HOOK_FINALE_EVAL_IF, Hook_FinaleScriptEvalIf);

    finaleStackInited = true;
}

void FI_StackShutdown(void)
{
    if(!finaleStackInited) return;
    if(finaleStack)
        Z_Free(finaleStack);
    finaleStack = 0; finaleStackSize = 0;

    Plug_RemoveHook(HOOK_FINALE_SCRIPT_STOP, Hook_FinaleScriptStop);
    Plug_RemoveHook(HOOK_FINALE_SCRIPT_TICKER, Hook_FinaleScriptTicker);
    Plug_RemoveHook(HOOK_FINALE_EVAL_IF, Hook_FinaleScriptEvalIf);

    finaleStackInited = false;
}

void FI_StackExecute(const char* scriptSrc, int flags, finale_mode_t mode)
{
    fi_state_t* s;
    gamestate_t prevGamestate;

    if(!finaleStackInited) Con_Error("FI_StackExecute: Not initialized yet!");

    G_SetGameAction(GA_NONE);
    prevGamestate = G_GetGameState();
    if(mode != FIMODE_OVERLAY)
    {
        G_ChangeGameState(GS_INFINE);
    }

    // Only the top-most script is active.
    if((s = stackTop()))
    {
        FI_ScriptSuspend(s->finaleId);
    }

    s = stackPush(FI_Execute(scriptSrc, flags), mode, prevGamestate);

    // Do we need to transmit the state conditions to clients?
    if(IS_SERVER && !(flags & FF_LOCAL))
    {
        Sv_SendFinaleState(s);
    }
}

boolean FI_StackActive(void)
{
    if(!finaleStackInited) Con_Error("FI_StackActive: Not initialized yet!");
    {fi_state_t* s;
    if((s = stackTop()))
    {
        return FI_ScriptActive(s->finaleId);
    }}
    return false;
}

void FI_StackClear(void)
{
    if(!finaleStackInited) Con_Error("FI_StackClear: Not initialized yet!");
    {fi_state_t* s;
    if((s = stackTop()) && FI_ScriptActive(s->finaleId))
    {
        // The state is suspended when the PlayDemo command is used.
        // Being suspended means that InFine is currently not active, but
        // will be restored at a later time.
        if(FI_ScriptSuspended(s->finaleId))
            return;

        // Pop all the states.
        while((s = stackTop()))
        {
            FI_ScriptTerminate(s->finaleId);
        }
    }}
}

int Hook_FinaleScriptStop(int hookType, int finaleId, void* paramaters)
{
    gamestate_t initialGamestate;
    finale_mode_t mode;
    { fi_state_t* s = stateForFinaleId(finaleId);
    if(!s)
    {   // Finale was not initiated by us.
        return true;
    }
    initialGamestate = s->initialGamestate;
    mode = s->mode; }

    // Should we go back to NULL?
    if(finaleStackSize > 1)
    {   // Resume the next script on the stack.
        finaleStack = Z_Realloc(finaleStack, sizeof(*finaleStack) * --finaleStackSize, PU_STATIC);
        FI_ScriptResume(stackTop()->finaleId);
        return true;
    }

    /**
     * No more scripts are left.
     */
    Z_Free(finaleStack); finaleStack = 0;
    finaleStackSize = 0;

    // Return to the previous game state?
    if(FI_ScriptFlags(finaleId) & FF_LOCAL)
    {
        G_ChangeGameState(initialGamestate);
        return true;
    }

    // Go to the next game mode?
    if(mode == FIMODE_AFTER) // A map has been completed.
    {
        if(IS_CLIENT)
            return true;
        G_SetGameAction(GA_MAPCOMPLETED);
        // Don't play the debriefing again.
        briefDisabled = true;
    }
    else if(mode == FIMODE_BEFORE)
    {
        // Enter the map, this was a briefing.
        G_ChangeGameState(GS_MAP);
        S_MapMusic(gameEpisode, gameMap);
        mapStartTic = (int) GAMETIC;
        mapTime = actualMapTime = 0;
    }
    return true;
}

int Hook_FinaleScriptTicker(int hookType, int finaleId, void* paramaters)
{
    ddhook_finale_script_ticker_paramaters_t* p = (ddhook_finale_script_ticker_paramaters_t*) paramaters;
    gamestate_t gamestate = G_GetGameState();
    fi_state_t* s = stateForFinaleId(finaleId);

    if(!s)
    {   // Finale was not initiated by us, leave it alone.
        return true;
    }

    /**
     * Once the game state changes we suspend ticking of InFine scripts.
     * Additionally, in overlay mode we stop the script if its skippable.
     *
     * Is this really the best place to handle this?
     */
    if(gamestate != GS_INFINE && s->initialGamestate != gamestate)
    {
        // Overlay scripts don't survive this...
        if(s->mode == FIMODE_OVERLAY && p->canSkip)
        {
            FI_ScriptTerminate(s->finaleId);
        }
        p->runTick = false;
    }
    return true;
}

#if __JHEXEN__
static int playerClassForName(const char* name)
{
    if(name && name[0])
    {
        if(!stricmp(name, "fighter"))
            return PCLASS_FIGHTER;
        if(!stricmp(name, "cleric"))
            return PCLASS_CLERIC;
        if(!stricmp(name, "mage"))
            return PCLASS_MAGE;
    }
    return 0;
}
#endif

int Hook_FinaleScriptEvalIf(int hookType, int finaleId, void* paramaters)
{
    ddhook_finale_script_evalif_paramaters_t* p = (ddhook_finale_script_evalif_paramaters_t*) paramaters;
    fi_state_t* s = stateForFinaleId(finaleId);

    if(!s)
    {   // Finale was not initiated by us, therefore we have no say in this.
        return false;
    }

    if(!stricmp(p->token, "secret"))
    {   // Secret exit was used?
        p->returnVal = s->conditions.secret;
        return true;
    }

    if(!stricmp(p->token, "deathmatch"))
    {
        p->returnVal = deathmatch != false;
        return true;
    }

    if(!stricmp(p->token, "shareware"))
    {
#if __JDOOM__
        p->returnVal = (gameMode == shareware);
#elif __JHERETIC__
        p->returnVal = shareware != false;
#else
        p->returnVal = false; // Hexen has no shareware.
#endif
        return true;
    }

    // Generic game mode string check.
    if(!strnicmp(p->token, "mode:", 5))
    {
        p->returnVal = !stricmp(p->token + 5, (char*) G_GetVariable(DD_GAME_MODE));
        return true;
    }

#if __JDOOM__
    // Game modes.
    if(!stricmp(p->token, "ultimate"))
    {
        p->returnVal = (gameMode == retail);
        return true;
    }

    if(!stricmp(p->token, "commercial"))
    {
        p->returnVal = (gameMode == commercial);
        return true;
    }
#endif

    if(!stricmp(p->token, "leavehub"))
    {   // Current hub has been completed?
        p->returnVal = s->conditions.leave_hub;
        return true;
    }

#if __JHEXEN__
    // Player class names.
    {int pclass;
    if((pclass = playerClassForName(p->token)))
    {
        p->returnVal = pclass;
        return true;
    }}
#endif

    return false;
}

boolean FI_Responder(event_t* ev)
{
    if(!finaleStackInited) Con_Error("FI_Responder: Not initialized yet!");
    {fi_state_t* s;
    if((s = stackTop()))
    {
        return FI_ScriptResponder(s->finaleId, ev);
    }}
    return false;
}

boolean FI_IsMenuTrigger(void)
{
    if(!finaleStackInited) Con_Error("FI_IsMenuTrigger: Not initialized yet!");
    {fi_state_t* s;
    if((s = stackTop()))
    {
        return FI_ScriptIsMenuTrigger(s->finaleId);
    }}
    return false;
}

boolean FI_RequestSkip(void)
{
    if(!finaleStackInited) Con_Error("FI_RequestSkip: Not initialized yet!");
    {fi_state_t* s;
    if((s = stackTop()))
    {
        return FI_ScriptRequestSkip(s->finaleId);
    }}
    return false;
}

DEFCC(StartFinale)
{
    ddfinale_t fin;
    // Only one active overlay allowed.
    if(FI_StackActive())
        return false;
    if(!Def_Get(DD_DEF_FINALE, argv[1], &fin))
    {
        Con_Printf("Script '%s' is not defined.\n", argv[1]);
        return false;
    }
    FI_StackExecute(fin.script, FF_LOCAL, FIMODE_OVERLAY);
    return true;
}

DEFCC(StopFinale)
{
    fi_state_t* s;
    if(!FI_StackActive())
        return false;
    // Only 'overlays' can be explictly stopped this way.
    if((s = stackTop()) && s->mode == FIMODE_OVERLAY)
    {
        FI_ScriptTerminate(s->finaleId);
        return true;
    }
    return false;
}
