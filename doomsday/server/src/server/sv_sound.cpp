/** @file sv_sound.cpp Serverside Sound Management.
 * @ingroup server
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
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#include "de_base.h"
#include "de_console.h"
#include "de_network.h"
#include "de_play.h"
#include "de_audio.h"

#include <de/Log>

static inline boolean isRealMobj(const mobj_t* base)
{
    return base && base->thinker.id != 0;
}

/**
 * Find the map object to whom @a base belongs.
 */
static void Sv_IdentifySoundBase(mobj_t **base, Sector **sector, Polyobj **poly,
                                 Surface **surface)
{
    *sector  = 0;
    *poly    = 0;
    *surface = 0;

    if(!*base || isRealMobj(*base)) return;

    /// @todo Optimize: All sound emitters in a sector are linked together forming
    /// a chain. Make use of the chains instead.

    // No mobj ID => it's not a real mobj.
    *poly = theMap->polyobjByBase(*reinterpret_cast<ddmobj_base_t *>(base));
    if(!*poly)
    {
        // Not a polyobj. Try the sectors next.
        *sector = theMap->sectorBySoundEmitter(*reinterpret_cast<ddmobj_base_t *>(base));
        if(!*sector)
        {
            // Not a sector. Try the surfaces.
            *surface = theMap->surfaceBySoundEmitter(*reinterpret_cast<ddmobj_base_t *>(base));
        }
    }

#ifdef _DEBUG
    if(!*sector && !*poly && !*surface)
    {
        Con_Error("Sv_IdentifySoundBase: Bad sound base.\n");
    }
#endif

    *base = 0;
}

void Sv_Sound(int soundId, mobj_t* origin, int toPlr)
{
    Sv_SoundAtVolume(soundId, origin, 1, toPlr);
}

void Sv_SoundAtVolume(int soundIDAndFlags, mobj_t* origin, float volume, int toPlr)
{
    if(isClient) return;

    int soundID = (soundIDAndFlags & ~DDSF_FLAG_MASK);
    if(!soundID) return;

    Sector* sector;
    Polyobj* poly;
    Surface* surface;
    Sv_IdentifySoundBase(&origin, &sector, &poly, &surface);

    int targetPlayers = 0;
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

    LOG_TRACE("Sv_SoundAtVolume: id: #%i volume: %f targets: %x")
            << soundID << volume << targetPlayers;

    Sv_NewSoundDelta(soundID, origin, sector, poly, surface, volume,
                     (soundIDAndFlags & DDSF_REPEAT) != 0, targetPlayers);
}

void Sv_StopSound(int soundId, mobj_t* origin)
{
    if(isClient) return;

    Sector* sector;
    Polyobj* poly;
    Surface* surface;
    Sv_IdentifySoundBase(&origin, &sector, &poly, &surface);

    LOG_TRACE("Sv_StopSound: id: #%i origin: %i(%p) sec: %p poly: %p surface: %p")
            << soundId << (origin? origin->thinker.id : 0)
            << origin << sector << poly << surface;

    Sv_NewSoundDelta(soundId, origin, sector, poly, surface, 0/*silence*/,
                     false/*non-repeating*/, -1/*all clients*/);
}
