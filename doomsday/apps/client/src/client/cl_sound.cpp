/** @file cl_sound.cpp  Clientside sounds.
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
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#include "de_base.h"
#include "api_client.h"
#include "api_sound.h"
#include "client/cl_sound.h"
#include "client/cl_player.h"
#include "network/net_msg.h"
#include "world/map.h"
#include "world/p_players.h"

#include <doomsday/world/sector.h>
#include <de/logbuffer.h>

using namespace de;

using world::Sector;
using world::World;

void Cl_ReadSoundDelta(deltatype_t type)
{
    LOG_AS("Cl_ReadSoundDelta");

    /// @todo Do not assume the CURRENT map.
    Map &map = World::get().map().as<Map>();

    dint sound = 0, soundFlags = 0;
    mobj_t *cmo = 0;
    thid_t mobjId = 0;
    Sector *sector = 0;
    Polyobj *poly = 0;
    world::LineSide *side = 0;
    mobj_t *emitter = 0;

    const duint16 deltaId = Reader_ReadUInt16(::msgReader);
    const byte flags      = Reader_ReadByte(::msgReader);

    bool skip = false;
    if (type == DT_SOUND)
    {
        // Delta ID is the sound ID.
        sound = deltaId;
    }
    else if (type == DT_MOBJ_SOUND) // Mobj as emitter
    {
        mobjId = deltaId;
        if ((cmo = ClMobj_Find(mobjId)) != nullptr)
        {
            ClientMobjThinkerData::RemoteSync *info = ClMobj_GetInfo(cmo);
            if (info->flags & CLMF_HIDDEN)
            {
                // We can't play sounds from hidden mobjs, because we
                // aren't sure exactly where they are located.
                cmo = nullptr;
                LOG_NET_VERBOSE("Can't find sound emitter ") << mobjId;
            }
            else
            {
                emitter = cmo;
            }
        }
    }
    else if (type == DT_SECTOR_SOUND) // Plane as emitter
    {
        dint index = deltaId;
        if (!(sector = map.sectorPtr(index)))
        {
            LOG_NET_WARNING("Received sound delta has invalid sector index %i") << index;
            skip = true;
        }
    }
    else if (type == DT_SIDE_SOUND) // Side section as emitter
    {
        dint index = deltaId;
        if (!(side = map.sidePtr(index)))
        {
            LOG_NET_WARNING("Received sound delta has invalid side index %i") << index;
            skip = true;
        }
    }
    else // DT_POLY_SOUND
    {
        dint index = deltaId;

        LOG_NET_XVERBOSE("DT_POLY_SOUND: poly=%d", index);

        if (!(emitter = (mobj_t *) (poly = map.polyobjPtr(index))))
        {
            LOG_NET_WARNING("Received sound delta has invalid polyobj index %i") << index;
            skip = true;
        }
    }

    if (type != DT_SOUND)
    {
        // The sound ID.
        sound = Reader_ReadUInt16(::msgReader);
    }

    if (type == DT_SECTOR_SOUND && !skip)
    {
        // Select the emitter for the sound.
        if (flags & SNDDF_PLANE_FLOOR)
        {
            emitter = (mobj_t *) &sector->floor().soundEmitter();
        }
        else if (flags & SNDDF_PLANE_CEILING)
        {
            emitter = (mobj_t *) &sector->ceiling().soundEmitter();
        }
        else
        {
            // Must be the sector's sound emitter, then.
            emitter = (mobj_t *) &sector->soundEmitter();
        }
    }

    if (type == DT_SIDE_SOUND && !skip)
    {
        if (flags & SNDDF_SIDE_MIDDLE)
            emitter = (mobj_t *) &side->middleSoundEmitter();
        else if (flags & SNDDF_SIDE_TOP)
            emitter = (mobj_t *) &side->topSoundEmitter();
        else if (flags & SNDDF_SIDE_BOTTOM)
            emitter = (mobj_t *) &side->bottomSoundEmitter();
    }

    dfloat volume = 1;
    if (flags & SNDDF_VOLUME)
    {
        byte b = Reader_ReadByte(::msgReader);

        if (b == 255)
        {
            // Full volume, no attenuation.
            soundFlags |= DDSF_NO_ATTENUATION;
        }
        else
        {
            volume = b / 127.0f;
        }
    }

    if (flags & SNDDF_REPEAT)
    {
        soundFlags |= DDSF_REPEAT;
    }

    // The delta has been read. Are we skipping?
    if (skip) return;

    // Now the entire delta has been read.
    // Should we start or stop a sound?
    if (volume > 0 && sound > 0)
    {
        // Do we need to queue this sound?
        if (type == DT_MOBJ_SOUND && !cmo)
        {
            // Create a new Hidden clmobj.
            cmo = map.clMobjFor(mobjId, true/*create*/);

            ClientMobjThinkerData::RemoteSync *info = ClMobj_GetInfo(cmo);
            info->flags |= CLMF_HIDDEN | CLMF_SOUND;
            info->sound  = sound;
            info->volume = volume;

            // The sound will be started when the clmobj is unhidden.
            return;
        }

        // We will start a sound.
        if (type != DT_SOUND && !emitter)
        {
            // Not enough information.
            LOG_NET_VERBOSE("Cl_ReadSoundDelta2(%i): Insufficient data, snd=%i") << type << sound;
            return;
        }

        // Sounds originating from the viewmobj should really originate
        // from the real player mobj.
        if (cmo && cmo->thinker.id == ClPlayer_State(consolePlayer)->clMobjId)
        {
            emitter = DD_Player(consolePlayer)->publicData().mo;
        }

        // First stop any sounds originating from the same emitter.
        // We allow only one sound per emitter.
        if (type != DT_SOUND && emitter)
        {
            S_StopSound(0, emitter);
        }

        S_LocalSoundAtVolume(sound | soundFlags, emitter, volume);
    }
    else if (sound >= 0)
    {
        // We must stop a sound. We'll only stop sounds from
        // specific sources.
        if (emitter)
        {
            S_StopSound(sound, emitter);
        }
    }
}

