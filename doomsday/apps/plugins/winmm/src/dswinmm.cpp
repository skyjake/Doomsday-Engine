/** @file dswinmm.cpp  Windows Multimedia, Doomsday audio driver plugin.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2008-2015 Daniel Swanson <danij@dengine.net>
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

#include "dswinmm.h"

#include "cdaudio.h"
#include "midistream.h"
#include "mixer.h"
#include <de/App>

using namespace de;

/**
 * Macro for conveniently accessing the de::App singleton instance.
 */
#define DENG2_APP               (&de::App::app())

static bool inited;
static dint origCdVol    = -1;  ///< Original volume. @c -1= unknown.
static dint origSynthVol = -1;  ///< Original volume. @c -1= unknown.

static std::unique_ptr<Mixer> mixer;
static std::unique_ptr<CdAudio> cdaudio;
static std::unique_ptr<MidiStreamer> midiStreamer;

int DS_Init()
{
    LOG_AS("[WinMM]");
    //LOG_WIP("DS_Init");

    // Already been here?
    if(inited) return true;

    ::origCdVol    = -1;
    ::origSynthVol = -1;

    // Initialize the line mixer?
    if(!DENG2_APP->commandLine().has("-nomixer"))
    {
        LOG_AUDIO_VERBOSE("Number of mixer devices: %i")
            << Mixer::deviceCount();

        ::mixer.reset(new Mixer);
        if(::mixer->isReady())
        {
            // Get the original mixer volume settings (restored at shutdown).
            if(::mixer->cdLine().isReady())
            {
                ::origCdVol = ::mixer->cdLine().volume();
            }

            if(::mixer->synthLine().isReady())
            {
                ::origSynthVol = ::mixer->synthLine().volume();
            }
        }
    }

    ::inited = true;
    return ::inited;
}

void DS_Shutdown()
{
    LOG_AS("[WinMM]");
    //LOG_WIP("DS_Shutdown");

    // Already been here?
    if(!::inited) return;

    // In case the engine hasn't already done so, close open interfaces.
    DM_CDAudio_Shutdown();
    DM_Music_Shutdown();

    // Restore original line out volumes?
    if(bool( ::mixer ))
    {
        if(::origCdVol >= 0)
        {
            ::mixer->cdLine().setVolume(::origCdVol);
        }
        if(::origSynthVol >= 0)
        {
            ::mixer->synthLine().setVolume(::origSynthVol);
        }
    }
    // We're done with the mixer.
    ::mixer.reset();

    ::inited = false;
}

/**
 * The Event function is called to tell the driver about certain critical
 * events like the beginning and end of an update cycle.
 */
void DS_Event(int /*type*/)
{
    // Do nothing...
}

int DS_Get(int prop, void *ptr)
{
    switch(prop)
    {
    case AUDIOP_IDENTITYKEY: {
        auto *idKey = reinterpret_cast<AutoStr *>(ptr);
        DENG2_ASSERT(idKey);
        if(idKey) Str_Set(idKey, "winmm");
        return true; }

    case AUDIOP_TITLE: {
        auto *title = reinterpret_cast<AutoStr *>(ptr);
        DENG2_ASSERT(title);
        if(title) Str_Set(title, "Windows Multimedia");
        return true; }

    default: DENG2_ASSERT("[WinMM]DS_Get: Unknown property"); break;
    }
    return false;
}

int DM_CDAudio_Init()
{
    LOG_AS("[WinMM]");
    //LOG_WIP("DM_CDAudio_Init");

    if(!cdaudio)
    {
        cdaudio.reset(new CdAudio);
    }
    return bool( cdaudio );
}

void DM_CDAudio_Shutdown()
{
    LOG_AS("[WinMM]");
    //LOG_WIP("DM_CDAudio_Shutdown");

    ::cdaudio.reset();
}

void DM_CDAudio_Set(int prop, float value)
{
    LOG_AS("[WinMM]");
    //LOG_WIP("DM_CDAudio_Set(prop:%i value:%f)") << prop << value;

    if(!::cdaudio) return;

    switch(prop)
    {
    case MUSIP_VOLUME:
        if(bool( ::mixer ))
        {
            if(::mixer->cdLine().isReady())
            {
                mixer->cdLine().setVolume(value);
            }
        }
        break;

    default: break;
    }
}

int DM_CDAudio_Get(int prop, void *ptr)
{
    LOG_AS("[WinMM]");
    //LOG_WIP("DM_CDAudio_Get(prop:%i ptr:%p)") << prop << dintptr(ptr);

    switch(prop)
    {
    case MUSIP_IDENTITYKEY:
        if(ptr)
        {
            qstrcpy((char *) ptr, "cd");
            return true;
        }
        break;

    case MUSIP_PLAYING:
        if(::cdaudio)
        {
            return ::cdaudio->isPlaying();
        }
        break;

    case MUSIP_PAUSED:
        if(::cdaudio)
        {
            *(reinterpret_cast<dint *>(ptr)) = ::cdaudio->isPaused();
            return true;
        }
        break;

    default: break;
    }

    return 0;
}

