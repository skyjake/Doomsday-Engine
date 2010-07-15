/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2006-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2009 Daniel Swanson <danij@dengine.net>
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
 * fi_lib.c: Common game-side implemention for the InFine API.
 */

#include <stdio.h>
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

int Hook_FinaleScriptSerializeExtraData(int hookType, int param, const void* paramaters);
int Hook_FinaleScriptDeserializeExtraData(int hookType, int flags, const void* paramaters);
int Hook_FinaleScriptEvalIf(int hookType, int param, void* paramaters);

int Hook_FinaleScriptStart(int hookType, int mode, void* paramaters);
int Hook_FinaleScriptStop(int hookType, int oldMode, void* paramaters);
int Hook_FinaleScriptTicker(int hookType, int mode, void* paramaters);

int Hook_DemoStop(int hookType, int params, const void* paramaters);

void FI_RegisterHooks(void)
{
    Plug_AddHook(HOOK_FINALE_SCRIPT_SERIALIZE_EXTRADATA, Hook_FinaleScriptSerializeExtraData);
    Plug_AddHook(HOOK_FINALE_SCRIPT_DESERIALIZE_EXTRADATA, Hook_FinaleScriptDeserializeExtraData);
    Plug_AddHook(HOOK_FINALE_EVAL_IF, Hook_FinaleScriptEvalIf);

    Plug_AddHook(HOOK_FINALE_SCRIPT_BEGIN, Hook_FinaleScriptStart);
    Plug_AddHook(HOOK_FINALE_SCRIPT_TERMINATE, Hook_FinaleScriptStop);
    Plug_AddHook(HOOK_FINALE_SCRIPT_TICKER, Hook_FinaleScriptTicker);
}

int FI_GetGameState(void)
{
    return G_GetGameState();
}

void FI_DemoEnds(void)
{
    G_ChangeGameState(GS_INFINE);
    G_SetGameAction(GA_NONE);

    {int i;
    for(i = 0; i < MAXPLAYERS; ++i)
        AM_Open(AM_MapForPlayer(i), false, true);
    }
}

int Hook_FinaleScriptStart(int hookType, int mode, void* unused)
{
    uint i;
   
    for(i = 0; i < MAXPLAYERS; ++i)
    {
        // Clear the message queue for all local players.
        Hu_LogEmpty(i);

        // Close the automap for all local players.
        AM_Open(AM_MapForPlayer(i), false, true);
    }

    return true;
}

int Hook_FinaleScriptStop(int hookType, int mode, void* paramaters)
{
    const ddhook_finale_script_stop_paramaters_t* p = (const ddhook_finale_script_stop_paramaters_t*) paramaters;
    finale_extradata_t* extraData = (finale_extradata_t*) p->extraData;

    if(FI_Active())
        return true;

    /**
     * No more scripts are left.
     */

    // Return to the previous game state?
    if(mode == FIMODE_LOCAL)
    {
        G_ChangeGameState(p->initialGameState);
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

int Hook_FinaleScriptTicker(int hookType, int mode, void* paramaters)
{
    ddhook_finale_script_ticker_paramaters_t* p = (ddhook_finale_script_ticker_paramaters_t*) paramaters;
    gamestate_t gameState = G_GetGameState();

    /**
     * Once the game state changes we suspend ticking of InFine scripts.
     * Additionally, in overlay mode we stop the script if its skippable.
     *
     * Is this really the best place to handle this?
     */
    if(gameState != GS_INFINE && p->gameState != gameState)
    {
        // Overlay scripts don't survive this...
        if(mode == FIMODE_OVERLAY && p->canSkip)
        {
            FI_ScriptTerminate();
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

int Hook_FinaleScriptEvalIf(int hookType, int unused, void* paramaters)
{
    ddhook_finale_script_evalif_paramaters_t* p = (ddhook_finale_script_evalif_paramaters_t*) paramaters;
    finale_extradata_t* data = p->extraData;

    if(!stricmp(p->token, "secret"))
    {   // Secret exit was used?
        p->returnVal = data->secret;
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
        p->returnVal = data->leavehub;
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

int Hook_FinaleScriptSerializeExtraData(int hookType, int flags, const void* paramaters)
{
    ddhook_finale_script_serialize_extradata_t* p = (ddhook_finale_script_serialize_extradata_t*) paramaters;
    const finale_extradata_t* data = (const finale_extradata_t*) p->extraData;
    byte* ptr = p->outBuf;

    *ptr++ = data->secret;
    *ptr++ = data->leavehub;
    p->outBufSize = 2;
    
    return true;
}

int Hook_FinaleScriptDeserializeExtraData(int hookType, int flags, const void* paramaters)
{
    const ddhook_finale_script_deserialize_extradata_t* p = (const ddhook_finale_script_deserialize_extradata_t*) paramaters;
    const byte* ptr = p->inBuf;
    finale_extradata_t* data = (finale_extradata_t*) p->extraData;

    data->secret = *ptr++;
    if(flags & FIRCF_LEAVEHUB)
        data->leavehub = *ptr;

    *p->gameState = G_GetGameState();

    return true;
}
