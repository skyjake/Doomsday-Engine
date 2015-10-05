/** @file fmod_music.cpp  Music playback interface.
 *
 * @authors Copyright © 2011-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2015 Daniel Swanson <danij@dengine.net>
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

#include <de/String>
#include <cstring>

using namespace de;

struct SongBuffer
{
    int size;
    char *data;

    SongBuffer(int newSize) : size(newSize) {
        data = new char[newSize];
    }

    ~SongBuffer() {
        delete [] data;
    }
};

static float musicVolume;
static std::string soundFontFileName;

static FMOD::Sound *song;
static SongBuffer *songBuffer;
static bool needReleaseSong;

static FMOD::Channel *music;

static FMOD_RESULT F_CALLBACK
musicCallback(FMOD_CHANNEL *chanPtr, FMOD_CHANNEL_CALLBACKTYPE type,
    void * /*commanddata1*/, void * /*commanddata2*/)
{
    if(reinterpret_cast<FMOD::Channel *>(chanPtr) != ::music)
        return FMOD_OK;  // Safety check.

    switch(type)
    {
    case FMOD_CHANNEL_CALLBACKTYPE_END:
        // The music has stopped.
        ::music = nullptr;
        break;

    default: break;
    }

    return FMOD_OK;
}

static void releaseSong()
{
    if(::song)
    {
        if(::needReleaseSong)
        {
            DSFMOD_TRACE("releaseSong: Song " << ::song << " will be released.");
            ::song->release();
        }
        else
        {
            DSFMOD_TRACE("releaseSong: Song " << ::song << " will NOT be released.");
        }
        ::song = nullptr;
        ::needReleaseSong = false;
    }

    ::music = nullptr;
}

static void releaseSongBuffer()
{
    delete ::songBuffer;
    ::songBuffer = nullptr;
}

static bool startSong()
{
    if(!::fmodSystem) return false;

    if(!::song) return false;

    if(::music)
    {
        ::music->stop();
    }

    // Start playing the song.
    FMOD_RESULT res = ::fmodSystem->playSound(FMOD_CHANNEL_FREE, ::song, true, &::music);
    DSFMOD_ERRCHECK(res);

    // Properties.
    ::music->setVolume(::musicVolume);
    ::music->setCallback(::musicCallback);

    // Start playing.
    ::music->setPaused(false);

    return true;
}

/// @internal
void DMFmod_Music_SetSoundFont(char const *fileName)
{
    if(fileName && fileName[0])
    {
        ::soundFontFileName = fileName;
    }
    else
    {
        ::soundFontFileName.clear();
    }
}

/// @internal
bool DMFmod_Music_PlaySound(FMOD::Sound *customSound, bool needRelease)
{
    releaseSong();
    releaseSongBuffer();

    // Use this as the song.
    ::needReleaseSong = needRelease;
    ::song = customSound;
    return startSong();
}

void setDefaultStreamBufferSize()
{
    if(!::fmodSystem) return;

    FMOD_RESULT res = fmodSystem->setStreamBufferSize(16*1024, FMOD_TIMEUNIT_RAWBYTES);
    DSFMOD_ERRCHECK(res);
}

int DM_Music_Init()
{
    ::music           = nullptr;
    ::song            = nullptr;
    ::needReleaseSong = false;
    ::musicVolume     = 1.f;
    ::songBuffer      = nullptr;
    ::soundFontFileName.clear(); // empty for the default
    return ::fmodSystem != nullptr;
}

void DMFmod_Music_Shutdown()
{
    if(!::fmodSystem) return;

    releaseSongBuffer();
    releaseSong();

    ::soundFontFileName.clear();

    // Will be shut down with the rest of FMOD.
    DSFMOD_TRACE("Music_Shutdown.");
}

void DM_Music_Shutdown()
{
    DMFmod_Music_Shutdown();
}

