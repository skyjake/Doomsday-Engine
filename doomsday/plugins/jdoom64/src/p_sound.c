/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2012 Daniel Swanson <danij@dengine.net>
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

#include "jdoom64.h"

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
 * Start the song for the specified map.
 */
void S_MapMusic(uint episode, uint map)
{
    Uri* mapUri = G_ComposeMapUri(episode, map);
    ddstring_t* mapPath = Uri_Compose(mapUri);
    ddmapinfo_t mapInfo;
    if(Def_Get(DD_DEF_MAP_INFO, Str_Text(mapPath), &mapInfo))
    {
        if(S_StartMusicNum(mapInfo.music, true))
        {
            // Set the game status cvar for the map music.
            gsvMapMusic = mapInfo.music;
        }
    }
    Str_Delete(mapPath);
    Uri_Delete(mapUri);
}

/**
 * Doom-like sector sounds: when a new sound starts, stop any old ones
 * from the same origin.
 *
 * @param sec           Sector in which the sound should be played.
 * @param origin        Origin of the sound (center/floor/ceiling).
 * @param id            ID number of the sound to be played.
 */
void S_SectorSound(Sector* sec, int origin, int id)
{
    mobj_t* centerOrigin, *floorOrigin, *ceilingOrigin;

    centerOrigin  = (mobj_t*) P_GetPtrp(sec, DMU_BASE);
    floorOrigin   = (mobj_t*) P_GetPtrp(sec, DMU_FLOOR_ORIGIN);
    ceilingOrigin = (mobj_t*) P_GetPtrp(sec, DMU_CEILING_ORIGIN);

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
