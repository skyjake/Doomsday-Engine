/**\file fi_lib.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2006-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2012 Daniel Swanson <danij@dengine.net>
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
#include <assert.h>

#include "common.h"
#include "p_sound.h"
#include "p_tick.h"
#include "hu_log.h"
#include "am_map.h"
#include "g_common.h"
#include "r_common.h"
#include "d_net.h"

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

    /**
     * Optionally the ID of the source script definition. A new script is
     * not started if its definition ID matches one already on the stack.
     * @note Maximum ID length defined in the DED Reader implementation.
     */
    char defId[64];
} fi_state_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

D_CMD(StartFinale);
D_CMD(StopFinale);

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
static fi_state_t remoteFinaleState; // For the client.

// Console commands for this library:
static ccmdtemplate_t ccmds[] = {
    { "startfinale",    "s",    CCmdStartFinale },
    { "startinf",       "s",    CCmdStartFinale },
    { "stopfinale",     "",     CCmdStopFinale },
    { "stopinf",        "",     CCmdStopFinale },
    { NULL }
};

// CODE --------------------------------------------------------------------

void FI_StackRegister(void)
{
    int i;
    for(i = 0; ccmds[i].name; ++i)
        Con_AddCommand(ccmds + i);
}

static void initStateConditions(fi_state_t* s)
{
    // Only the server is able to figure out the truth values of all the conditions.
    if(IS_CLIENT)
    {
        // Set the presets.
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

    if(IS_CLIENT)
    {
        if(remoteFinaleState.finaleId)
        {
#ifdef _DEBUG
            VERBOSE2( Con_Message("stateForFinaleId: Finale %i is remote, using server's state (id %i).\n",
                                  id, remoteFinaleState.finaleId) );
#endif
            return &remoteFinaleState;
        }
    }
    return 0;
}

static boolean stackHasDefId(const char* defId)
{
    uint i;
    for(i = 0; i < finaleStackSize; ++i)
    {
        fi_state_t* s = &finaleStack[i];
        if(!stricmp(s->defId, defId))
            return true;
    }
    return false;
}

static __inline fi_state_t* stackTop(void)
{
    return (finaleStackSize == 0? 0 : &finaleStack[finaleStackSize-1]);
}

static fi_state_t* stackPush(finaleid_t finaleId, finale_mode_t mode, gamestate_t prevGamestate,
                             const char* defId)
{
    fi_state_t* s;
    finaleStack = Z_Realloc(finaleStack, sizeof(*finaleStack) * ++finaleStackSize, PU_GAMESTATIC);
    s = &finaleStack[finaleStackSize-1];
    s->finaleId = finaleId;
    s->mode = mode;
    s->initialGamestate = prevGamestate;
    if(defId)
    {
        (void) strncpy(s->defId, defId, sizeof(s->defId) - 1);
        s->defId[sizeof(s->defId) - 1] = 0; // terminate
    }
    else
    {
        // Source ID not provided.
        memset(s->defId, 0, sizeof(s->defId));
    }
    initStateConditions(s);
    return s;
}

static void NetSv_SendFinaleState(fi_state_t* s)
{
    Writer* writer = D_NetWrite();

    // First the flags.
    Writer_WriteByte(writer, s->mode);

    Writer_WriteUInt32(writer, s->finaleId);

    // Then the conditions.
    Writer_WriteByte(writer, 2); // Number of conditions.
    Writer_WriteByte(writer, s->conditions.secret);
    Writer_WriteByte(writer, s->conditions.leave_hub);

    Net_SendPacket(DDSP_ALL_PLAYERS, GPT_FINALE_STATE, Writer_Data(writer), Writer_Size(writer));
}

void NetCl_UpdateFinaleState(Reader* msg)
{
    int i, numConds;
    fi_state_t* s = &remoteFinaleState;

    // Flags.
    s->mode = Reader_ReadByte(msg);

    s->finaleId = Reader_ReadUInt32(msg); // serverside id (local is different)

    // Conditions.
    numConds = Reader_ReadByte(msg);
    for(i = 0; i < numConds; ++i)
    {
        byte cond = Reader_ReadByte(msg);
        if(i == 0) s->conditions.secret = cond;
        if(i == 1) s->conditions.leave_hub = cond;
    }

#ifdef _DEBUG
    Con_Message("NetCl_FinaleState: Updated finale %i: mode %i, secret=%i, leave_hud=%i\n",
                s->finaleId, s->mode, s->conditions.secret, s->conditions.leave_hub);
#endif
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

    // Terminate all scripts on the stack.
    FI_StackClearAll();

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
    FI_StackExecuteWithId(scriptSrc, flags, mode, NULL);
}

void FI_StackExecuteWithId(const char* scriptSrc, int flags, finale_mode_t mode, const char* defId)
{
    fi_state_t* s, *prevTopScript;
    gamestate_t prevGamestate;
    ddstring_t setupCmds;
    finaleid_t finaleId;
    int i, fontIdx;

    if(!finaleStackInited) Con_Error("FI_StackExecute: Not initialized yet!");

    // Should we ignore this?
    if(defId && stackHasDefId(defId))
    {
        Con_Message("There already is a finale running with ID \"%s\"; won't execute again.\n", defId);
        return;
    }

    prevGamestate = G_GameState();
    prevTopScript = stackTop();

    // Configure the predefined fonts.
    Str_Init(&setupCmds);
    fontIdx = 1;
    Str_Appendf(&setupCmds,   "prefont %i %s", fontIdx++, "a");
    Str_Appendf(&setupCmds, "\nprefont %i %s", fontIdx++, "b");
    Str_Appendf(&setupCmds, "\nprefont %i %s", fontIdx++, "status");
#if __JDOOM__
    Str_Appendf(&setupCmds, "\nprefont %i %s", fontIdx++, "index");
#endif
#if __JDOOM__ || __JDOOM64__
    Str_Appendf(&setupCmds, "\nprefont %i %s", fontIdx++, "small");
#endif
#if __JHERETIC__ || __JHEXEN__
    Str_Appendf(&setupCmds, "\nprefont %i %s", fontIdx++, "smallin");
#endif

    // Configure the predefined colors.
#if __JDOOM__
    Str_Appendf(&setupCmds, "\nprecolor 2 %f %f %f\n", defFontRGB[CR],  defFontRGB[CG],  defFontRGB[CB]);
    Str_Appendf(&setupCmds, "\nprecolor 1 %f %f %f\n", defFontRGB2[CR], defFontRGB2[CG], defFontRGB2[CB]);
    Str_Appendf(&setupCmds, "\nprecolor 3 %f %f %f\n", defFontRGB3[CR], defFontRGB3[CG], defFontRGB3[CB]);
    i = 4;
#elif __JHERETIC__
    Str_Appendf(&setupCmds, "\nprecolor 3 %f %f %f\n", defFontRGB[CR],  defFontRGB[CG],  defFontRGB[CB]);
    Str_Appendf(&setupCmds, "\nprecolor 2 %f %f %f\n", defFontRGB2[CR], defFontRGB2[CG], defFontRGB2[CB]);
    Str_Appendf(&setupCmds, "\nprecolor 1 %f %f %f\n", defFontRGB3[CR], defFontRGB3[CG], defFontRGB3[CB]);
    i = 4;
#elif __JHEXEN__
    Str_Appendf(&setupCmds, "\nprecolor 3 %f %f %f\n", defFontRGB[CR],  defFontRGB[CG],  defFontRGB[CB]);
    Str_Appendf(&setupCmds, "\nprecolor 2 %f %f %f\n", defFontRGB2[CR], defFontRGB2[CG], defFontRGB2[CB]);
    Str_Appendf(&setupCmds, "\nprecolor 1 %f %f %f\n", defFontRGB3[CR], defFontRGB3[CG], defFontRGB3[CB]);
    i = 4;
#elif __JDOOM64__
    Str_Appendf(&setupCmds, "\nprecolor 2 %f %f %f\n", defFontRGB[CR],  defFontRGB[CG],  defFontRGB[CB]);
    Str_Appendf(&setupCmds, "\nprecolor 1 %f %f %f\n", defFontRGB2[CR], defFontRGB2[CG], defFontRGB2[CB]);
    i = 3;
#else
    i = 0;
#endif
    // Set the rest to white.
    for(; i <= FIPAGE_NUM_PREDEFINED_COLORS; ++i)
    {
        Str_Appendf(&setupCmds, "\nprecolor %i 1 1 1\n", i);
    }

    finaleId = FI_Execute2(scriptSrc, flags, Str_Text(&setupCmds));
    Str_Free(&setupCmds);
    if(finaleId == 0)
        return;

    if(mode != FIMODE_OVERLAY)
    {
        G_ChangeGameState(GS_INFINE);
    }

    // Only the top-most script can be "active".
    if(prevTopScript)
    {
        FI_ScriptSuspend(prevTopScript->finaleId);
    }

    s = stackPush(finaleId, mode, prevGamestate, defId);

    // Do we need to transmit the state conditions to clients?
    if(IS_SERVER && !(flags & FF_LOCAL))
    {
        NetSv_SendFinaleState(s);
    }
}

boolean FI_StackActive(void)
{
    fi_state_t* s;
    if(!finaleStackInited) Con_Error("FI_StackActive: Not initialized yet!");
    if((s = stackTop()))
    {
        return FI_ScriptActive(s->finaleId);
    }
    return false;
}

static void stackClear(boolean ignoreSuspendedScripts)
{
    fi_state_t* s;
    assert(finaleStackInited);
    if((s = stackTop()) && FI_ScriptActive(s->finaleId))
    {
        // The state is suspended when the PlayDemo command is used.
        // Being suspended means that InFine is currently not active, but
        // will be restored at a later time.
        if(ignoreSuspendedScripts && FI_ScriptSuspended(s->finaleId))
            return;

        // Pop all the states.
        while((s = stackTop()))
        {

            FI_ScriptTerminate(s->finaleId);
        }
    }
}

void FI_StackClear(void)
{
    if(!finaleStackInited) Con_Error("FI_StackClear: Not initialized yet!");
    stackClear(true);
}

void FI_StackClearAll(void)
{
    if(!finaleStackInited) Con_Error("FI_StackClearAll: Not initialized yet!");
    stackClear(false);
}

int Hook_FinaleScriptStop(int hookType, int finaleId, void* parameters)
{
    gamestate_t initialGamestate;
    finale_mode_t mode;
    fi_state_t* s = stateForFinaleId(finaleId);

    DENG_UNUSED(hookType);
    DENG_UNUSED(parameters);

    if(IS_CLIENT && s == &remoteFinaleState)
    {
#ifdef _DEBUG
        Con_Message("Hook_FinaleScriptStop: Clientside script stopped, clearing remote state.\n");
        memset(&remoteFinaleState, 0, sizeof(remoteFinaleState));
#endif
        return true;
    }

    if(!s)
    {
        // Finale was not initiated by us...
        return true;
    }
    initialGamestate = s->initialGamestate;
    mode = s->mode;

    // Should we go back to NULL?
    if(finaleStackSize > 1)
    {   // Resume the next script on the stack.
        finaleStack = Z_Realloc(finaleStack, sizeof(*finaleStack) * --finaleStackSize, PU_GAMESTATIC);
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
        if(IS_CLIENT) return true;

        G_SetGameAction(GA_MAPCOMPLETED);
        // Don't play the debriefing again.
        briefDisabled = true;
    }
    else if(mode == FIMODE_BEFORE) // A briefing has ended.
    {
        // Its time to start the map; que music and begin!
        S_MapMusic(gameEpisode, gameMap);
        G_BeginMap();
    }
    return true;
}

int Hook_FinaleScriptTicker(int hookType, int finaleId, void* paramaters)
{
    ddhook_finale_script_ticker_paramaters_t* p = (ddhook_finale_script_ticker_paramaters_t*) paramaters;
    gamestate_t gamestate = G_GameState();
    fi_state_t* s = stateForFinaleId(finaleId);

    if(!s || IS_CLIENT)
    {
        // Finale was not initiated by us, leave it alone.
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
#if __JHEXEN__
    // Player class names.
    int pclass;
#endif

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
        p->returnVal = (deathmatch != false);
        return true;
    }

    if(!stricmp(p->token, "leavehub"))
    {   // Current hub has been completed?
        p->returnVal = s->conditions.leave_hub;
        return true;
    }

#if __JHEXEN__
    // Player class names.
    if((pclass = playerClassForName(p->token)))
    {
        p->returnVal = pclass;
        return true;
    }
#endif

    /**
     * Game modes.
     * \todo dj: the following conditions should be moved into the engine.
     */
    if(!stricmp(p->token, "shareware"))
    {
#if __JDOOM__
        p->returnVal = ((gameMode == doom_shareware) != false);
#elif __JHERETIC__
        p->returnVal = ((gameMode == heretic_shareware) != false);
/*#elif __JHEXEN__
        p->returnVal = ((gameMode == hexen_demo) != false);*/
#else
        p->returnVal = false;
#endif
        return true;
    }
#if __JDOOM__
    if(!stricmp(p->token, "ultimate"))
    {
        p->returnVal = (gameMode == doom_ultimate);
        return true;
    }
    if(!stricmp(p->token, "commercial"))
    {
        p->returnVal = (gameModeBits & GM_ANY_DOOM2) != 0;
        return true;
    }
#endif

    return false;
}

int FI_PrivilegedResponder(const void* ev)
{
    fi_state_t* s;
    if(!finaleStackInited) return false;
    if(IS_CLIENT && DD_GetInteger(DD_CURRENT_CLIENT_FINALE_ID))
    {
        return FI_ScriptResponder(DD_GetInteger(DD_CURRENT_CLIENT_FINALE_ID), ev);
    }
    if((s = stackTop()))
    {
        return FI_ScriptResponder(s->finaleId, ev);
    }
    return false;
}

boolean FI_IsMenuTrigger(void)
{
    fi_state_t* s;
    if(!finaleStackInited) Con_Error("FI_IsMenuTrigger: Not initialized yet!");
    if((s = stackTop()))
    {
        return FI_ScriptIsMenuTrigger(s->finaleId);
    }
    return false;
}

boolean FI_RequestSkip(void)
{
    fi_state_t* s;
    if(!finaleStackInited) Con_Error("FI_RequestSkip: Not initialized yet!");
    if((s = stackTop()))
    {
        return FI_ScriptRequestSkip(s->finaleId);
    }
    return false;
}

D_CMD(StartFinale)
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
    G_SetGameAction(GA_NONE);
    FI_StackExecute(fin.script, FF_LOCAL, FIMODE_OVERLAY);
    return true;
}

D_CMD(StopFinale)
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
