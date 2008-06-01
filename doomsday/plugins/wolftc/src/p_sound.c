/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2008 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2008 Daniel Swanson <danij@dengine.net>
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

// HEADER FILES ------------------------------------------------------------

#include "wolftc.h"

#include "dmu_lib.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern int gsvMapMusic;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

int S_GetMusicNum(int episode, int map)
{
    int     mnum;

    if(gamemode == commercial)
        mnum = MUS_MAP01 + map - 1;
    else
    {
        int     spmus[] = {
            // Song - Who? - Where?
            MUS_E3M4,           // American     e4m1
            MUS_E3M2,           // Romero       e4m2
            MUS_E3M3,           // Shawn        e4m3
            MUS_E1M5,           // American     e4m4
            MUS_E2M7,           // Tim          e4m5
            MUS_E2M4,           // Romero       e4m6
            MUS_E2M6,           // J.Anderson   e4m7 CHIRON.WAD
            MUS_E2M5,           // Shawn        e4m8
            MUS_E1M9            // Tim          e4m9
        };

        if(episode < 4)
            mnum = MUS_E1M1 + (episode - 1) * 9 + map - 1;
        else
            mnum = spmus[map - 1];
    }
    return mnum;
}

/**
 * Starts playing the music for this level
 */
void S_LevelMusic(void)
{
    int         songid;

    if(G_GetGameState() != GS_LEVEL)
        return;

    // Start new music for the level.
    if(Get(DD_MAP_MUSIC) == -1)
    {
        songid = S_GetMusicNum(gameepisode, gamemap);
        S_StartMusicNum(songid, true);
    }
    else
    {
        songid = Get(DD_MAP_MUSIC);
        S_StartMusicNum(songid, true);
    }

    // set the game status cvar for the map music
    gsvMapMusic = songid;
}

/**
 * Doom-like sector sounds: when a new sound starts, stop any old ones
 * from the same origin.
 *
 * @param sec           Sector in which the sound should be played.
 * @param origin        Origin of the sound (center/floor/ceiling).
 * @param id            ID number of the sound to be played.
 */
void S_SectorSound(sector_t *sec, sectorsoundorigin_t origin, int id)
{
    mobj_t             *centerOrigin, *floorOrigin, *ceilingOrigin;

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
