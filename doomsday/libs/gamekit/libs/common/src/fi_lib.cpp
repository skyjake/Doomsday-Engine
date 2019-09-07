/** @file fi_lib.cpp  InFine Helper routines and LIFO "script stack".
 *
 * @authors Copyright © 2006-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2015 Daniel Swanson <danij@dengine.net>
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
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#include "common.h"
#include "fi_lib.h"

#include <cstring>
#include <doomsday/defs/episode.h>
#include "d_net.h"
#include "g_common.h"
#include "g_defs.h"
#include "gamesession.h"
#include "hu_stuff.h"
#include "p_sound.h"
#include "p_tick.h"
#include "r_common.h"

using namespace de;
using namespace common;

struct fi_state_conditions_t
{
    byte secret:1;
    byte leave_hub:1;
};

struct fi_state_t
{
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
};

int Hook_FinaleScriptStop(int hookType, int finaleId, void *context);
int Hook_FinaleScriptTicker(int hookType, int finalId, void *context);
int Hook_FinaleScriptEvalIf(int hookType, int finaleId, void *context);

/// Script state stack.
static dd_bool finaleStackInited;
static uint finaleStackSize;
static fi_state_t *finaleStack;
static fi_state_t remoteFinaleState; // For the client.

static void initStateConditions(fi_state_t &s)
{
    // Set the presets.
    s.conditions.secret = false;
#if !__JHEXEN__
    s.conditions.leave_hub = false;
#endif

    // Only the server is able to figure out the truth values of all the conditions.
    if(IS_CLIENT) return;

#if __JHEXEN__
    s.conditions.secret = false;
#else
    s.conditions.secret = ::secretExit;
#endif

#if __JHEXEN__
    // Leaving the current hub?
    if(const Record *episodeDef = gfw_Session()->episodeDef())
    {
        defn::Episode epsd(*episodeDef);
        const Record *currentHub = epsd.tryFindHubByMapId(gfw_Session()->mapUri().compose());
        s.conditions.leave_hub = (!currentHub || currentHub != epsd.tryFindHubByMapId(::nextMapUri.compose()));
    }
    LOGDEV_SCR_VERBOSE("Infine state condition: leave_hub=%i") << s.conditions.leave_hub;
#endif
}

static fi_state_t *stateForFinaleId(finaleid_t id)
{
    if(finaleStackInited)
    {
        for(uint i = 0; i < finaleStackSize; ++i)
        {
            fi_state_t *s = &finaleStack[i];
            if(s->finaleId == id)
            {
                return s;
            }
        }
    }

    if(IS_CLIENT)
    {
        if(remoteFinaleState.finaleId)
        {
            App_Log(DE2_DEV_SCR_XVERBOSE,
                    "stateForFinaleId: Finale %i is remote, using server's state (id %i)",
                                  id, remoteFinaleState.finaleId);

            return &remoteFinaleState;
        }
    }
    return 0;
}

static dd_bool stackHasDefId(const char *defId)
{
    for(uint i = 0; i < finaleStackSize; ++i)
    {
        fi_state_t *s = &finaleStack[i];
        if(!iCmpStrCase(s->defId, defId))
        {
            return true;
        }
    }
    return false;
}

static inline fi_state_t *stackTop()
{
    return (finaleStackSize == 0? 0 : &finaleStack[finaleStackSize-1]);
}

static fi_state_t *stackPush(finaleid_t finaleId, finale_mode_t mode, gamestate_t prevGamestate,
    const char *defId)
{
    finaleStack = (fi_state_t *)Z_Realloc(finaleStack, sizeof(*finaleStack) * ++finaleStackSize, PU_GAMESTATIC);

    fi_state_t *s = &finaleStack[finaleStackSize-1];

    s->finaleId         = finaleId;
    s->mode             = mode;
    s->initialGamestate = prevGamestate;
    if(defId)
    {
        strncpy(s->defId, defId, sizeof(s->defId) - 1);
        s->defId[sizeof(s->defId) - 1] = 0; // terminate
    }
    else
    {
        // Source ID not provided.
        de::zap(s->defId);
    }
    initStateConditions(*s);

    return s;
}

static void NetSv_SendFinaleState(fi_state_t *s)
{
    DE_ASSERT(s != 0);

    writer_s *writer = D_NetWrite();

    // First the flags.
    Writer_WriteByte(writer, s->mode);

    Writer_WriteUInt32(writer, s->finaleId);

    // Then the conditions.
    Writer_WriteByte(writer, 2); // Number of conditions.
    Writer_WriteByte(writer, s->conditions.secret);
    Writer_WriteByte(writer, s->conditions.leave_hub);

    Net_SendPacket(DDSP_ALL_PLAYERS, GPT_FINALE_STATE, Writer_Data(writer), Writer_Size(writer));
}

void NetCl_UpdateFinaleState(reader_s *msg)
{
    DE_ASSERT(msg != 0);

    fi_state_t *s = &remoteFinaleState;

    // Flags.
    s->mode     = finale_mode_t(Reader_ReadByte(msg));
    s->finaleId = Reader_ReadUInt32(msg); // serverside id (local is different)

    // Conditions.
    int numConds = Reader_ReadByte(msg);
    for(int i = 0; i < numConds; ++i)
    {
        byte cond = Reader_ReadByte(msg);
        if(i == 0) s->conditions.secret    = cond;
        if(i == 1) s->conditions.leave_hub = cond;
    }

    LOGDEV_SCR_MSG("NetCl_FinaleState: Updated finale %i: mode %i, secret=%i, leave_hub=%i")
            << s->finaleId << s->mode << s->conditions.secret << s->conditions.leave_hub;
}

void FI_StackInit()
{
    if(finaleStackInited) return;
    finaleStack = 0; finaleStackSize = 0;

    Plug_AddHook(HOOK_FINALE_SCRIPT_STOP,   Hook_FinaleScriptStop);
    Plug_AddHook(HOOK_FINALE_SCRIPT_TICKER, Hook_FinaleScriptTicker);
    Plug_AddHook(HOOK_FINALE_EVAL_IF,       Hook_FinaleScriptEvalIf);

    finaleStackInited = true;
}

void FI_StackShutdown()
{
    if(!finaleStackInited) return;

    // Terminate all scripts on the stack.
    FI_StackClearAll();

    Z_Free(finaleStack);
    finaleStack = 0; finaleStackSize = 0;

    Plug_RemoveHook(HOOK_FINALE_SCRIPT_STOP,   Hook_FinaleScriptStop);
    Plug_RemoveHook(HOOK_FINALE_SCRIPT_TICKER, Hook_FinaleScriptTicker);
    Plug_RemoveHook(HOOK_FINALE_EVAL_IF,       Hook_FinaleScriptEvalIf);

    finaleStackInited = false;
}

void FI_StackExecute(const char *scriptSrc, int flags, finale_mode_t mode)
{
    FI_StackExecuteWithId(scriptSrc, flags, mode, NULL);
}

void FI_StackExecuteWithId(const char *scriptSrc, int flags, finale_mode_t mode, const char *defId)
{
    DE_ASSERT(finaleStackInited);

    // Should we ignore this?
    if(defId && stackHasDefId(defId))
    {
        App_Log(DE2_SCR_NOTE, "Finale ID \"%s\" is already running, won't execute again", defId);
        return;
    }

    const gamestate_t prevGamestate = G_GameState();
    fi_state_t *prevTopScript = stackTop();

    // Configure the predefined fonts.
    ddstring_t setupCmds;
    Str_Init(&setupCmds);
    int fontIdx = 1;
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
    int i;
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

    finaleid_t finaleId = FI_Execute2(scriptSrc, flags, Str_Text(&setupCmds));
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

    fi_state_t *s = stackPush(finaleId, mode, prevGamestate, defId);

    // Do we need to transmit the state conditions to clients?
    if(IS_SERVER && !(flags & FF_LOCAL))
    {
        NetSv_SendFinaleState(s);
    }
}

dd_bool FI_StackActive()
{
    if(!finaleStackInited) Con_Error("FI_StackActive: Not initialized yet!");
    if(fi_state_t *s = stackTop())
    {
        return FI_ScriptActive(s->finaleId);
    }
    return false;
}

static void stackClear(dd_bool ignoreSuspendedScripts)
{
    DE_ASSERT(finaleStackInited);

    if(fi_state_t *s = stackTop())
    {
        if(FI_ScriptActive(s->finaleId))
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
}

void FI_StackClear()
{
    if(!finaleStackInited) Con_Error("FI_StackClear: Not initialized yet!");
    stackClear(true);
}

void FI_StackClearAll()
{
    if(!finaleStackInited) Con_Error("FI_StackClearAll: Not initialized yet!");
    stackClear(false);
}

int Hook_FinaleScriptStop(int /*hookType*/, int finaleId, void * /*context*/)
{
    fi_state_t *s = stateForFinaleId(finaleId);

    if(IS_CLIENT && s == &remoteFinaleState)
    {
        LOGDEV_SCR_MSG("Hook_FinaleScriptStop: Clientside script stopped, clearing remote state");
        de::zap(remoteFinaleState);
        return true;
    }

    if(!s)
    {
        // Finale was not initiated by us...
        return true;
    }

    finale_mode_t mode           = s->mode;
    gamestate_t initialGamestate = s->initialGamestate;

    // Should we go back to NULL?
    if(finaleStackSize > 1)
    {
        // Resume the next script on the stack.
        finaleStack = (fi_state_t *)Z_Realloc(finaleStack, sizeof(*finaleStack) * --finaleStackSize, PU_GAMESTATIC);
        FI_ScriptResume(stackTop()->finaleId);
        return true;
    }

    /*
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

        G_SetGameAction(GA_ENDDEBRIEFING);
    }
    else if(mode == FIMODE_BEFORE) // A briefing has ended.
    {
        // Its time to start the map; cue music and begin!
        S_MapMusic(gfw_Session()->mapUri());
        HU_WakeWidgets(-1 /* all players */);
        G_BeginMap();
        Pause_End(); // skip forced period
    }
    return true;
}

