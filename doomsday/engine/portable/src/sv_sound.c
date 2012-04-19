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
 * sv_sound.c: Server-side Sound Management
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_console.h"
#include "de_network.h"
#include "de_refresh.h"
#include "de_play.h"
#include "de_audio.h"

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
 * Tell clients to play a sound with full volume.
 */
void Sv_Sound(int sound_id, mobj_t *origin, int toPlr)
{
    Sv_SoundAtVolume(sound_id, origin, 1, toPlr);
}

/**
 * Finds the sector/polyobj to whom the base mobj belong.
 */
static void Sv_IdentifySoundBase(mobj_t** base, Sector** sector, Polyobj** poly)
{
    *sector = NULL;
    *poly = NULL;

    if(*base && !(*base)->thinker.id)
    {
        // No mobj ID => it's not a real mobj.

        *poly = GameMap_PolyobjByBase(theMap, *base);
        if(!*poly)
        {
            // It wasn't a polyobj.
            // Try the sectors instead.
            *sector = GameMap_SectorByBase(theMap, *base);
        }

#ifdef _DEBUG
        if(!*poly && !*sector)
        {
            Con_Error("Sv_IdentifySoundBase: Bad mobj.\n");
        }
#endif
        *base = NULL;
    }
}

/**
 * Tell clients to play a sound.
 */
void Sv_SoundAtVolume(int soundIDAndFlags, mobj_t* origin, float volume,
                      int toPlr)
{
    int                 soundID = (soundIDAndFlags & ~DDSF_FLAG_MASK);
    Sector             *sector;
    Polyobj            *poly;
    int                 targetPlayers = 0;

    if(isClient || !soundID)
        return;

    Sv_IdentifySoundBase(&origin, &sector, &poly);

    if(toPlr & SVSF_TO_ALL)
    {
        targetPlayers = -1;
    }
    else
    {
        targetPlayers = (1 << (toPlr & 0xf));
    }

    if(toPlr & SVSF_EXCLUDE_ORIGIN)
    {
        // Remove the bit of the player who owns the origin mobj (if any).
        if(origin && origin->dPlayer)
        {
            targetPlayers &= ~(1 << P_GetDDPlayerIdx(origin->dPlayer));
        }
    }

#ifdef _DEBUG
    VERBOSE2( Con_Message("Sv_SoundAtVolume: Id=%i, vol=%g, targets=%x\n",
                          soundID, volume, targetPlayers) );
#endif

    Sv_NewSoundDelta(soundID, origin, sector, poly, volume,
                     (soundIDAndFlags & DDSF_REPEAT) != 0,
                     targetPlayers);
}

/**
 * This is called when the server needs to tell clients to stop
 * a sound.
 */
void Sv_StopSound(int sound_id, mobj_t *origin)
{
    Sector     *sector;
    Polyobj    *poly;

    if(isClient)
        return;

    Sv_IdentifySoundBase(&origin, &sector, &poly);

    /*#ifdef _DEBUG
       Con_Printf("Sv_StopSound: id=%i origin=%i(%p) sec=%i poly=%i\n",
       sound_id, origin? origin->thinker.id : 0, origin,
       sector, poly);
       #endif */

    // Send the stop sound delta to everybody.
    // Volume zero means silence.
    Sv_NewSoundDelta(sound_id, origin, sector, poly, 0, false, -1);
}
