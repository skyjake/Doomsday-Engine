/** @file cl_sound.cpp  Clientside sounds.
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

#include "de_base.h"
#include "client/cl_sound.h"

#include "de_console.h"
#include "de_network.h"
#include "de_play.h"
#include "de_audio.h"

using namespace de;

void Cl_ReadSoundDelta(deltatype_t type)
{
    LOG_AS("Cl_ReadSoundDelta");

    /// @todo Do not assume the CURRENT map.
    Map &map = App_WorldSystem().map();

    int sound = 0, soundFlags = 0;
    mobj_t *cmo = 0;
    thid_t mobjId = 0;
    Sector *sector = 0;
    Polyobj *poly = 0;
    LineSide *side = 0;
    mobj_t *emitter = 0;

    uint16_t const deltaId = Reader_ReadUInt16(msgReader);
    byte const flags       = Reader_ReadByte(msgReader);

    bool skip = false;
    if(type == DT_SOUND)
    {
        // Delta ID is the sound ID.
        sound = deltaId;
    }
    else if(type == DT_MOBJ_SOUND) // Mobj as emitter
    {
        mobjId = deltaId;
        if((cmo = ClMobj_Find(mobjId)) != NULL)
        {
            ClientMobjThinkerData::NetworkState* info = ClMobj_GetInfo(cmo);
            if(info->flags & CLMF_HIDDEN)
            {
                // We can't play sounds from hidden mobjs, because we
                // aren't sure exactly where they are located.
                cmo = NULL;
                LOG_NET_VERBOSE("Can't find sound emitter ") << mobjId;
            }
            else
            {
                emitter = cmo;
            }
        }
    }
    else if(type == DT_SECTOR_SOUND) // Plane as emitter
    {        
        int index = deltaId;

        if(index >= 0 && index < map.sectorCount())
        {
            sector = map.sectors().at(index);
        }
        else
        {
            LOG_NET_WARNING("Received sound delta has invalid sector index %i") << index;
            skip = true;
        }
    }
    else if(type == DT_SIDE_SOUND) // Side section as emitter
    {
        int index = deltaId;

        side = map.sideByIndex(index);
        if(!side)
        {
            LOG_NET_WARNING("Received sound delta has invalid side index %i") << index;
            skip = true;
        }
    }
    else // DT_POLY_SOUND
    {
        int index = deltaId;

        LOG_NET_XVERBOSE("DT_POLY_SOUND: poly=%d") << index;

        if(index >= 0 && index < map.polyobjCount())
        {
            poly = map.polyobjs().at(index);
            emitter = (mobj_t *) poly;
        }
        else
        {
            LOG_NET_WARNING("Received sound delta has invalid polyobj index %i") << index;
            skip = true;
        }
    }

    if(type != DT_SOUND)
    {
        // The sound ID.
        sound = Reader_ReadUInt16(msgReader);
    }

    if(type == DT_SECTOR_SOUND)
    {
        // Select the emitter for the sound.
        if(flags & SNDDF_PLANE_FLOOR)
        {
            emitter = (mobj_t *) &sector->floor().soundEmitter();
        }
        else if(flags & SNDDF_PLANE_CEILING)
        {
            emitter = (mobj_t *) &sector->ceiling().soundEmitter();
        }
        else
        {
            // Must be the sector's sound emitter, then.
            emitter = (mobj_t *) &sector->soundEmitter();
        }
    }

    if(type == DT_SIDE_SOUND)
    {
        if(flags & SNDDF_SIDE_MIDDLE)
            emitter = (mobj_t *) &side->middleSoundEmitter();
        else if(flags & SNDDF_SIDE_TOP)
            emitter = (mobj_t *) &side->topSoundEmitter();
        else if(flags & SNDDF_SIDE_BOTTOM)
            emitter = (mobj_t *) &side->bottomSoundEmitter();
    }

    float volume = 1;
    if(flags & SNDDF_VOLUME)
    {
        byte b = Reader_ReadByte(msgReader);

        if(b == 255)
        {
            // Full volume, no attenuation.
            soundFlags |= DDSF_NO_ATTENUATION;
        }
        else
        {
            volume = b / 127.0f;
        }
    }

    if(flags & SNDDF_REPEAT)
    {
        soundFlags |= DDSF_REPEAT;
    }

    // The delta has been read. Are we skipping?
    if(skip)
        return;

    // Now the entire delta has been read.
    // Should we start or stop a sound?
    if(volume > 0 && sound > 0)
    {
        // Do we need to queue this sound?
        if(type == DT_MOBJ_SOUND && !cmo)
        {
            ClientMobjThinkerData::NetworkState *info = 0;

            // Create a new Hidden clmobj.
            cmo = map.clMobjFor(mobjId, true/*create*/);
            info = ClMobj_GetInfo(cmo);
            info->flags |= CLMF_HIDDEN | CLMF_SOUND;
            info->sound = sound;
            info->volume = volume;
            // The sound will be started when the clmobj is unhidden.
            return;
        }

        // We will start a sound.
        if(type != DT_SOUND && !emitter)
        {
            // Not enough information.
            LOG_NET_VERBOSE("Cl_ReadSoundDelta2(%i): Insufficient data, snd=%i") << type << sound;
            return;
        }

        // Sounds originating from the viewmobj should really originate
        // from the real player mobj.
        if(cmo && cmo->thinker.id == ClPlayer_State(consolePlayer)->clMobjId)
        {
            emitter = ddPlayers[consolePlayer].shared.mo;
        }

        // First stop any sounds originating from the same emitter.
        // We allow only one sound per emitter.
        if(type != DT_SOUND && emitter)
        {
            S_StopSound(0, emitter);
        }

        S_LocalSoundAtVolume(sound | soundFlags, emitter, volume);
    }
    else if(sound >= 0)
    {
        // We must stop a sound. We'll only stop sounds from
        // specific sources.
        if(emitter)
        {
            S_StopSound(sound, emitter);
        }
    }
}