int Hook_FinaleScriptTicker(int /*hookType*/, int finaleId, void *context)
{
    ddhook_finale_script_ticker_paramaters_t *p = static_cast<ddhook_finale_script_ticker_paramaters_t *>(context);
    fi_state_t *s = stateForFinaleId(finaleId);

    if(!s || IS_CLIENT)
    {
        // Finale was not initiated by us, leave it alone.
        return true;
    }

    /*
     * Once the game state changes we suspend ticking of InFine scripts.
     * Additionally, in overlay mode we stop the script if its skippable.
     *
     * Is this really the best place to handle this?
     */
    gamestate_t gamestate = G_GameState();
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
static int playerClassForName(const char *name)
{
    if(name && name[0])
    {
        if(!iCmpStrCase(name, "fighter")) return PCLASS_FIGHTER;
        if(!iCmpStrCase(name, "cleric"))  return PCLASS_CLERIC;
        if(!iCmpStrCase(name, "mage"))    return PCLASS_MAGE;
    }
    return PCLASS_NONE;
}
#endif

int Hook_FinaleScriptEvalIf(int /*hookType*/, int finaleId, void *context)
{
    ddhook_finale_script_evalif_paramaters_t* p = static_cast<ddhook_finale_script_evalif_paramaters_t *>(context);

    fi_state_t *s = stateForFinaleId(finaleId);
    if(!s)
    {
        // Finale was not initiated by us, therefore we have no say in this.
        return false;
    }

    if(!iCmpStrCase(p->token, "secret"))
    {
        // Secret exit was used?
        p->returnVal = s->conditions.secret;
        return true;
    }

    if(!iCmpStrCase(p->token, "deathmatch"))
    {
        p->returnVal = (gfw_Rule(deathmatch) != false);
        return true;
    }

    if(!iCmpStrCase(p->token, "leavehub"))
    {
        // Current hub has been completed?
        p->returnVal = s->conditions.leave_hub;
        return true;
    }

#if __JHEXEN__
    // Player class names.
    int pclass;
    if((pclass = playerClassForName(p->token)) != PCLASS_NONE)
    {
        if(IS_DEDICATED)
        {
            // Always false; no local players on the server.
            p->returnVal = false;
        }
        else
        {
            p->returnVal = (cfg.playerClass[CONSOLEPLAYER] == pclass);
        }
        return true;
    }
#endif

    // Game modes.
    /// @todo The following conditions should be moved into the engine. -dj
    if(!iCmpStrCase(p->token, "shareware"))
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
    if(!iCmpStrCase(p->token, "ultimate"))
    {
        p->returnVal = (gameMode == doom_ultimate);
        return true;
    }
    if(!iCmpStrCase(p->token, "commercial"))
    {
        p->returnVal = (gameModeBits & GM_ANY_DOOM2) != 0;
        return true;
    }
#endif

    return false;
}

int FI_PrivilegedResponder(const void *ev)
{
    if(!finaleStackInited) return false;

    if(IS_CLIENT && DD_GetInteger(DD_CURRENT_CLIENT_FINALE_ID))
    {
        return FI_ScriptResponder(DD_GetInteger(DD_CURRENT_CLIENT_FINALE_ID), ev);
    }

    if(fi_state_t *s = stackTop())
    {
        return FI_ScriptResponder(s->finaleId, ev);
    }

    return false;
}

dd_bool FI_IsMenuTrigger()
{
    if(!finaleStackInited) Con_Error("FI_IsMenuTrigger: Not initialized yet!");
    if(fi_state_t *s = stackTop())
    {
        return FI_ScriptIsMenuTrigger(s->finaleId);
    }
    return false;
}

dd_bool FI_RequestSkip()
{
    if(!finaleStackInited) Con_Error("FI_RequestSkip: Not initialized yet!");
    if(fi_state_t *s = stackTop())
    {
        return FI_ScriptRequestSkip(s->finaleId);
    }
    return false;
}

D_CMD(StartFinale)
{
    DE_UNUSED(src, argc);

    String scriptId = argv[1];

    // Only one active overlay is allowed.
    if(FI_StackActive()) return false;

    if(const Record *finale = Defs().finales.tryFind("id", scriptId))
    {
        G_SetGameAction(GA_NONE);
        FI_StackExecute(finale->gets("script"), FF_LOCAL, FIMODE_OVERLAY);
        return true;
    }

    LOG_SCR_ERROR("Script '%s' is not defined") << scriptId;
    return false;

}

D_CMD(StopFinale)
{
    DE_UNUSED(src, argc, argv);

    if(FI_StackActive())
    {
        // Only 'overlays' can be explictly stopped this way.
        if(fi_state_t *s = stackTop())
        {
            if(s->mode == FIMODE_OVERLAY)
            {
                FI_ScriptTerminate(s->finaleId);
            }
        }
    }

    return true; // Always
}

void FI_StackRegister()
{
    C_CMD("startfinale",    "s",    StartFinale);
    C_CMD("startinf",       "s",    StartFinale);
    C_CMD("stopfinale",     "",     StopFinale);
    C_CMD("stopinf",        "",     StopFinale);
}
