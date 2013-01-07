/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
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
 * cl_sound.c: Clientside Sounds
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_console.h"
#include "de_network.h"
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
 * Read a sound delta from the message buffer and play it.
 * Only used with PSV_FRAME2 packets.
 */
void Cl_ReadSoundDelta2(deltatype_t type, boolean skip)
{
    int                 sound = 0, soundFlags = 0;
    byte                flags = 0;
    mobj_t             *cmo = NULL;
    thid_t              mobjId = 0;
    Sector             *sector = NULL;
    Polyobj            *poly = NULL;
    mobj_t             *emitter = NULL;
    float               volume = 1;

    if(type == DT_SOUND)
    {
        // Delta ID is the sound ID.
        sound = Reader_ReadUInt16(msgReader);
    }
    else if(type == DT_MOBJ_SOUND)
    {
        if((cmo = ClMobj_Find(mobjId = Reader_ReadUInt16(msgReader))) != NULL)
        {
            clmoinfo_t* info = ClMobj_GetInfo(cmo);
            if(info->flags & CLMF_HIDDEN)
            {
                // We can't play sounds from hidden mobjs, because we
                // aren't sure exactly where they are located.
                cmo = NULL;
            }
            else
            {
                emitter = cmo;
            }
        }
    }
    else if(type == DT_SECTOR_SOUND)
    {
        uint index = Reader_ReadUInt16(msgReader);

        if(index < NUM_SECTORS)
        {
            sector = SECTOR_PTR(index);
        }
        else
        {
            Con_Message("Cl_ReadSoundDelta2: DT_SECTOR_SOUND contains "
                        "invalid sector num %u. Skipping.\n", index);
            skip = true;
        }
    }
    else                        /* DT_POLY_SOUND */
    {
        uint index = Reader_ReadUInt16(msgReader);

        if(index < NUM_POLYOBJS)
        {
            poly = polyObjs[index];
            emitter = (mobj_t *) poly;
        }
        else
        {
            Con_Message("Cl_ReadSoundDelta2: DT_POLY_SOUND contains "
                        "invalid polyobj num %u. Skipping.\n", index);
            skip = true;
        }
    }

    flags = Reader_ReadByte(msgReader);

    if(type != DT_SOUND)
    {
        // The sound ID.
        sound = Reader_ReadUInt16(msgReader);
    }

    if(type == DT_SECTOR_SOUND)
    {
        // Should we use a specific origin?
        if(flags & SNDDF_PLANE_FLOOR)
            emitter = (mobj_t*) &sector->SP_floorsurface.base;
        else if(flags & SNDDF_PLANE_CEILING)
            emitter = (mobj_t*) &sector->SP_ceilsurface.base;
        else
            emitter = (mobj_t*) &sector->base;
    }

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
            clmoinfo_t* info = 0;

            // Create a new Hidden clmobj.
            cmo = ClMobj_Create(mobjId);
            info = ClMobj_GetInfo(cmo);
            info->flags |= CLMF_HIDDEN | CLMF_SOUND;
            info->sound = sound;
            info->volume = volume;
            /*#ifdef _DEBUG
               Con_Printf("Cl_ReadSoundDelta2(%i): Queueing: id=%i snd=%i vol=%.2f\n",
               type, mobjId, sound, volume);
               #endif */
            // The sound will be started when the clmobj is unhidden.
            return;
        }

        // We will start a sound.
        if(type != DT_SOUND && !emitter)
        {
            // Not enough information.
#ifdef _DEBUG
            Con_Printf("Cl_ReadSoundDelta2(%i): Insufficient data, snd=%i\n", type, sound);
#endif
            return;
        }

        // Sounds originating from the viewmobj should really originate
        // from the real player mobj.
        if(cmo && cmo->thinker.id == ClPlayer_State(consolePlayer)->clMobjId)
        {
            /*#ifdef _DEBUG
               Con_Printf("Cl_ReadSoundDelta2(%i): ViewMobj sound...\n", type);
               #endif */
            emitter = ddPlayers[consolePlayer].shared.mo;
        }

        // First stop any sounds originating from the same emitter.
        // We allow only one sound per emitter.
        if(type != DT_SOUND && emitter)
        {
            S_StopSound(0, emitter);
        }

        S_LocalSoundAtVolume(sound | soundFlags, emitter, volume);
/*#
ifdef _DEBUG
Con_Printf("Cl_ReadSoundDelta2(%i): Start snd=%i [%x] vol=%.2f",
           type, sound, flags, volume);
if(cmo) Con_Printf(", mo=%i\n", cmo->mo.thinker.id);
else if(sector) Con_Printf(", sector=%i\n", GET_SECTOR_IDX(sector));
else if(poly) Con_Printf(", poly=%i\n", GET_POLYOBJ_IDX(poly));
else Con_Printf("\n");
#endif
*/
    }
    else if(sound >= 0)
    {
        // We must stop a sound. We'll only stop sounds from
        // specific sources.
        if(emitter)
        {
            S_StopSound(sound, emitter);

/*
#ifdef _DEBUG
Con_Printf("Cl_ReadSoundDelta2(%i): Stop sound %i",
           type, sound);
if(cmo)  Con_Printf(", mo=%i\n", cmo->mo.thinker.id);
else if(sector) Con_Printf(", sector=%i\n", GET_SECTOR_IDX(sector));
else if(poly) Con_Printf(", poly=%i\n", GET_POLYOBJ_IDX(poly));
else Con_Printf("\n");
#endif
*/
        }
    }
}

/**
 * Called when a PSV_FRAME sound packet is received.
 */
void Cl_Sound(void)
{
    int sound, volume = 127;
    coord_t pos[3];
    byte flags;
    uint num;
    mobj_t* mo = NULL;

    flags = Reader_ReadByte(msgReader);

    // Sound ID.
    if(flags & SNDF_SHORT_SOUND_ID)
    {
        sound = Reader_ReadUInt16(msgReader);
    }
    else
    {
        sound = Reader_ReadByte(msgReader);
    }

    // Is the ID valid?
    if(sound < 1 || sound >= defs.count.sounds.num)
    {
        Con_Message("Cl_Sound: Out of bounds ID %i.\n", sound);
        return;                 // Bad sound ID!
    }
#ifdef _DEBUG
    Con_Printf("Cl_Sound: %i\n", sound);
#endif

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
        num = Reader_ReadPackedUInt16(msgReader);
        if(num >= NUM_SECTORS)
        {
            Con_Message("Cl_Sound: Invalid sector number %i.\n", num);
            return;
        }
        mo = (mobj_t*) &SECTOR_PTR(num)->base;
        //S_StopSound(0, mo);
        S_LocalSoundAtVolume(sound, mo, volume / 127.0f);
    }
    else if(flags & SNDF_ORIGIN)
    {
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
#ifdef _DEBUG
Con_Printf("Cl_Sound: NULL orig sound %i\n", sound);
#endif
        S_LocalSoundAtVolume(sound, NULL, volume / 127.0f);
    }
}
