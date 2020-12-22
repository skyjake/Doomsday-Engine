/** @file fmod_cd.cpp  CD audio playback.
 *
 * @authors Copyright © 2011-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2014 Daniel Swanson <danij@dengine.net>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html (with exception granted to allow
 * linking against FMOD Ex)
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses
 *
 * <b>Special Exception to GPLv2:</b>
 * Linking the Doomsday Audio Plugin for FMOD Ex (audio_fmod) statically or
 * dynamically with other modules is making a combined work based on
 * audio_fmod. Thus, the terms and conditions of the GNU General Public License
 * cover the whole combination. In addition, <i>as a special exception</i>, the
 * copyright holders of audio_fmod give you permission to combine audio_fmod
 * with free software programs or libraries that are released under the GNU
 * LGPL and with code included in the standard release of "FMOD Ex Programmer's
 * API" under the "FMOD Ex Programmer's API" license (or modified versions of
 * such code, with unchanged license). You may copy and distribute such a
 * system following the terms of the GNU GPL for audio_fmod and the licenses of
 * the other code concerned, provided that you include the source code of that
 * other code when and as the GNU GPL requires distribution of source code.
 * (Note that people who make modified versions of audio_fmod are not obligated
 * to grant this special exception for their modified versions; it is their
 * choice whether to do so. The GNU General Public License gives permission to
 * release a modified version without this exception; this exception also makes
 * it possible to release a modified version which carries forward this
 * exception.) </small>
 */

#if 0

#include "driver_fmod.h"
#include <de/logbuffer.h>

static int numDrives;
static FMOD::Sound *cdSound;

int DM_CDAudio_Init()
{
    numDrives = 0;

    FMOD_RESULT result;
    result = fmodSystem->getNumCDROMDrives(&numDrives);
    DSFMOD_ERRCHECK(result);
    DSFMOD_TRACE("CDAudio_Init: " << numDrives << " CD drives available.");

    return fmodSystem != nullptr;
}

void DMFmod_CDAudio_Shutdown()
{
    // Will be shut down with the rest of FMOD.
    DSFMOD_TRACE("CDAudio_Shutdown.");
}

void DM_CDAudio_Shutdown()
{
    DMFmod_CDAudio_Shutdown();
}

void DM_CDAudio_Update()
{
    // No need to update anything.
}

void DM_CDAudio_Set(int prop, float value)
{
    if(!fmodSystem) return;

    switch(prop)
    {
    case MUSIP_VOLUME:
        fmod_Music_Set(MUSIP_VOLUME, value);
        break;

    default: break;
    }
}

int DM_CDAudio_Get(int prop, void* ptr)
{
    if(!fmodSystem) return false;

    switch(prop)
    {
    case MUSIP_ID:
        if(ptr)
        {
            strcpy((char*) ptr, "FMOD/CD");
            return true;
        }
        break;

    case MUSIP_PLAYING:
        return fmod_Music_Get(MUSIP_PLAYING, ptr);

    default:
        DE_ASSERT_FAIL("CDAudio_Get: Unknown property id");
        return false;
    }

    return true;
}

int DM_CDAudio_Play(int track, int looped)
{
    if(!fmodSystem) return false;
    if(!numDrives)
    {
        DSFMOD_TRACE("CDAudio_Play: No CD drives available.");
        return false;
    }

    // Use a bigger stream buffer for CD audio.
    fmodSystem->setStreamBufferSize(64*1024, FMOD_TIMEUNIT_RAWBYTES);

    // Get the drive name.
    /// @todo Make drive selection configurable.
    FMOD_RESULT result;
    char driveName[80];
    fmodSystem->getCDROMDriveName(0, driveName, sizeof(driveName), 0, 0, 0, 0);
    DSFMOD_TRACE("CDAudio_Play: CD drive name: '" << driveName << "'");

    FMOD::Sound *trackSound = nullptr;
    bool needRelease = false;

#if MACOSX
    for(char* ptr = driveName; *ptr; ptr++)
    {
        if(*ptr == '/') *ptr = ':';
    }

    // The CD tracks are mapped to files.
    char trackPath[200];
    sprintf(trackPath, "/Volumes/%s/%i Audio Track.aiff", driveName, track);
    DSFMOD_TRACE("CDAudio_Play: Opening track: " << trackPath);

    result = fmodSystem->createStream(trackPath, (looped? FMOD_LOOP_NORMAL : 0), 0, &trackSound);
    DSFMOD_TRACE("CDAudio_Play: Track " << track << " => Sound " << trackSound);
    DSFMOD_ERRCHECK(result);

    needRelease = true;
#else
#ifdef WIN32
    /// @todo Check if there is a data track.
    // (Hexen CD) The audio tracks begin at #1 even though there is a data track.
    track--;
#endif

    if(!cdSound)
    {
        // Get info about the CD tracks.
        result = fmodSystem->createStream(driveName, FMOD_OPENONLY, 0, &cdSound);
        DSFMOD_TRACE("CDAudio_Play: Opening CD, cdSound " << cdSound);
        DSFMOD_ERRCHECK(result);
        if(result != FMOD_OK) return false;
    }
    DE_ASSERT(cdSound);

    int numTracks = 0;
    result = cdSound->getNumSubSounds(&numTracks);
    DSFMOD_TRACE("CDAudio_Play: Number of tracks = " << numTracks);
    if(result != FMOD_OK) return false;

    // The subsounds are indexed starting from zero (CD track 1 == subsound 0).
    track--;

    if(track >= numTracks)
    {
        DSFMOD_TRACE("CDAudio_Play: Track " << track << " out of bounds.");
        return false;
    }

    result = cdSound->getSubSound(track, &trackSound);
    DSFMOD_ERRCHECK(result);
    DSFMOD_TRACE("CDAudio_Play: Track " << track + 1 << " got subsound " << trackSound);

    if(looped)
    {
        trackSound->setMode(FMOD_LOOP_NORMAL);
    }
#endif

    return fmod_Music_PlaySound(trackSound, needRelease); // takes ownership
}

void DM_CDAudio_Pause(int pause)
{
    fmod_Music_Pause(pause);
}

void DM_CDAudio_Stop()
{
    fmod_Music_Stop();

    if(cdSound)
    {
        cdSound->release(); cdSound = nullptr;
    }
}

#endif

