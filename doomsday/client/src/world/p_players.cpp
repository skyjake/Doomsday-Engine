/** @file p_players.cpp
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
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
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#define DENG_NO_API_MACROS_PLAYER

#include "de_base.h"
#include "de_play.h"
#include "de_network.h"

#include "world/gamemap.h"

using namespace de;

player_t *viewPlayer;
player_t ddPlayers[DDMAXPLAYERS];
int consolePlayer;
int displayPlayer;

/**
 * Determine which console is used by the given local player. Local players
 * are numbered starting from zero.
 */
int P_LocalToConsole(int localPlayer)
{
    int count = 0;
    for(int i = 0; i < DDMAXPLAYERS; ++i)
    {
        int console = (i + consolePlayer) % DDMAXPLAYERS;
        player_t *plr = &ddPlayers[console];
        ddplayer_t *ddpl = &plr->shared;

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
    int i, count = 0;
    player_t *plr = &ddPlayers[playerNum];

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
        player_t *plr = &ddPlayers[console];

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
int P_GetDDPlayerIdx(ddplayer_t *ddpl)
{
    if(ddpl)
    for(uint i = 0; i < DDMAXPLAYERS; ++i)
    {
        if(&ddPlayers[i].shared == ddpl)
            return i;
    }

    return -1;
}

boolean P_IsInVoid(player_t *player)
{
    if(!player) return false;
    ddplayer_t *ddpl = &player->shared;

    // Cameras are allowed to move completely freely (so check z height
    // above/below ceiling/floor).
    if(ddpl->flags & DDPF_CAMERA)
    {
        if(ddpl->inVoid) return true;

        if(ddpl->mo && ddpl->mo->bspLeaf)
        {
            Sector &sec = ddpl->mo->bspLeaf->sector();

            if(sec.ceilingSurface().hasSkyMaskedMaterial())
            {
                coord_t const skyCeil = theMap->skyFixCeiling();
                if(skyCeil < DDMAXFLOAT && ddpl->mo->origin[VZ] > skyCeil - 4)
                    return true;
            }
            else if(ddpl->mo->origin[VZ] > sec.ceiling().visHeight() - 4)
            {
                return true;
            }

            if(sec.floorSurface().hasSkyMaskedMaterial())
            {
                coord_t const skyFloor = theMap->skyFixFloor();
                if(skyFloor > DDMINFLOAT && ddpl->mo->origin[VZ] < skyFloor + 4)
                    return true;
            }
            else if(ddpl->mo->origin[VZ] < sec.floor().visHeight() + 4)
            {
                return true;
            }
        }
    }

    return false;
}

short P_LookDirToShort(float lookDir)
{
    int dir = int( lookDir/110.f * DDMAXSHORT );

    if(dir < DDMINSHORT) return DDMINSHORT;
    if(dir > DDMAXSHORT) return DDMAXSHORT;
    return (short) dir;
}

float P_ShortToLookDir(short s)
{
    return s / float( DDMAXSHORT ) * 110.f;
}

#undef DD_GetPlayer
DENG_EXTERN_C ddplayer_t *DD_GetPlayer(int number)
{
    return (ddplayer_t *) &ddPlayers[number].shared;
}

// net_main.c
DENG_EXTERN_C char const *Net_GetPlayerName(int player);
DENG_EXTERN_C ident_t Net_GetPlayerID(int player);
DENG_EXTERN_C Smoother *Net_PlayerSmoother(int player);

// p_control.c
DENG_EXTERN_C void P_NewPlayerControl(int id, controltype_t type, char const *name, char const *bindContext);
DENG_EXTERN_C void P_GetControlState(int playerNum, int control, float *pos, float *relativeOffset);
DENG_EXTERN_C int P_GetImpulseControlState(int playerNum, int control);
DENG_EXTERN_C void P_Impulse(int playerNum, int control);

DENG_DECLARE_API(Player) =
{
    { DE_API_PLAYER },
    Net_GetPlayerName,
    Net_GetPlayerID,
    Net_PlayerSmoother,
    DD_GetPlayer,
    P_NewPlayerControl,
    P_GetControlState,
    P_GetImpulseControlState,
    P_Impulse
};
