/** @file audio/drivers/sdlmixer.cpp  SDL_mixer, for SFX, Ext and Mus interfaces.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2007-2015 Daniel Swanson <danij@dengine.net>
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

#ifndef DENG_DISABLE_SDLMIXER

#include "de_base.h"
#include "audio/drivers/sdlmixer.h"

#include <cstdlib>
#include <cstring>
#include <SDL.h>
#include <SDL_mixer.h>

#include <de/timer.h>
#include <de/Log>

#include "api_audiod.h"
#include "api_audiod_sfx.h"
#include "api_audiod_mus.h"

using namespace de;

#define DEFAULT_MIDI_COMMAND    "" //"timidity"

// Base interface:
int DS_SDLMixerInit();
void DS_SDLMixerShutdown();
void DS_SDLMixerEvent(int type);

// Sfx interface:
int DS_SDLMixer_SFX_Init();
sfxbuffer_t *DS_SDLMixer_SFX_CreateBuffer(int flags, int bits, int rate);
void DS_SDLMixer_SFX_DestroyBuffer(sfxbuffer_t *buf);
void DS_SDLMixer_SFX_Load(sfxbuffer_t *buf, struct sfxsample_s *sample);
void DS_SDLMixer_SFX_Reset(sfxbuffer_t *buf);
void DS_SDLMixer_SFX_Play(sfxbuffer_t *buf);
void DS_SDLMixer_SFX_Stop(sfxbuffer_t *buf);
void DS_SDLMixer_SFX_Refresh(sfxbuffer_t *buf);
void DS_SDLMixer_SFX_Set(sfxbuffer_t *buf, int prop, float value);
void DS_SDLMixer_SFX_Setv(sfxbuffer_t *buf, int prop, float *values);
void DS_SDLMixer_SFX_Listener(int prop, float value);
void DS_SDLMixer_SFX_Listenerv(int prop, float *values);

// Music interface:
int DS_SDLMixer_Music_Init();
void DS_SDLMixer_Music_Update();
void DS_SDLMixer_Music_Set(int prop, float value);
int DS_SDLMixer_Music_Get(int prop, void *value);
void DS_SDLMixer_Music_Pause(int pause);
void DS_SDLMixer_Music_Stop();
int DS_SDLMixer_Music_PlayFile(char const *fileName, int looped);

audiodriver_t audiod_sdlmixer = {
    DS_SDLMixerInit,
    DS_SDLMixerShutdown,
    DS_SDLMixerEvent
};

audiointerface_sfx_t audiod_sdlmixer_sfx = { {
    DS_SDLMixer_SFX_Init,
    DS_SDLMixer_SFX_CreateBuffer,
    DS_SDLMixer_SFX_DestroyBuffer,
    DS_SDLMixer_SFX_Load,
    DS_SDLMixer_SFX_Reset,
    DS_SDLMixer_SFX_Play,
    DS_SDLMixer_SFX_Stop,
    DS_SDLMixer_SFX_Refresh,
    DS_SDLMixer_SFX_Set,
    DS_SDLMixer_SFX_Setv,
    DS_SDLMixer_SFX_Listener,
    DS_SDLMixer_SFX_Listenerv
} };

audiointerface_music_t audiod_sdlmixer_music = { {
    DS_SDLMixer_Music_Init,
    nullptr,
    DS_SDLMixer_Music_Update,
    DS_SDLMixer_Music_Set,
    DS_SDLMixer_Music_Get,
    DS_SDLMixer_Music_Pause,
    DS_SDLMixer_Music_Stop },
    nullptr,
    nullptr,
    DS_SDLMixer_Music_PlayFile,
};

static bool sdlInitOk;
static dint numChannels;
static bool *usedChannels;
static Mix_Music *lastMusic;

/**
 * This is the hook we ask SDL_mixer to call when music playback finishes.
 */
#ifdef DENG2_DEBUG
static void musicPlaybackFinished()
{
    LOG_AUDIO_VERBOSE("[SDLMixer] Music playback finished");
}
#endif