void Cl_Sound()
{
    LOG_AS("Cl_Sound");

    /// @todo Do not assume the CURRENT map.
    world::Map &map = App_World().map();

    const byte flags = Reader_ReadByte(::msgReader);

    // Sound ID.
    dint sound;
    if (flags & SNDF_SHORT_SOUND_ID)
    {
        sound = Reader_ReadUInt16(::msgReader);
    }
    else
    {
        sound = Reader_ReadByte(::msgReader);
    }

    // Is the ID valid?
    if (sound < 1 || sound >= DED_Definitions()->sounds.size())
    {
        LOGDEV_NET_WARNING("Invalid sound ID %i") << sound;
        return;
    }

    LOGDEV_NET_XVERBOSE("id %i", sound);

    dint volume = 127;
    if (flags & SNDF_VOLUME)
    {
        volume = Reader_ReadByte(::msgReader);
        if (volume > 127)
        {
            volume = 127;
            sound |= DDSF_NO_ATTENUATION;
        }
    }

    if (flags & SNDF_ID)
    {
        thid_t sourceId = Reader_ReadUInt16(::msgReader);
        if (mobj_t *cmob = ClMobj_Find(sourceId))
        {
            S_LocalSoundAtVolume(sound, cmob, volume / 127.0f);
        }
    }
    else if (flags & SNDF_SECTOR)
    {
        auto num = dint( Reader_ReadPackedUInt16(::msgReader) );
        if (num >= map.sectorCount())
        {
            LOG_NET_WARNING("Invalid sector number %i") << num;
            return;
        }
        auto *mob = (mobj_t *) &map.sector(num).soundEmitter();
        //S_StopSound(0, mob);
        S_LocalSoundAtVolume(sound, mob, volume / 127.0f);
    }
    else if (flags & SNDF_ORIGIN)
    {
        coord_t pos[3];
        pos[0] = Reader_ReadInt16(::msgReader);
        pos[1] = Reader_ReadInt16(::msgReader);
        pos[2] = Reader_ReadInt16(::msgReader);
        S_LocalSoundAtVolumeFrom(sound, nullptr, pos, volume / 127.0f);
    }
    else if (flags & SNDF_PLAYER)
    {
        const dint player = (flags & 0xf0) >> 4;
        DE_ASSERT(player >= 0 && player < DDMAXPLAYERS);
        S_LocalSoundAtVolume(sound, DD_Player(player)->publicData().mo, volume / 127.0f);
    }
    else
    {
        // Play it from "somewhere".
        LOGDEV_NET_VERBOSE("Unspecified origin for sound %i") << sound;

        S_LocalSoundAtVolume(sound, nullptr, volume / 127.0f);
    }
}
