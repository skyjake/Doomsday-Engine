/**
 * @file fluidsynth_music.cpp
 * Music playback interface. @ingroup dsfluidsynth
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

#include "driver_fluidsynth.h"
#include <string.h>

struct SongBuffer
{
    int size;
    char* data;

    SongBuffer(int newSize) : size(newSize) {
        data = new char[newSize];
    }

    ~SongBuffer() {
        delete [] data;
    }
};

#if 0
static FMOD::Sound* song;
static FMOD::Channel* music;
static bool needReleaseSong;
static float musicVolume;
#endif
static SongBuffer* songBuffer;
static const char* soundFontFileName;

#if 0
static FMOD_RESULT F_CALLBACK
musicCallback(FMOD_CHANNEL* chanPtr, FMOD_CHANNEL_CALLBACKTYPE type,
              void* /*commanddata1*/, void* /*commanddata2*/)
{
    if(reinterpret_cast<FMOD::Channel*>(chanPtr) != music)
        return FMOD_OK; // Safety check.

    switch(type)
    {
    case FMOD_CHANNEL_CALLBACKTYPE_END:
        // The music has stopped.
        music = 0;
        break;

    default:
        break;
    }
    return FMOD_OK;
}

static void releaseSong()
{
    if(song)
    {
        if(needReleaseSong)
        {
            DSFMOD_TRACE("releaseSong: Song " << song << " will be released.")
            song->release();
        }
        else
        {
            DSFMOD_TRACE("releaseSong: Song " << song << " will NOT be released.");
        }
        song = 0;
        needReleaseSong = false;
    }
    music = 0;
}
#endif

static void releaseSongBuffer()
{
    if(songBuffer)
    {
        delete songBuffer;
        songBuffer = 0;
    }
}

#if 0
void setDefaultStreamBufferSize()
{
    if(!fmodSystem) return;

    FMOD_RESULT result;
    result = fmodSystem->setStreamBufferSize(16*1024, FMOD_TIMEUNIT_RAWBYTES);
    DSFMOD_ERRCHECK(result);
}
#endif

int DM_Music_Init(void)
{
#if 0
    music = 0;
    song = 0;
    needReleaseSong = false;
    musicVolume = 1.f;
#endif
    songBuffer = 0;
    soundFontFileName = 0; // empty for the default
    return true; //fmodSystem != 0;
}

void DM_Music_Shutdown(void)
{
    releaseSongBuffer();
    soundFontFileName = 0;

    DSFLUIDSYNTH_TRACE("Music_Shutdown.");
}

void DM_Music_SetSoundFont(const char* fileName)
{
    soundFontFileName = fileName;
}

void DM_Music_Set(int prop, float value)
{
#if 0
    if(!fmodSystem)
        return;

    switch(prop)
    {
    case MUSIP_VOLUME:
        musicVolume = value;
        if(music) music->setVolume(musicVolume);
        DSFMOD_TRACE("Music_Set: MUSIP_VOLUME = " << musicVolume);
        break;

    default:
        break;
    }
#endif
}

int DM_Music_Get(int prop, void* ptr)
{
    switch(prop)
    {
    case MUSIP_ID:
        if(ptr)
        {
            strcpy((char*) ptr, "FluidSynth/Ext");
            return true;
        }
        break;

    case MUSIP_PLAYING:
        return false;
#if 0
        if(!fmodSystem) return false;
        return music != 0; // NULL when not playing.
#endif

    default:
        break;
    }

    return false;
}

void DM_Music_Update(void)
{
    // No need to do anything. The callback handles restarting.
}

void DM_Music_Stop(void)
{
#if 0
    if(!fmodSystem || !music) return;

    DSFMOD_TRACE("Music_Stop.");

    music->stop();
#endif
}

#if 0
static bool startSong()
{
    if(!fmodSystem || !song) return false;

    if(music) music->stop();

    // Start playing the song.
    FMOD_RESULT result;
    result = fmodSystem->playSound(FMOD_CHANNEL_FREE, song, true, &music);
    DSFMOD_ERRCHECK(result);

    // Properties.
    music->setVolume(musicVolume);
    music->setCallback(musicCallback);

    // Start playing.
    music->setPaused(false);
    return true;
}

/// @internal
bool DM_Music_PlaySound(FMOD::Sound* customSound, bool needRelease)
{
    releaseSong();
    releaseSongBuffer();

    // Use this as the song.
    needReleaseSong = needRelease;
    song = customSound;
    return startSong();
}
#endif

int DM_Music_Play(int looped)
{
#if 0
    if(!fmodSystem) return false;

    if(songBuffer)
    {
        // Get rid of the old song.
        releaseSong();

        setDefaultStreamBufferSize();

        FMOD_CREATESOUNDEXINFO extra;
        zeroStruct(extra);
        extra.length = songBuffer->size;
        if(endsWith(soundFontFileName, ".dls"))
        {
            extra.dlsname = soundFontFileName;
        }

        // Load a new song.
        FMOD_RESULT result;
        result = fmodSystem->createSound(songBuffer->data,
                                         FMOD_CREATESTREAM | FMOD_OPENMEMORY |
                                         (looped? FMOD_LOOP_NORMAL : 0),
                                         &extra, &song);
        DSFMOD_TRACE("Music_Play: songBuffer has " << songBuffer->size << " bytes, created Sound " << song);
        DSFMOD_ERRCHECK(result);

        needReleaseSong = true;

        // The song buffer remains in memory, in case FMOD needs to stream from it.
    }
    return startSong();
#endif
    return 0;
}

void DM_Music_Pause(int setPause)
{
#if 0
    if(!fmodSystem || !music) return;

    music->setPaused(setPause != 0);
#endif
}

void* DM_Music_SongBuffer(unsigned int length)
{
    releaseSongBuffer();

    DSFLUIDSYNTH_TRACE("Music_SongBuffer: Allocating a song buffer for " << length << " bytes.");

    // The caller will put data in this buffer. Before playing, we will create
    // the FluidSynth sound based on the data in the song buffer.

    songBuffer = new SongBuffer(length);
    return songBuffer->data;
}

int DM_Music_PlayFile(const char *filename, int looped)
{
    // Get rid of the current song.
    //releaseSong();
    releaseSongBuffer();

#if 0
    if(!fmodSystem) return false;

    setDefaultStreamBufferSize();

    FMOD_CREATESOUNDEXINFO extra;
    zeroStruct(extra);
    if(endsWith(soundFontFileName, ".dls"))
    {
        extra.dlsname = soundFontFileName;
    }

    FMOD_RESULT result;
    result = fmodSystem->createSound(filename, FMOD_CREATESTREAM | (looped? FMOD_LOOP_NORMAL : 0),
                                     &extra, &song);
    DSFMOD_TRACE("Music_Play: loaded '" << filename << "' => Sound " << song);
    DSFMOD_ERRCHECK(result);

    needReleaseSong = true;

    return startSong();
#endif
    return false;
}