static dint getFreeChannel()
{
    for(dint i = 0; i < numChannels; ++i)
    {
        if(!usedChannels[i])
            return i;
    }
    return -1;
}

/**
 * Returns the length of the buffer in milliseconds.
 */
static duint getBufLength(sfxbuffer_t const &buf)
{
    DENG2_ASSERT(buf.sample);
    return 1000 * buf.sample->numSamples / buf.freq;
}

dint DS_SDLMixerInit()
{
    // Already been here?
    if(::sdlInitOk) return true;

    if(SDL_InitSubSystem(SDL_INIT_AUDIO))
    {
        LOG_AUDIO_ERROR("Error initializing SDL audio: %s") << SDL_GetError();
        return false;
    }

    SDL_version compVer; SDL_MIXER_VERSION(&compVer);
    SDL_version const *linkVer = Mix_Linked_Version();
    DENG2_ASSERT(linkVer);

    if(SDL_VERSIONNUM(linkVer->major, linkVer->minor, linkVer->patch) >
       SDL_VERSIONNUM(compVer.major, compVer.minor, compVer.patch))
    {
        LOG_AUDIO_WARNING("Linked version of SDL_mixer (%u.%u.%u) is "
                          "newer than expected (%u.%u.%u)")
            << linkVer->major << linkVer->minor << linkVer->patch
            << compVer.major << compVer.minor << compVer.patch;
    }

    if(Mix_OpenAudio(MIX_DEFAULT_FREQUENCY, MIX_DEFAULT_FORMAT, 2, 1024))
    {
        LOG_AUDIO_ERROR("Failed initializing SDL_mixer: %s") << Mix_GetError();
        return false;
    }

    duint16 format;
    dint freq, channels;
    Mix_QuerySpec(&freq, &format, &channels);

    // Announce capabilites.
    LOG_AUDIO_VERBOSE("SDLMixer configuration:");
    LOG_AUDIO_VERBOSE("  " _E(>) "Output: %s"
                      "\nFormat: %x (%x)"
                      "\nFrequency: %iHz (%iHz)"
                      "\nInitial Channels: %i")
            << (channels > 1? "stereo" : "mono")
            << format << (duint16) AUDIO_S16LSB
            << freq << (dint) MIX_DEFAULT_FREQUENCY
            << MIX_CHANNELS;

    // Prepare to play simultaneous sounds.
    /*numChannels =*/ Mix_AllocateChannels(MIX_CHANNELS);
    usedChannels = nullptr;

    // Everything is OK.
    sdlInitOk = true;
    return true;
}

void DS_SDLMixerShutdown()
{
    // Already been here?
    if(!::sdlInitOk) return;

    M_Free(::usedChannels); ::usedChannels = nullptr;

    if(::lastMusic)
    {
        Mix_HaltMusic();
        Mix_FreeMusic(::lastMusic);
        ::lastMusic = nullptr;
    }

    Mix_CloseAudio();
    SDL_QuitSubSystem(SDL_INIT_AUDIO);

    ::sdlInitOk = false;
}

void DS_SDLMixerEvent(int)
{
    // Not supported.
}

int DS_SDLMixer_SFX_Init()
{
    // No extra init needed.
    return sdlInitOk;
}

sfxbuffer_t *DS_SDLMixer_SFX_CreateBuffer(int flags, int bits, int rate)
{
    auto *buf = (sfxbuffer_t *) Z_Calloc(sizeof(sfxbuffer_t), PU_APPSTATIC, 0);

    buf->bytes = bits / 8;
    buf->rate  = rate;
    buf->flags = flags;
    buf->freq  = rate;  // Modified by calls to Set(SFXBP_FREQUENCY).

    // The cursor is used to keep track of the channel on which the sample
    // is playing.
    buf->cursor = getFreeChannel();
    if((dint)buf->cursor < 0)
    {
        buf->cursor = numChannels++;
        usedChannels = (bool *) M_Realloc(usedChannels, sizeof(usedChannels[0]) * numChannels);

        // Make sure we have enough channels allocated.
        Mix_AllocateChannels(numChannels);

        Mix_UnregisterAllEffects(buf->cursor);
    }

    usedChannels[buf->cursor] = true;

    return buf;
}

