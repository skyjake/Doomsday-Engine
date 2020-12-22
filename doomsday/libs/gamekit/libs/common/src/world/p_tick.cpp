/** @file p_tick.cpp  Common top-level tick stuff.
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
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#include "common.h"
#include "p_tick.h"

#include "gamesession.h"
#include "d_netsv.h"
#include "g_common.h"
#include "g_controls.h"
#include "hu_menu.h"
#include "hu_msg.h"
#include "p_actor.h"
#include "p_user.h"
#include "player.h"
#include "r_common.h"
#include "r_special.h"

using namespace common;

int mapTime;
int actualMapTime;
int timerGame;

void P_RunPlayers(timespan_t ticLength)
{
    for(int i = 0; i < MAXPLAYERS; ++i)
    {
        if(players[i].plr->inGame)
        {
            // The player thinks.
            P_PlayerThink(&players[i], ticLength);
        }
    }
}

void P_DoTick()
{
    Pause_Ticker();

    // If the game is paused, nothing will happen.
    if(paused) return;

    actualMapTime++;

    if(!IS_CLIENT && timerGame && !paused)
    {
        if(!--timerGame)
        {
            G_SetGameActionMapCompleted(gfw_Session()->mapUriForNamedExit("next"));
        }
    }

    // Pause if in menu and at least one tic has been run.
    if(!IS_NETGAME && (Hu_MenuIsActive() || Hu_IsMessageActive()) &&
       !Get(DD_PLAYBACK) && mapTime > 1)
        return;

    Thinker_Run();

#if __JDOOM__ || __JDOOM64__ || __JHERETIC__
    // Extended lines and sectors.
    XG_Ticker();
#endif

#if __JHEXEN__
    P_AnimateLightning();
#endif

#if __JDOOM64__
    P_ThunderSector();
#endif

    P_ProcessDeferredSpawns();

#if __JHERETIC__
    P_AmbientSound();
#endif

    // Let the engine know where the local players are now.
    for(int i = 0; i < MAXPLAYERS; ++i)
    {
        R_UpdateConsoleView(i);
    }

    R_UpdateSpecialFilter(DISPLAYPLAYER);

    // For par times, among other things.
    mapTime++;
}
