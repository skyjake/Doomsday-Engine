/**\file
 *\section License
 * License: GPL + jHeretic/jHexen Exception
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2007 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2008 Daniel Swanson <danij@dengine.net>
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
 *
 * In addition, as a special exception, we, the authors of deng
 * give permission to link the code of our release of deng with
 * the libjhexen and/or the libjheretic libraries (or with modified
 * versions of it that use the same license as the libjhexen or
 * libjheretic libraries), and distribute the linked executables.
 * You must obey the GNU General Public License in all respects for
 * all of the code used other than “libjhexen or libjheretic”. If
 * you modify this file, you may extend this exception to your
 * version of the file, but you are not obligated to do so. If you
 * do not wish to do so, delete this exception statement from your version.
 */

/**
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
#include "hu_menu.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

void    P_ClientSideThink(void);

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int levelTime;
int actualLevelTime;
int timerGame;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

/**
 * @return              @c true, if the game is currently paused.
 */
boolean P_IsPaused(void)
{
    return paused || (!IS_NETGAME && Hu_MenuIsActive());
}

/**
 * This is called at all times, no matter gamestate.
 */
void P_RunPlayers(timespan_t ticLength)
{
    uint                i;
    boolean             isPaused = P_IsPaused();
    //ticcmd_t            command;
    //boolean             gotCommands;

    // This is not for clients.
    if(IS_CLIENT)
        return;

    // Each player gets to think one cmd.
    // For the local player, this is always the cmd of the current tick.
    // For remote players, this might be a predicted cmd or a real cmd
    // from the past.
    for(i = 0; i < MAXPLAYERS; ++i)
        if(players[i].plr->inGame)
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
            P_PlayerThink(&players[i], ticLength);
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

    actualLevelTime++;

    if(!IS_CLIENT && timerGame && !paused)
    {
        if(!--timerGame)
        {
#if __JHEXEN__ || __JSTRIFE__
            G_LeaveLevel(G_GetLevelNumber(gameEpisode, P_GetMapNextMap(gameMap)),
                         0, false);
#else
            G_LeaveLevel(G_GetLevelNumber(gameEpisode, gameMap), 0, false);
#endif
        }
    }

    // Pause if in menu and at least one tic has been run.
    if(!IS_NETGAME && Hu_MenuIsActive() && !Get(DD_PLAYBACK) &&
       players[CONSOLEPLAYER].plr->viewZ != 1)
        return;

    P_RunThinkers();
    P_UpdateSpecials();

#if __DOOM64TC__
    P_ThunderSector();
#endif

#if __JDOOM__ || __JSTRIFE__
    P_CheckRespawnQueue();
#elif __JHERETIC__
    P_AmbientSound();
#else
    P_AnimateSurfaces();
#endif

    P_ClientSideThink();

    // For par times, among other things.
    levelTime++;
}
