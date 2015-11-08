/** @file cl_sound.cpp  Clientside sounds.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "client/cl_sound.h"

#include "api_client.h"
#include "client/cl_player.h"

#include "audio/sound.h"

#include "network/net_msg.h"

#include "world/map.h"
#include "world/p_players.h"
#include "Sector"

#include "clientapp.h"

using namespace de;

void Cl_ReadSoundDelta(deltatype_t type)
{
    LOG_AS("Cl_ReadSoundDelta");
    duint16 const deltaId = Reader_ReadUInt16(::msgReader);
    byte const flags      = Reader_ReadByte(::msgReader);

    /// @todo Do not assume the CURRENT map.
    Map &map = ClientApp::worldSystem().map();

    thid_t mobjId  = 0;
    mobj_t *cmo    = nullptr;
    Sector *sector = nullptr;
    Polyobj *poly  = nullptr;
    LineSide *side = nullptr;
    SoundEmitter *emitter = nullptr;

    ::audio::SoundParams sound;

    bool skip = false;
    if(type == DT_SOUND)
    {
        // Delta ID is the effect ID.
        sound.effectId = deltaId;
    }
    else if(type == DT_MOBJ_SOUND)  // Mobj as emitter
    {
        mobjId = deltaId;
        if((cmo = ClMobj_Find(mobjId)) != nullptr)
        {
            ClientMobjThinkerData::RemoteSync *info = ClMobj_GetInfo(cmo);
            if(info->flags & CLMF_HIDDEN)
            {
                // We can't play sounds from hidden mobjs, because we
                // aren't sure exactly where they are located.
                cmo = nullptr;
                LOG_NET_VERBOSE("Can't find sound emitter ") << mobjId;
            }
            else
            {
                emitter = (SoundEmitter *)cmo;
            }
        }
    }
    else if(type == DT_SECTOR_SOUND) // Plane as emitter
    {        
        dint index = deltaId;
        if(!(sector = map.sectorPtr(index)))
        {
            LOG_NET_WARNING("Received sound delta has invalid sector index %i") << index;
            skip = true;
        }
    }
    else if(type == DT_SIDE_SOUND) // Side section as emitter
    {
        dint index = deltaId;
        if(!(side = map.sidePtr(index)))
        {
            LOG_NET_WARNING("Received sound delta has invalid side index %i") << index;
            skip = true;
        }
    }
    else // DT_POLY_SOUND
    {
        dint index = deltaId;

        LOG_NET_XVERBOSE("DT_POLY_SOUND: poly=%d") << index;

        if(!(emitter = (SoundEmitter *) (poly = map.polyobjPtr(index))))
        {
            LOG_NET_WARNING("Received sound delta has invalid polyobj index %i") << index;
            skip = true;
        }
    }

    if(type != DT_SOUND)
    {
        // The effect ID.
        sound.effectId = Reader_ReadUInt16(::msgReader);
    }

    // Select the emitter for the sound.
    if(type == DT_SECTOR_SOUND)
    {
        if(flags & SNDDF_PLANE_FLOOR)
        {
            emitter = &sector->floor().soundEmitter();
        }
        else if(flags & SNDDF_PLANE_CEILING)
        {
            emitter = &sector->ceiling().soundEmitter();
        }
        else
        {
            // Must be the sector's sound emitter, then.
            emitter = &sector->soundEmitter();
        }
    }
    else if(type == DT_SIDE_SOUND)
    {
        if(flags & SNDDF_SIDE_MIDDLE)
        {
            emitter = &side->middleSoundEmitter();
        }
        else if(flags & SNDDF_SIDE_TOP)
        {
            emitter = &side->topSoundEmitter();
        }
        else if(flags & SNDDF_SIDE_BOTTOM)
        {
            emitter = &side->bottomSoundEmitter();
        }
    }

    if(flags & SNDDF_VOLUME)
    {
        byte vol = Reader_ReadByte(::msgReader);
        if(vol == 255)
        {
            // Full volume, no attenuation.
            sound.flags |= ::audio::SoundFlag::NoVolumeAttenuation;
        }
        else
        {
            sound.volume = vol / 127.0f;
        }
    }

    if(flags & SNDDF_REPEAT)
    {
        sound.flags |= ::audio::SoundFlag::Repeat;
    }

    // The delta has been read. Are we skipping?
    if(skip) return;

    // Now the entire delta has been read.
    // Should we start or stop a sound?
    if(sound.volume > 0 && sound.effectId > 0)
    {
        // Do we need to queue this sound?
        if(type == DT_MOBJ_SOUND && !cmo)
        {
            // Create a new Hidden clmobj.
            cmo = map.clMobjFor(mobjId, true/*create*/);

            ClientMobjThinkerData::RemoteSync *info = ClMobj_GetInfo(cmo);
            info->flags |= CLMF_HIDDEN | CLMF_SOUND;
            info->sound  = sound.effectId;
            info->volume = sound.volume;

            // The sound will be started when the clmobj is unhidden.
            return;
        }

        // We will start a sound.
        if(type != DT_SOUND && !emitter)
        {
            // Not enough information.
            LOG_NET_VERBOSE("Cl_ReadSoundDelta2(%i): Insufficient data, snd=%i") << type << sound.effectId;
            return;
        }

        // Sounds originating from the viewmobj should actually originate from the real
        // player mobj.
        if(cmo && cmo->thinker.id == ClPlayer_State(consolePlayer)->clMobjId)
        {
            emitter = (SoundEmitter *)DD_Player(consolePlayer)->publicData().mo;
        }

        ClientApp::audioSystem().worldStage().playSound(sound, emitter);
    }
    else if(sound.effectId >= 0)
    {
        // We must stop a sound. We'll only stop sounds from
        // specific sources.
        if(emitter)
        {
            ClientApp::audioSystem().stopSound(::audio::World, sound.effectId, emitter);
        }
    }
}

