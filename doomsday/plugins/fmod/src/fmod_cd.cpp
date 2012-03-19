/**
 * @file fmod_cd.cpp
 * CD audio playback. @ingroup dsfmod
 *
 * @authors Copyright © 2011-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "driver_fmod.h"

static int numDrives;
static FMOD::Sound* cdSound;

int DM_CDAudio_Init(void)
{
    numDrives = 0;
    FMOD_RESULT result;
    result = fmodSystem->getNumCDROMDrives(&numDrives);
    DSFMOD_ERRCHECK(result);
    DSFMOD_TRACE("CDAudio_Init: " << numDrives << " CD drives available.");

    return fmodSystem != 0;
}

void DM_CDAudio_Shutdown(void)
{
    // Will be shut down with the rest of FMOD.
    DSFMOD_TRACE("CDAudio_Shutdown.");
}

void DM_CDAudio_Update(void)
{
    // No need to update anything.
}

void DM_CDAudio_Set(int prop, float value)
{
    if(!fmodSystem) return;

    switch(prop)
    {
    case MUSIP_VOLUME:
        DM_Music_Set(MUSIP_VOLUME, value);
        break;

    default:
        break;
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
        return DM_Music_Get(MUSIP_PLAYING, ptr);

    default:
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

    FMOD::Sound* trackSound = 0;
    bool needRelease = false;

#if MACOSX
    for(char* ptr = driveName; *ptr; ptr++)
        if(*ptr == '/') *ptr = ':';

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
    }

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

    return DM_Music_PlaySound(trackSound, needRelease); // takes ownership
}

void DM_CDAudio_Pause(int pause)
{
    DM_Music_Pause(pause);
}

void DM_CDAudio_Stop(void)
{
    DM_Music_Stop();

    if(cdSound)
    {
        cdSound->release();
        cdSound = 0;
    }
}
