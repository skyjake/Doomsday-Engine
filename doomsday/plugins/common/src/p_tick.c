/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
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
 * p_tick.c: Top-level tick stuff.
 */

// HEADER FILES ------------------------------------------------------------

#if __JDOOM__
#  include "jdoom.h"
#elif __JDOOM64__
#  include "jdoom64.h"
#elif __JHERETIC__
#  include "jheretic.h"
#elif __JHEXEN__
#  include "jhexen.h"
#endif

#include "p_actor.h"
#include "g_common.h"
#include "g_controls.h"
#include "player.h"
#include "p_user.h"
#include "hu_menu.h"
#include "hu_msg.h"
#include "d_netsv.h"
#include "r_common.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int mapTime;
timespan_t mapTimef;
int actualMapTime;
int timerGame;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

void P_RunPlayers(timespan_t ticLength)
{
    uint i;

    mapTimef += ticLength * TICRATE;

    for(i = 0; i < MAXPLAYERS; ++i)
        if(players[i].plr->inGame)
        {
            // The player thinks.
            P_PlayerThink(&players[i], ticLength);
        }
}

static boolean allowedToRunThinkers()
{
    // Pause if in menu and at least one tic has been run.
    if(!IS_NETGAME && (Hu_MenuIsActive() || Hu_IsMessageActive()) &&
       !Get(DD_PLAYBACK) && mapTime > 1)
        return false;

    return true;
}

static int playerMobjsOnly(thinker_t const *thinker)
{
    if(thinker->function == (thinkfunc_t) P_MobjThinker)
    {
        mobj_t const *mobj = (mobj_t const *) thinker;
        return Mobj_IsPlayer(mobj) && !Mobj_IsVoodooDoll(mobj);
    }
    return false;
}

static int excludePlayerMobjs(thinker_t const *thinker)
{
    return !playerMobjsOnly(thinker);
}

void P_RunThinkersForPlayerMobjs(timespan_t ticLength)
{
    if(IS_DEDICATED) return;

    DENG_ASSERT( !Get(DD_VANILLA_INPUT) );

    // When paused, do nothing.
    if(paused || !allowedToRunThinkers())
        return;

    DD_SetInteger(DD_GAME_TICK_DURATION, ticLength * TICRATE * 1000000);
    Thinker_RunFiltered(playerMobjsOnly);
}

/**
 * Called 35 times per second.
 * The heart of play sim.
 */
void P_DoTick(void)
{
    int i;

    DENG_ASSERT(DD_IsSharpTick());

    Pause_Ticker();

    // If the game is paused, nothing will happen.
    if(paused) return;

    actualMapTime++;

    if(!IS_CLIENT && timerGame && !paused)
    {
        if(!--timerGame)
        {
            G_LeaveMap(G_NextLogicalMapNumber(false), 0, false);
        }
    }

    // Pause if in menu and at least one tic has been run.
    if(!allowedToRunThinkers())
        return;

    DD_SetInteger(DD_GAME_TICK_DURATION, 1000000 /* single 35 Hz tick */);
    if(Get(DD_VANILLA_INPUT))
    {
        Thinker_Run();
    }
    else
    {
        Thinker_RunFiltered(excludePlayerMobjs);
    }

#if __JDOOM__ || __JDOOM64__ || __JHERETIC__
    // Extended lines and sectors.
    XG_Ticker();
#endif

#if __JHEXEN__
    P_AnimateSky();
    P_AnimateLightning();
#endif

#if __JDOOM64__
    P_ThunderSector();
#endif

    P_ProcessDeferredSpawns();

#if __JHERETIC__
    P_AmbientSound();
#endif

#ifdef __JDOOM__
    G_UpdateSpecialFilter(DISPLAYPLAYER);
#endif

    // For par times, among other things.
    mapTime++;
}