void Cl_Sound()
{
    LOG_AS("Cl_Sound");
    byte const dFlags = Reader_ReadByte(::msgReader);

    ::audio::SoundParams sound;
    sound.effectId = ((dFlags & SNDF_SHORT_SOUND_ID) ? Reader_ReadUInt16(::msgReader) : Reader_ReadByte(::msgReader));

    // Is the ID valid?
    if(sound.effectId < 1 || sound.effectId >= ::defs.sounds.size())
    {
        LOGDEV_NET_WARNING("Unknown Sound #%i") << sound.effectId;
        return;
    }

    if(dFlags & SNDF_VOLUME)
    {
        /// @todo Why do we not use the whole value range? -ds
        dbyte vol = Reader_ReadByte(::msgReader);
        if(vol > 127)
        {
            // Full volume, no attenuation.
            sound.flags |= ::audio::SoundFlag::NoVolumeAttenuation;
        }
        else
        {
            sound.volume = vol / 127.0f;
        }
    }

    // Locate the emitter and/or configure the origin of the sound.
    SoundEmitter *emitter = nullptr;
    if(dFlags & SNDF_ID)
    {
        emitter = (SoundEmitter *)ClMobj_Find(Reader_ReadUInt16(::msgReader));
        if(!emitter) return;  // Huh?
    }
    else if(dFlags & SNDF_SECTOR)
    {
        /// @todo Do not assume the CURRENT map.
        emitter = &ClientApp::worldSystem().map().sector(dint( Reader_ReadPackedUInt16(::msgReader) )).soundEmitter();
        if(!emitter) return;  // Huh?
    }
    else if(dFlags & SNDF_ORIGIN)
    {
        sound.flags &= ~::audio::SoundFlag::NoOrigin;
        sound.origin = Vector3d(  Reader_ReadInt16(::msgReader)
                                , Reader_ReadInt16(::msgReader)
                                , Reader_ReadInt16(::msgReader));
    }
    else if(dFlags & SNDF_PLAYER)
    {
        dint const playerNum = (dFlags & 0xf0) >> 4;
        DENG2_ASSERT(playerNum >= 0 && playerNum < DDMAXPLAYERS);
        emitter = (SoundEmitter *)DD_Player(playerNum)->publicData().mo;
    }
    else
    {
        // Play it from "somewhere".
        LOGDEV_NET_VERBOSE("Unknown emitter for Sound #%i") << sound.effectId;
    }

    ClientApp::audioSystem().worldStage().playSound(sound, emitter);
}