void DS_SDLMixer_SFX_DestroyBuffer(sfxbuffer_t *buf)
{
    if(!buf) return;

    Mix_HaltChannel(buf->cursor);
    usedChannels[buf->cursor] = false;
    Z_Free(buf);
}

void DS_SDLMixer_SFX_Load(sfxbuffer_t *buf, sfxsample_t *sample)
{
    DENG2_ASSERT(buf && sample);

    // Does the buffer already have a sample loaded?
    if(buf->sample)
    {
        // Is the same one?
        if(buf->sample->id == sample->id)
            return;

        // Free the existing data.
        buf->sample = nullptr;
        Mix_FreeChunk((Mix_Chunk *) buf->ptr);
    }

    dsize const size = 8 + 4 + 8 + 16 + 8 + sample->size;
    static char localBuf[0x40000];
    char *conv = nullptr;
    if(size <= sizeof(localBuf))
    {
        conv = localBuf;
    }
    else
    {
        conv = (char *) M_Malloc(size);
    }

    // Transfer the sample to SDL_mixer by converting it to WAVE format.
    strcpy(conv, "RIFF");
    *(Uint32 *) (conv + 4) = DD_ULONG(4 + 8 + 16 + 8 + sample->size);
    strcpy(conv + 8, "WAVE");

    // Format chunk.
    strcpy(conv + 12, "fmt ");
    *(Uint32 *) (conv + 16) = DD_ULONG(16);
    /**
     * WORD wFormatTag;         // Format category
     * WORD wChannels;          // Number of channels
     * uint dwSamplesPerSec;    // Sampling rate
     * uint dwAvgBytesPerSec;   // For buffer estimation
     * WORD wBlockAlign;        // Data block size
     * WORD wBitsPerSample;     // Sample size
     */
    *(Uint16 *) (conv + 20) = DD_USHORT(1);
    *(Uint16 *) (conv + 22) = DD_USHORT(1);
    *(Uint32 *) (conv + 24) = DD_ULONG(sample->rate);
    *(Uint32 *) (conv + 28) = DD_ULONG(sample->rate * sample->bytesPer);
    *(Uint16 *) (conv + 32) = DD_USHORT(sample->bytesPer);
    *(Uint16 *) (conv + 34) = DD_USHORT(sample->bytesPer * 8);

    // Data chunk.
    strcpy(conv + 36, "data");
    *(Uint32 *) (conv + 40) = DD_ULONG(sample->size);
    memcpy(conv + 44, sample->data, sample->size);

    buf->ptr = Mix_LoadWAV_RW(SDL_RWFromMem(conv, 44 + sample->size), 1);
    if(!buf->ptr)
    {
        LOG_AS("DS_SDLMixer_SFX_Load");
        LOG_AUDIO_WARNING("Failed loading sample: %s") << Mix_GetError();
    }

    if(conv != localBuf)
    {
        M_Free(conv);
    }

    buf->sample = sample;
}

/**
 * Stops the buffer and makes it forget about its sample.
 */
void DS_SDLMixer_SFX_Reset(sfxbuffer_t *buf)
{
    DENG2_ASSERT(buf);

    DS_SDLMixer_SFX_Stop(buf);
    buf->sample = nullptr;

    // Unallocate the resources of the source.
    Mix_FreeChunk((Mix_Chunk *) buf->ptr);
    buf->ptr = nullptr;
}

void DS_SDLMixer_SFX_Play(sfxbuffer_t *buf)
{
    DENG2_ASSERT(buf);

    // Playing is quite impossible without a sample.
    if(!buf->sample) return;

    // Update the volume at which the sample will be played.
    Mix_Volume(buf->cursor, buf->written);
    Mix_PlayChannel(buf->cursor, (Mix_Chunk *) buf->ptr, (buf->flags & SFXBF_REPEAT ? -1 : 0));

    // Calculate the end time (milliseconds).
    buf->endTime = Timer_RealMilliseconds() + getBufLength(*buf);

    // The buffer is now playing.
    buf->flags |= SFXBF_PLAYING;
}

