/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2007 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2006 Daniel Swanson <danij@dengine.net>
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

/*
 * p_tick.c: Top-level tick stuff.
 */

// HEADER FILES ------------------------------------------------------------

#if  __DOOM64TC__
#  include "doom64tc.h"
#elif __WOLFTC__
#  include "wolftc.h"
#elif __JDOOM__
#  include "jdoom.h"
#elif __JHERETIC__
#  include "jheretic.h"
#elif __JHEXEN__
#  include "jhexen.h"
#elif __JSTRIFE__
#  include "jstrife.h"
#endif

#include "g_common.h"
#include "g_controls.h"
#include "p_player.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

void    P_ClientSideThink(void);

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int     leveltime;
int     actual_leveltime;
int     TimerGame;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

/**
 * Returns true if the game is currently paused.
 */
boolean P_IsPaused(void)
{
    return paused || (!IS_NETGAME && menuactive);
}

/**
 * This is called at all times, no matter gamestate.
 */
void P_RunPlayers(timespan_t tickDuration)
{
    boolean     isPaused = P_IsPaused();
    uint        i;
    //ticcmd_t    command;
    //boolean     gotCommands;

    // This is not for clients.
    if(IS_CLIENT)
        return;

    // Each player gets to think one cmd.
    // For the local player, this is always the cmd of the current tick.
    // For remote players, this might be a predicted cmd or a real cmd
    // from the past.
    for(i = 0; i < MAXPLAYERS; ++i)
        if(players[i].plr->ingame)
        {
            /*
            // We will combine all the waiting commands into this
            // buffer.
            memset(&command, 0, sizeof(command));

            // Get all the commands for the player.
            gotCommands = false;
            while(Net_GetTicCmd(&players[i].plr->cmd, i))
            {
                P_MergeCommand(&command, &players[i].plr->cmd);
                gotCommands = true;

                // Local players run one tic at a time.
				if(players[i].plr->flags & DDPF_LOCAL)
                    break;
            }

            if(gotCommands)
            {
                // The new merged command will be the one that the
                // player uses for thinking on this tick.
                memcpy(&players[i].plr->cmd, &command, sizeof(command));
            }

            // Check for special buttons (pause and netsave).
            G_SpecialButton(i);
            */
            
            // The player thinks.
            P_PlayerThink(&players[i], tickDuration);
        }
}

/**
 * Called 35 times per second.
 * The heart of play sim.
 */
void P_DoTick(void)
{
    // If the game is paused, nothing will happen.
    if(paused)
        return;

    actual_leveltime++;

    if(!IS_CLIENT && TimerGame && !paused)
    {
        if(!--TimerGame)
        {
#if __JHEXEN__ || __JSTRIFE__
            G_LeaveLevel(G_GetLevelNumber(gameepisode, P_GetMapNextMap(gamemap)),
                         0, false);
#else
            G_LeaveLevel(G_GetLevelNumber(gameepisode, gamemap), 0, false);
#endif
        }
    }

    // pause if in menu and at least one tic has been run
    if(!IS_NETGAME && menuactive && !Get(DD_PLAYBACK) &&
       players[consoleplayer].plr->viewz != 1)
        return;


    P_RunThinkers();
    P_UpdateSpecials();

#if __DOOM64TC__
    P_ThunderSector();
#endif

#if __JDOOM__ || __JSTRIFE__
    P_RespawnSpecials();
#elif __JHERETIC__
    P_AmbientSound();
#else
    P_AnimateSurfaces();
#endif

    P_ClientSideThink();

    // For par times, among other things.
    leveltime++;
}
