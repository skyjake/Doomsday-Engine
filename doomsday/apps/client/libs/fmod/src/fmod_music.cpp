/**
 * @file fmod_music.cpp
 * Music playback interface. @ingroup dsfmod
 *
 * @authors Copyright © 2011-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "driver_fmod.h"
#include <string.h>
#include <string>
#include <de/logbuffer.h>

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

static FMOD_SOUND *   song;
static FMOD_CHANNEL * music;
static bool           needReleaseSong;
static float          musicVolume;
static SongBuffer *   songBuffer;
static std::string    soundFontFileName;

static FMOD_RESULT F_CALLBACK
musicCallback(FMOD_CHANNELCONTROL *channelcontrol,
              FMOD_CHANNELCONTROL_TYPE controltype,
              FMOD_CHANNELCONTROL_CALLBACK_TYPE callbacktype,
              void * /*commanddata1*/, void * /*commanddata2*/)
{
    if (controltype != FMOD_CHANNELCONTROL_CHANNEL)
        return FMOD_OK;

    if (reinterpret_cast<FMOD_CHANNEL *>(channelcontrol) != music)
        return FMOD_OK; // Safety check.

    switch (callbacktype)
    {
    case FMOD_CHANNELCONTROL_CALLBACK_END:
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
    if (song)
    {
        if (needReleaseSong)
        {
            DSFMOD_TRACE("releaseSong: Song %p will be released." << song);
            FMOD_Sound_Release(song);
        }
        else
        {
            DSFMOD_TRACE("releaseSong: Song %p will NOT be released." << song);
        }
        song = 0;
        needReleaseSong = false;
    }
    music = 0;
}

static void releaseSongBuffer()
{
    if (songBuffer)
    {
        delete songBuffer;
        songBuffer = 0;
    }
}

static void setDefaultStreamBufferSize()
{
    if (!fmodSystem) return;

    FMOD_RESULT result;
    result = FMOD_System_SetStreamBufferSize(fmodSystem,
                                             16 * 1024,
                                             FMOD_TIMEUNIT_RAWBYTES);
    DSFMOD_ERRCHECK(result);
}

int fmod_DM_Music_Init(void)
{
    music = 0;
    song = 0;
    needReleaseSong = false;
    musicVolume = 1.f;
    songBuffer = 0;
    soundFontFileName.clear(); // empty for the default
    return fmodSystem != 0;
}

void fmod_Music_Shutdown(void)
{
    if (!fmodSystem) return;

    releaseSongBuffer();
    releaseSong();

    soundFontFileName.clear();

    // Will be shut down with the rest of FMOD.
    DSFMOD_TRACE("Music_Shutdown.");
}

void fmod_DM_Music_Shutdown(void)
{
    fmod_Music_Shutdown();
}

void fmod_Music_SetSoundFont(const char *fileName)
{
    if (fileName && fileName[0])
    {
        soundFontFileName = fileName;
    }
    else
    {
        soundFontFileName.clear();
    }
}

void fmod_Music_Set(int prop, float value)
{
    if (!fmodSystem)
        return;

    switch (prop)
    {
    case MUSIP_VOLUME:
        musicVolume = value;
        if (music) FMOD_Channel_SetVolume(music, musicVolume);
        DSFMOD_TRACE("Music_Set: MUSIP_VOLUME = " << musicVolume);
        break;

    default:
        break;
    }
}

void fmod_DM_Music_Set(int prop, float value)
{
    fmod_Music_Set(prop, value);
}

int fmod_Music_Get(int prop, void* ptr)
{
    switch (prop)
    {
    case MUSIP_ID:
        if (ptr)
        {
            strcpy((char*) ptr, "FMOD/Ext");
            return true;
        }
        break;

    case MUSIP_PLAYING:
        if (!fmodSystem) return false;
        return music != 0; // NULL when not playing.

    default:
        break;
    }

    return false;
}

int fmod_DM_Music_Get(int prop, void* ptr)
{
    return fmod_Music_Get(prop, ptr);
}

void fmod_DM_Music_Update(void)
{
    // No need to do anything. The callback handles restarting.
}

void fmod_Music_Stop(void)
{
    if (!fmodSystem || !music) return;

    DSFMOD_TRACE("Music_Stop.");

    FMOD_Channel_Stop(music);
}

void fmod_DM_Music_Stop(void)
{
    fmod_Music_Stop();
}

static bool startSong()
{
    if (!fmodSystem || !song) return false;

    if (music) FMOD_Channel_Stop(music);

    // Start playing the song.
    FMOD_RESULT result;
    result = FMOD_System_PlaySound(fmodSystem, song, nullptr, true, &music);
    DSFMOD_ERRCHECK(result);

    // Properties.
    FMOD_Channel_SetVolume(music, musicVolume);
    FMOD_Channel_SetCallback(music, musicCallback);

    // Start playing.
    FMOD_Channel_SetPaused(music, false);
    return true;
}

bool fmod_Music_PlaySound(FMOD_SOUND* customSound, bool needRelease)
{
    releaseSong();
    releaseSongBuffer();

    // Use this as the song.
    needReleaseSong = needRelease;
    song = customSound;
    return startSong();
}

int fmod_DM_Music_Play(int looped)
{
    if (!fmodSystem) return false;

    if (songBuffer)
    {
        // Get rid of the old song.
        releaseSong();

        setDefaultStreamBufferSize();

        FMOD_CREATESOUNDEXINFO extra;
        zeroStruct(extra);
        extra.length = songBuffer->size;
        if (endsWith(soundFontFileName.c_str(), ".dls"))
        {
            extra.dlsname = soundFontFileName.c_str();
        }

        // Load a new song.
        FMOD_RESULT result;
        result = FMOD_System_CreateSound(fmodSystem,
                                         songBuffer->data,
                                         FMOD_CREATESTREAM | FMOD_OPENMEMORY |
                                         (looped? FMOD_LOOP_NORMAL : 0),
                                         &extra, &song);
        DSFMOD_TRACE("Music_Play: songBuffer has %d bytes, created Sound %p" << songBuffer->size << song);
        DSFMOD_ERRCHECK(result);

        needReleaseSong = true;

        // The song buffer remains in memory, in case FMOD needs to stream from it.
    }
    return startSong();
}

void fmod_Music_Pause(int setPause)
{
    if (!fmodSystem || !music) return;

    FMOD_Channel_SetPaused(music, setPause != 0);
}

void fmod_DM_Music_Pause(int setPause)
{
    fmod_Music_Pause(setPause);
}

void* fmod_DM_Music_SongBuffer(unsigned int length)
{
    if (!fmodSystem) return NULL;

    releaseSongBuffer();

    DSFMOD_TRACE("Music_SongBuffer: Allocating a song buffer for " << length << " bytes.");

    // The caller will put data in this buffer. Before playing, we will create the
    // FMOD sound based on the data in the song buffer.
    songBuffer = new SongBuffer(length);
    return songBuffer->data;
}

int fmod_DM_Music_PlayFile(const char *filename, int looped)
{
    if (!fmodSystem) return false;

    // Get rid of the current song.
    releaseSong();
    releaseSongBuffer();

    setDefaultStreamBufferSize();

    FMOD_CREATESOUNDEXINFO extra;
    zeroStruct(extra);
    if (endsWith(soundFontFileName.c_str(), ".dls"))
    {
        extra.dlsname = soundFontFileName.c_str();
    }

    FMOD_RESULT result;
    result = FMOD_System_CreateSound(fmodSystem,
                                     filename,
                                     FMOD_CREATESTREAM | (looped? FMOD_LOOP_NORMAL : 0),
                                     &extra, &song);
    DSFMOD_TRACE("Music_Play: loaded '%s'' => Sound %p" << filename << song);
    DSFMOD_ERRCHECK(result);

    needReleaseSong = true;

    return startSong();
}