void Cl_Sound()
{
    LOG_AS("Cl_Sound");

    /// @todo Do not assume the CURRENT map.
    Map &map = App_WorldSystem().map();

    byte const flags = Reader_ReadByte(msgReader);

    // Sound ID.
    int sound;
    if(flags & SNDF_SHORT_SOUND_ID)
    {
        sound = Reader_ReadUInt16(msgReader);
    }
    else
    {
        sound = Reader_ReadByte(msgReader);
    }

    // Is the ID valid?
    if(sound < 1 || sound >= defs.sounds.size())
    {
        LOGDEV_NET_WARNING("Invalid sound ID %i") << sound;
        return;
    }

    LOGDEV_NET_XVERBOSE("id %i") << sound;

    int volume = 127;
    if(flags & SNDF_VOLUME)
    {
        volume = Reader_ReadByte(msgReader);
        if(volume > 127)
        {
            volume = 127;
            sound |= DDSF_NO_ATTENUATION;
        }
    }

    if(flags & SNDF_ID)
    {
        thid_t  sourceId = Reader_ReadUInt16(msgReader);
        mobj_t *cmo = ClMobj_Find(sourceId);
        if(cmo)
        {
            S_LocalSoundAtVolume(sound, cmo, volume / 127.0f);
        }
    }
    else if(flags & SNDF_SECTOR)
    {
        int num = int(Reader_ReadPackedUInt16(msgReader));
        if(num >= map.sectorCount())
        {
            LOG_NET_WARNING("Invalid sector number %i") << num;
            return;
        }
        mobj_t *mo = (mobj_t *) &map.sectors().at(num)->soundEmitter();
        //S_StopSound(0, mo);
        S_LocalSoundAtVolume(sound, mo, volume / 127.0f);
    }
    else if(flags & SNDF_ORIGIN)
    {
        coord_t pos[3];
        pos[VX] = Reader_ReadInt16(msgReader);
        pos[VY] = Reader_ReadInt16(msgReader);
        pos[VZ] = Reader_ReadInt16(msgReader);
        S_LocalSoundAtVolumeFrom(sound, NULL, pos, volume / 127.0f);
    }
    else if(flags & SNDF_PLAYER)
    {
        S_LocalSoundAtVolume(sound, ddPlayers[(flags & 0xf0) >> 4].shared.mo,
                             volume / 127.0f);
    }
    else
    {
        // Play it from "somewhere".
        LOGDEV_NET_VERBOSE("Unspecified origin for sound %i") << sound;

        S_LocalSoundAtVolume(sound, NULL, volume / 127.0f);
    }
}
