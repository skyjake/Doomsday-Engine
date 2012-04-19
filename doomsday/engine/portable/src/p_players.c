/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * p_players.c: Players
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_play.h"
#include "de_refresh.h"
#include "de_network.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

player_t* viewPlayer;
player_t ddPlayers[DDMAXPLAYERS];
int consolePlayer;
int displayPlayer;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

/**
 * Determine which console is used by the given local player. Local players
 * are numbered starting from zero.
 */
int P_LocalToConsole(int localPlayer)
{
    int                 i, count;

    for(i = 0, count = 0; i < DDMAXPLAYERS; ++i)
    {
        int console = (i + consolePlayer) % DDMAXPLAYERS;
        player_t*           plr = &ddPlayers[console];
        ddplayer_t*         ddpl = &plr->shared;

        if(ddpl->flags & DDPF_LOCAL)
        {
            if(count++ == localPlayer)
                return console;
        }
    }

    // No match!
    return -1;
}

/**
 * Determine the local player number used by a particular console. Local
 * players are numbered starting from zero.
 */
int P_ConsoleToLocal(int playerNum)
{
    int                 i, count = 0;
    player_t*           plr = &ddPlayers[playerNum];

    if(playerNum < 0 || playerNum >= DDMAXPLAYERS)
    {
        // Invalid.
        return -1;
    }
    if(playerNum == consolePlayer)
    {
        return 0;
    }

    if(!(plr->shared.flags & DDPF_LOCAL))
        return -1; // Not local at all.

    for(i = 0; i < DDMAXPLAYERS; ++i)
    {
        int console = (i + consolePlayer) % DDMAXPLAYERS;
        player_t* plr = &ddPlayers[console];

        if(console == playerNum)
        {
            return count;
        }

        if(plr->shared.flags & DDPF_LOCAL)
            count++;
    }
    return -1;
}

/**
 * Given a ptr to ddplayer_t, return it's logical index.
 */
int P_GetDDPlayerIdx(ddplayer_t* ddpl)
{
    if(ddpl)
    {
        uint                i;

        for(i = 0; i < DDMAXPLAYERS; ++i)
            if(&ddPlayers[i].shared == ddpl)
                return i;
    }

    return -1;
}

/**
 * Do we THINK the given (camera) player is currently in the void.
 * The method used to test this is to compare the position of the mobj
 * each time it is linked into a BSP leaf.
 *
 * @note Cannot be 100% accurate so best not to use it for anything critical...
 *
 * @param player        The player to test.
 *
 * @return              @c true, If the player is thought to be in the void.
 */
boolean P_IsInVoid(player_t* player)
{
    ddplayer_t* ddpl;

    if(!player)
        return false;

    ddpl = &player->shared;

    // Cameras are allowed to move completely freely (so check z height
    // above/below ceiling/floor).
    if(ddpl->flags & DDPF_CAMERA)
    {
        if(ddpl->inVoid)
            return true;

        if(ddpl->mo && ddpl->mo->bspLeaf)
        {
            Sector* sec = ddpl->mo->bspLeaf->sector;

            if(Surface_IsSkyMasked(&sec->SP_ceilsurface))
            {
                const float skyCeil = GameMap_SkyFixCeiling(theMap);
                if(skyCeil < DDMAXFLOAT && ddpl->mo->origin[VZ] > skyCeil - 4)
                    return true;
            }
            else if(ddpl->mo->origin[VZ] > sec->SP_ceilvisheight - 4)
                return true;

            if(Surface_IsSkyMasked(&sec->SP_floorsurface))
            {
                const float skyFloor = GameMap_SkyFixFloor(theMap);
                if(skyFloor > DDMINFLOAT && ddpl->mo->origin[VZ] < skyFloor + 4)
                    return true;
            }
            else if(ddpl->mo->origin[VZ] < sec->SP_floorvisheight + 4)
                return true;
        }
    }

    return false;
}

short P_LookDirToShort(float lookDir)
{
    int dir = (int) (lookDir/110.f * DDMAXSHORT);

    if(dir < DDMINSHORT) return DDMINSHORT;
    if(dir > DDMAXSHORT) return DDMAXSHORT;
    return (short) dir;
}

float P_ShortToLookDir(short s)
{
    return s / (float)DDMAXSHORT * 110.f;
}