void DM_CDAudio_Update()
{
    LOG_AS("[WinMM]");
    //LOG_WIP("DM_CDAudio_Update");

    if(!::cdaudio) return;
    ::cdaudio->update();
}

int DM_CDAudio_Play(int newTrack, int looped)
{
    LOG_AS("[WinMM]");
    //LOG_WIP("DM_CDAudio_Play(newTrack:%i looped:%b)") << newTrack << CPP_BOOL(looped);

    if(!::cdaudio) return 0;
    return ::cdaudio->play(newTrack, CPP_BOOL(looped));
}

void DM_CDAudio_Pause(int doPause)
{
    LOG_AS("[WinMM]");
    //LOG_WIP("DM_CDAudio_Pause(doPause:%b)") << CPP_BOOL(doPause);

    if(!::cdaudio) return;

    if(doPause) ::cdaudio->pause();
    else        ::cdaudio->resume();
}

void DM_CDAudio_Stop()
{
    LOG_AS("[WinMM]");
    //LOG_WIP("DM_CDAudio_Stop");

    if(!::cdaudio) return;
    ::cdaudio->stop();
}

/**
 * Returns @c true if successful.
 */
int DM_Music_Init()
{
    LOG_AS("[WinMM]");
    //LOG_WIP("DM_Music_Init");

    // Already been here?
    if(::midiStreamer) return true;

    LOG_AUDIO_NOTE("Number of MIDI-out devices: %i")
        << MidiStreamer::deviceCount();

    ::midiStreamer.reset(new MidiStreamer);
    try
    {
        // Try to open the output stream.
        ::midiStreamer->openStream();

        // Double output volume?
        ::midiStreamer->setVolumeShift(DENG2_APP->commandLine().has("-mdvol") ? 1 : 0);

        // Now the MIDI is available.
        LOG_AUDIO_VERBOSE("MIDI initialized");
        return true;
    }
    catch(MidiStreamer::OpenError const &er)
    {
        LOG_AUDIO_ERROR("") << er.asText();
    }
    return false;
}

void DM_Music_Shutdown()
{
    LOG_AS("[WinMM]");
    //LOG_WIP("DM_Music_Shutdown");

    ::midiStreamer.reset();
}

void DM_Music_Set(int prop, float value)
{
    LOG_AS("[WinMM]");
    //LOG_WIP("DM_Music_Set(prop:%i value:%f)") << prop << value;

    if(!::midiStreamer) return;

    switch(prop)
    {
    case MUSIP_VOLUME: 
        if(bool( ::mixer ))
        {
            if(::mixer->synthLine().isReady())
            {
                ::mixer->synthLine().setVolume(value);
            }
        }
        break;

    default: break;
    }
}

int DM_Music_Get(int prop, void *ptr)
{
    LOG_AS("[WinMM]");
    //LOG_WIP("DM_Music_Get(prop:%i ptr:%p)") << prop << dintptr(ptr);

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
        if(::midiStreamer)
        {
            return ::midiStreamer->isPlaying();
        }
        break;

    case MUSIP_PAUSED:
        if(::midiStreamer)
        {
            *(reinterpret_cast<dint *>(ptr)) = ::midiStreamer->isPaused();
            return true;
        }
        break;

    default: break;
    }

    return false;
}

void DM_Music_Update()
{
    //LOG_AS("[WinMM]");
    //LOG_WIP("DM_Music_Update");
    // No need to do anything. The callback handles restarting.
}

void DM_Music_Stop()
{
    LOG_AS("[WinMM]");
    //LOG_WIP("DM_Music_Stop");
    if(!::midiStreamer) return;

    ::midiStreamer->stop();
}

int DM_Music_Play(int looped)
{
    LOG_AS("[WinMM]");
    //LOG_WIP("DM_Music_Play(looped:%b)") << CPP_BOOL(looped);
    if(!::midiStreamer) return false;

    ::midiStreamer->play(looped);
    return true;
}

void DM_Music_Pause(int setPause)
{
    LOG_AS("[WinMM]");
    //LOG_WIP("DM_Music_Pause(setPause:%b)") << CPP_BOOL(setPause);
    if(!::midiStreamer) return;

    if(setPause) ::midiStreamer->pause();
    else         ::midiStreamer->resume();
}

void *DM_Music_SongBuffer(unsigned int length)
{
    LOG_AS("[WinMM]");
    //LOG_WIP("DM_Music_SongBuffer(length:%u)") << length;
    if(!::midiStreamer) return nullptr;

    return ::midiStreamer->songBuffer(length);
}

/**
 * Declares the type of the plugin so the engine knows how to treat it. Called
 * automatically when the plugin is loaded.
 */
DENG_EXTERN_C char const *deng_LibraryType()
{
    return "deng-plugin/audio";
}

DENG_DECLARE_API(Con);

DENG_API_EXCHANGE(
    DENG_GET_API(DE_API_CONSOLE, Con);
)
