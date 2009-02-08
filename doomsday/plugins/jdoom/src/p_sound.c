/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2009 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 1993-1996 by id Software, Inc.
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
 * p_sound.c:
 */

// HEADER FILES ------------------------------------------------------------

#include "jdoom.h"

#include "dmu_lib.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

/**
 * Starts playing the music for the current map.
 */
void S_MapMusic(void)
{
    int                 songid;

    if(G_GetGameState() != GS_MAP)
        return;

    songid = Get(DD_MAP_MUSIC);
    if(S_StartMusicNum(songid, true))
    {
        // Set the game status cvar for the map music.
        gsvMapMusic = songid;
    }
}

/**
 * Doom-like sector sounds: when a new sound starts, stop any old ones
 * from the same origin.
 *
 * @param sec           Sector in which the sound should be played.
 * @param origin        Origin of the sound (center/floor/ceiling).
 * @param id            ID number of the sound to be played.
 */
void S_SectorSound(sector_t* sec, sectorsoundorigin_t origin, int id)
{
    mobj_t*             centerOrigin, *floorOrigin, *ceilingOrigin;

    centerOrigin = (mobj_t *) P_GetPtrp(sec, DMU_SOUND_ORIGIN);
    floorOrigin = (mobj_t *) P_GetPtrp(sec, DMU_FLOOR_SOUND_ORIGIN);
    ceilingOrigin = (mobj_t *) P_GetPtrp(sec, DMU_CEILING_SOUND_ORIGIN);

    S_StopSound(0, centerOrigin);
    S_StopSound(0, floorOrigin);
    S_StopSound(0, ceilingOrigin);

    switch(origin)
    {
    case SORG_FLOOR:
        S_StartSound(id, floorOrigin);
        break;

    case SORG_CEILING:
        S_StartSound(id, ceilingOrigin);
        break;

    case SORG_CENTER:
    default:
        S_StartSound(id, centerOrigin);
        break;
    }
}
