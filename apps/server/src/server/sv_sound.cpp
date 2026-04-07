/** @file sv_sound.cpp  Serverside Sound Management.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2015 Daniel Swanson <danij@dengine.net>
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
#include "network/net_main.h"
#include "server/sv_sound.h"
#include "server/sv_pool.h"
#include "world/p_players.h"

#include <de/logbuffer.h>

using namespace de;

static inline dd_bool isRealMobj(const mobj_t *base)
{
    return base && base->thinker.id != 0;
}

/**
 * Find the map object to whom @a base belongs.
 */
static void identifySoundEmitter(const mobj_t **base, world::Sector **sector, Polyobj **poly,
                                 world::Plane **plane, world::Surface **surface)
{
    *sector  = nullptr;
    *poly    = nullptr;
    *plane   = nullptr;
    *surface = nullptr;

    if (!*base || isRealMobj(*base)) return;

    /// @todo fixme: Do not assume the current map.
    App_World().map().identifySoundEmitter(*reinterpret_cast<const SoundEmitter *>(*base),
                                           sector, poly, plane, surface);

#ifdef DE_DEBUG
    if (!*sector && !*poly && !*plane && !*surface)
    {
        throw Error("Sv_IdentifySoundBase", "Bad sound base");
    }
#endif

    *base = nullptr;
}

void Sv_Sound(dint soundId, const mobj_t *origin, dint toPlr)
{
    Sv_SoundAtVolume(soundId, origin, 1, toPlr);
}

void Sv_SoundAtVolume(dint soundIDAndFlags, const mobj_t *origin, dfloat volume, dint toPlr)
{
    if (netState.isClient) return;

    dint soundID = (soundIDAndFlags & ~DDSF_FLAG_MASK);
    if (!soundID) return;

    world::Sector *sector;
    Polyobj *poly;
    world::Plane *plane;
    world::Surface *surface;
    identifySoundEmitter(&origin, &sector, &poly, &plane, &surface);

    dint targetPlayers = 0;
    if (toPlr & SVSF_TO_ALL)
    {
        targetPlayers = -1;
    }
    else
    {
        targetPlayers = (1 << (toPlr & 0xf));
    }

    if (toPlr & SVSF_EXCLUDE_ORIGIN)
    {
        // Remove the bit of the player who owns the origin mobj (if any).
        if (origin && origin->dPlayer)
        {
            targetPlayers &= ~(1 << P_GetDDPlayerIdx(origin->dPlayer));
        }
    }

    LOGDEV_NET_XVERBOSE("Sv_SoundAtVolume: id: #%i volume: %f targets: %x",
                        soundID << volume << targetPlayers);

    Sv_NewSoundDelta(soundID, origin, sector, poly, plane, surface, volume,
                     (soundIDAndFlags & DDSF_REPEAT) != 0, targetPlayers);
}

void Sv_StopSound(dint soundId, const mobj_t *origin)
{
    if (netState.isClient) return;

    world::Sector *sector;
    world::Plane *plane;
    world::Surface *surface;
    Polyobj *poly;
    identifySoundEmitter(&origin, &sector, &poly, &plane, &surface);

    LOGDEV_NET_XVERBOSE("Sv_StopSound: id: #%i origin: %i(%p) sec: %p poly: %p plane: %p surface: %p",
                        soundId << (origin? origin->thinker.id : 0)
                        << origin << sector << poly << plane << surface);

    Sv_NewSoundDelta(soundId, origin, sector, poly, plane, surface, 0/*silence*/,
                     false/*non-repeating*/, -1/*all clients*/);
}