void DS_SDLMixer_SFX_Stop(sfxbuffer_t *buf)
{
    DENG2_ASSERT(buf);

    if(!buf->sample) return;

    Mix_HaltChannel(buf->cursor);
    //usedChannels[buf->cursor] = false;
    buf->flags &= ~SFXBF_PLAYING;
}

void DS_SDLMixer_SFX_Refresh(sfxbuffer_t *buf)
{
    DENG2_ASSERT(buf);

    // Can only be done if there is a sample and the buffer is playing.
    if(!buf->sample || !(buf->flags & SFXBF_PLAYING))
        return;

    duint const nowTime = Timer_RealMilliseconds();

    /**
     * Have we passed the predicted end of sample?
     * @note This test fails if the game has been running for about 50 days,
     * since the millisecond counter overflows. It only affects sounds that
     * are playing while the overflow happens, though.
     */
    if(!(buf->flags & SFXBF_REPEAT) && nowTime >= buf->endTime)
    {
        // Time for the sound to stop.
        buf->flags &= ~SFXBF_PLAYING;
    }
}

void DS_SDLMixer_SFX_Set(sfxbuffer_t *buf, int prop, float value)
{
    DENG2_ASSERT(buf);

    switch(prop)
    {
    case SFXBP_VOLUME:
        // 'written' is used for storing the volume of the channel.
        buf->written = duint( value * MIX_MAX_VOLUME );
        Mix_Volume(buf->cursor, buf->written);
        break;

    case SFXBP_PAN: { // -1 ... +1
        auto const right = dint( (value + 1) * 127 );
        Mix_SetPanning(buf->cursor, 254 - right, right);
        break; }

    default: break;
    }
}

void DS_SDLMixer_SFX_Setv(sfxbuffer_t *, int , float *)
{
    // Not supported.
}

void DS_SDLMixer_SFX_Listener(int, float)
{
    // Not supported.
}

void SetEnvironment(float *)
{
    // Not supported.
}

void DS_SDLMixer_SFX_Listenerv(int, float *)
{
    // Not supported.
}

int DS_SDLMixer_Music_Init()
{
#ifdef DENG2_DEBUG
    Mix_HookMusicFinished(musicPlaybackFinished);
#endif

    return ::sdlInitOk;
}

void DS_SDLMixer_Music_Update()
{
    // Nothing to update.
}

void DS_SDLMixer_Music_Set(int prop, float value)
{
    if(!::sdlInitOk) return;

    switch(prop)
    {
    case MUSIP_VOLUME:
        Mix_VolumeMusic(dint( MIX_MAX_VOLUME * value ));
        break;

    default: break;
    }
}

int DS_SDLMixer_Music_Get(int prop, void *value)
{
    if(!::sdlInitOk) return false;

    switch(prop)
    {
    case MUSIP_ID:
        strcpy((char *) value, "SDLMixer::Music");
        return true;

    case MUSIP_PLAYING:
        return Mix_PlayingMusic();

    default: return false;
    }
}

void DS_SDLMixer_Music_Pause(int pause)
{
    if(!::sdlInitOk) return;

    if(pause)
        Mix_PauseMusic();
    else
        Mix_ResumeMusic();
}

void DS_SDLMixer_Music_Stop()
{
    if(!::sdlInitOk) return;

    Mix_HaltMusic();
}

int DS_SDLMixer_Music_PlayFile(char const *filename, int looped)
{
    if(!::sdlInitOk) return false;

    // Free any previously loaded music.
    if(::lastMusic)
    {
        Mix_HaltMusic();
        Mix_FreeMusic(::lastMusic);
    }

    ::lastMusic = Mix_LoadMUS(filename);
    if(!lastMusic)
    {
        LOG_AS("DS_SDLMixer_Music_PlayFile");
        LOG_AUDIO_ERROR("Failed to load music: %s") << Mix_GetError();
        return false;
    }

    return !Mix_PlayMusic(lastMusic, looped ? -1 : 1);
}

#endif  // DENG_DISABLE_SDLMIXER