void DMFmod_Music_Set(int prop, float value)
{
    if(!::fmodSystem) return;

    switch(prop)
    {
    case MUSIP_VOLUME:
        ::musicVolume = value;
        if(::music) ::music->setVolume(::musicVolume);
        DSFMOD_TRACE("Music_Set: MUSIP_VOLUME = " << ::musicVolume);
        break;

    default: break;
    }
}

void DM_Music_Set(int prop, float value)
{
    DMFmod_Music_Set(prop, value);
}

int DMFmod_Music_Get(int prop, void *ptr)
{
    switch(prop)
    {
    case MUSIP_IDENTITYKEY:
        if(ptr)
        {
            qstrcpy((char *) ptr, "music");
            return true;
        }
        break;

    case MUSIP_PLAYING:
        if(!::fmodSystem) return false;
        return ::music != nullptr;  /// @c nullptr when not playing.

    default: break;
    }

    return false;
}

int DM_Music_Get(int prop, void *ptr)
{
    return DMFmod_Music_Get(prop, ptr);
}

void DM_Music_Update()
{
    // No need to do anything. The callback handles restarting.
}

void DMFmod_Music_Stop()
{
    if(!::fmodSystem || !::music) return;

    DSFMOD_TRACE("Music_Stop");
    ::music->stop();
}

void DM_Music_Stop()
{
    DMFmod_Music_Stop();
}

int DM_Music_Play(int looped)
{
    if(!::fmodSystem) return false;

    if(::songBuffer)
    {
        // Get rid of the old song.
        releaseSong();

        setDefaultStreamBufferSize();

        FMOD_CREATESOUNDEXINFO extra; zeroStruct(extra);
        extra.length = ::songBuffer->size;
        if(!String::fromStdString(::soundFontFileName).fileNameExtension().compareWithoutCase(".dls"))
        {
            extra.dlsname = soundFontFileName.c_str();
        }

        // Load a new song.
        FMOD_RESULT res = ::fmodSystem->createSound(::songBuffer->data,
                                                    FMOD_CREATESTREAM | FMOD_OPENMEMORY | (looped ? FMOD_LOOP_NORMAL : 0),
                                                    &extra, &song);
        DSFMOD_TRACE("Music_Play: songBuffer has " << ::songBuffer->size << " bytes, created Sound " << ::song);
        DSFMOD_ERRCHECK(res);

        ::needReleaseSong = true;

        // The song buffer remains in memory, in case FMOD needs to stream from it.
    }

    return startSong();
}

void DMFmod_Music_Pause(int setPause)
{
    if(!::fmodSystem) return;
    
    if(!::music) return;
    ::music->setPaused(setPause != 0);
}

void DM_Music_Pause(int setPause)
{
    DMFmod_Music_Pause(setPause);
}

void *DM_Music_SongBuffer(unsigned int length)
{
    if(!::fmodSystem) return nullptr;

    releaseSongBuffer();

    DSFMOD_TRACE("Music_SongBuffer: Allocating a song buffer for " << length << " bytes.");

    // The caller will put data in this buffer. Before playing, we will create the
    // FMOD sound based on the data in the song buffer.
    ::songBuffer = new SongBuffer(length);
    return ::songBuffer->data;
}

int DM_Music_PlayFile(char const *filename, int looped)
{
    if(!::fmodSystem) return false;

    // Get rid of the current song.
    releaseSong();
    releaseSongBuffer();

    setDefaultStreamBufferSize();

    FMOD_CREATESOUNDEXINFO extra; zeroStruct(extra);
    if(!String::fromStdString(::soundFontFileName).fileNameExtension().compareWithoutCase(".dls"))
    {
        extra.dlsname = ::soundFontFileName.c_str();
    }

    FMOD_RESULT res = ::fmodSystem->createSound(filename, FMOD_CREATESTREAM | (looped ? FMOD_LOOP_NORMAL : 0),
                                                &extra, &::song);
    DSFMOD_TRACE("Music_Play: loaded '" << filename << "' => Sound " << ::song);
    DSFMOD_ERRCHECK(res);

    ::needReleaseSong = true;

    return startSong();
}
