/**\file sys_audiod_sdlmixer.cpp
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2007-2013 Daniel Swanson <danij@dengine.net>
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

/**
 * SDL_mixer, for SFX, Ext and Mus interfaces.
 */

#ifndef DENG_DISABLE_SDLMIXER

// HEADER FILES ------------------------------------------------------------

#include <stdlib.h>
#include <string.h>

#include <SDL.h>
#include <SDL_mixer.h>

#include "de_base.h"
#include "de_console.h"
#include "de_system.h"
#include "de_misc.h"

#include "api_audiod.h"
#include "api_audiod_sfx.h"
#include "api_audiod_mus.h"

#include "audio/sys_audiod_sdlmixer.h"

// MACROS ------------------------------------------------------------------

#define DEFAULT_MIDI_COMMAND    "" //"timidity"

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

int         DS_SDLMixerInit(void);
void        DS_SDLMixerShutdown(void);
void        DS_SDLMixerEvent(int type);

int         DS_SDLMixer_SFX_Init(void);
sfxbuffer_t* DS_SDLMixer_SFX_CreateBuffer(int flags, int bits, int rate);
void        DS_SDLMixer_SFX_DestroyBuffer(sfxbuffer_t* buf);
void        DS_SDLMixer_SFX_Load(sfxbuffer_t* buf, struct sfxsample_s* sample);
void        DS_SDLMixer_SFX_Reset(sfxbuffer_t* buf);
void        DS_SDLMixer_SFX_Play(sfxbuffer_t* buf);
void        DS_SDLMixer_SFX_Stop(sfxbuffer_t* buf);
void        DS_SDLMixer_SFX_Refresh(sfxbuffer_t* buf);
void        DS_SDLMixer_SFX_Set(sfxbuffer_t* buf, int prop, float value);
void        DS_SDLMixer_SFX_Setv(sfxbuffer_t* buf, int prop, float* values);
void        DS_SDLMixer_SFX_Listener(int prop, float value);
void        DS_SDLMixer_SFX_Listenerv(int prop, float* values);

// The music interface.
int         DS_SDLMixer_Music_Init(void);
void        DS_SDLMixer_Music_Update(void);
void        DS_SDLMixer_Music_Set(int prop, float value);
int         DS_SDLMixer_Music_Get(int prop, void* value);
void        DS_SDLMixer_Music_Pause(int pause);
void        DS_SDLMixer_Music_Stop(void);
int         DS_SDLMixer_Music_PlayFile(const char* fileName, int looped);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

boolean sdlInitOk = false;

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
    NULL,
    DS_SDLMixer_Music_Update,
    DS_SDLMixer_Music_Set,
    DS_SDLMixer_Music_Get,
    DS_SDLMixer_Music_Pause,
    DS_SDLMixer_Music_Stop },
    NULL,
    NULL,
    DS_SDLMixer_Music_PlayFile,
};

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static int numChannels;
static boolean* usedChannels;

static Mix_Music* lastMusic;
//static boolean playingMusic = false;

// CODE --------------------------------------------------------------------

/**
 * This is the hook we ask SDL_mixer to call when music playback finishes.
 */
#if _DEBUG
static void musicPlaybackFinished(void)
{
    Con_Printf("DS_SDLMixer: Music playback finished.\n");
}
#endif

static int getFreeChannel(void)
{
    int                 i;

    for(i = 0; i < numChannels; ++i)
    {
        if(!usedChannels[i])
            return i;
    }

    return -1;
}

/**
 * @return              Length of the buffer in milliseconds.
 */
static unsigned int getBufLength(sfxbuffer_t* buf)
{
    if(!buf)
        return 0;

    return 1000 * buf->sample->numSamples / buf->freq;
}

int DS_SDLMixerInit(void)
{
    int                 freq, channels;
    uint16_t            format;
    SDL_version         compVer;
    const SDL_version*  linkVer;

    if(sdlInitOk)
        return true;

    if(SDL_InitSubSystem(SDL_INIT_AUDIO))
    {
        Con_Message("Warning:DS_SDLMixerInit: Error initializing SDL AUDIO\n %s", SDL_GetError());
        return false;
    }

    SDL_MIXER_VERSION(&compVer);
    linkVer = Mix_Linked_Version();

    if(SDL_VERSIONNUM(linkVer->major, linkVer->minor, linkVer->patch) >
       SDL_VERSIONNUM(compVer.major, compVer.minor, compVer.patch))
    {
        Con_Message("Warning:DS_SDLMixerInit: Linked version of SDLMixer (%u.%u.%u) is "
                    "newer than expected (%u.%u.%u)", linkVer->major, linkVer->minor,
                    linkVer->patch, compVer.major, compVer.minor, compVer.patch);
    }

    if(Mix_OpenAudio(MIX_DEFAULT_FREQUENCY, MIX_DEFAULT_FORMAT, 2, 1024))
    {
        Con_Message("Warning:DS_SDLMixerInit: Failed opening mixer %s.", Mix_GetError());
        return false;
    }

    Mix_QuerySpec(&freq, &format, &channels);

    if(verbose)
    {   // Announce capabilites.
        Con_Printf("SDLMixer configuration:\n");
        Con_Printf("  Output: %s\n", channels > 1? "stereo" : "mono");
        Con_Printf("  Format: %#x (%#x)\n", format, (uint16_t) AUDIO_S16LSB);
        Con_Printf("  Frequency: %iHz (%iHz)\n", freq, (int) MIX_DEFAULT_FREQUENCY);
        Con_Printf("  Initial Channels: %i\n", MIX_CHANNELS);
    }

    // Prepare to play simultaneous sounds.
    /*numChannels =*/ Mix_AllocateChannels(MIX_CHANNELS);
    usedChannels = NULL;

    // Everything is OK.
    sdlInitOk = true;
    return true;
}

void DS_SDLMixerShutdown(void)
{
    if(!sdlInitOk)
        return;

    if(usedChannels)
        M_Free(usedChannels);

    if(lastMusic)
    {
        Mix_HaltMusic();
        Mix_FreeMusic(lastMusic);
    }
    lastMusic = NULL;

    Mix_CloseAudio();
    SDL_QuitSubSystem(SDL_INIT_AUDIO);

    sdlInitOk = false;
}

void DS_SDLMixerEvent(int type)
{
    // Not supported.
}

int DS_SDLMixer_SFX_Init(void)
{
    // No extra init needed.
    return sdlInitOk;
}

sfxbuffer_t* DS_SDLMixer_SFX_CreateBuffer(int flags, int bits, int rate)
{
    sfxbuffer_t*        buf;

    // Create the buffer.
    buf = (sfxbuffer_t *) Z_Calloc(sizeof(*buf), PU_APPSTATIC, 0);

    buf->bytes = bits / 8;
    buf->rate = rate;
    buf->flags = flags;
    buf->freq = rate; // Modified by calls to Set(SFXBP_FREQUENCY).

    // The cursor is used to keep track of the channel on which the sample
    // is playing.
    if(-1 == (buf->cursor = getFreeChannel()))
    {
        buf->cursor = numChannels++;
        usedChannels = (ddboolean_t *) M_Realloc(usedChannels, sizeof(usedChannels[0]) * numChannels);

        // Make sure we have enough channels allocated.
        Mix_AllocateChannels(numChannels);

        Mix_UnregisterAllEffects(buf->cursor);
    }

    usedChannels[buf->cursor] = true;

    return buf;
}

void DS_SDLMixer_SFX_DestroyBuffer(sfxbuffer_t* buf)
{
    Mix_HaltChannel(buf->cursor);
    usedChannels[buf->cursor] = false;

    if(buf)
        Z_Free(buf);
}

void DS_SDLMixer_SFX_Load(sfxbuffer_t* buf, struct sfxsample_s* sample)
{
    static char         localBuf[0x40000];
    char*               conv = NULL;
    size_t              size;

    if(!buf || !sample)
        return; // Wha?

    // Does the buffer already have a sample loaded?
    if(buf->sample)
    {
        // Is the same one?
        if(buf->sample->id == sample->id)
            return;

        // Free the existing data.
        buf->sample = NULL;
        Mix_FreeChunk((Mix_Chunk *) buf->ptr);
    }

    size = 8 + 4 + 8 + 16 + 8 + sample->size;
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
    *(Uint32 *) (conv + 4) = ULONG(4 + 8 + 16 + 8 + sample->size);
    strcpy(conv + 8, "WAVE");

    // Format chunk.
    strcpy(conv + 12, "fmt ");
    *(Uint32 *) (conv + 16) = ULONG(16);
    /**
     * WORD wFormatTag;         // Format category
     * WORD wChannels;          // Number of channels
     * uint dwSamplesPerSec;    // Sampling rate
     * uint dwAvgBytesPerSec;   // For buffer estimation
     * WORD wBlockAlign;        // Data block size
     * WORD wBitsPerSample;     // Sample size
     */
    *(Uint16 *) (conv + 20) = USHORT(1);
    *(Uint16 *) (conv + 22) = USHORT(1);
    *(Uint32 *) (conv + 24) = ULONG(sample->rate);
    *(Uint32 *) (conv + 28) = ULONG(sample->rate * sample->bytesPer);
    *(Uint16 *) (conv + 32) = USHORT(sample->bytesPer);
    *(Uint16 *) (conv + 34) = USHORT(sample->bytesPer * 8);

    // Data chunk.
    strcpy(conv + 36, "data");
    *(Uint32 *) (conv + 40) = ULONG(sample->size);
    memcpy(conv + 44, sample->data, sample->size);

    buf->ptr = Mix_LoadWAV_RW(SDL_RWFromMem(conv, 44 + sample->size), 1);
    if(!buf->ptr)
    {
        Con_Message("DS_SDLMixer_SFX_Load: Warning, failed loading sample (%s).",
                    Mix_GetError());
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
void DS_SDLMixer_SFX_Reset(sfxbuffer_t* buf)
{
    if(!buf)
        return;

    DS_SDLMixer_SFX_Stop(buf);
    buf->sample = NULL;

    // Unallocate the resources of the source.
    Mix_FreeChunk((Mix_Chunk *) buf->ptr);
    buf->ptr = NULL;
}

void DS_SDLMixer_SFX_Play(sfxbuffer_t* buf)
{
    // Playing is quite impossible without a sample.
    if(!buf || !buf->sample)
        return;

    // Update the volume at which the sample will be played.
    Mix_Volume(buf->cursor, buf->written);
    Mix_PlayChannel(buf->cursor, (Mix_Chunk *) buf->ptr, (buf->flags & SFXBF_REPEAT ? -1 : 0));

    // Calculate the end time (milliseconds).
    buf->endTime = Timer_RealMilliseconds() + getBufLength(buf);

    // The buffer is now playing.
    buf->flags |= SFXBF_PLAYING;
}

void DS_SDLMixer_SFX_Stop(sfxbuffer_t* buf)
{
    if(!buf || !buf->sample)
        return;

    Mix_HaltChannel(buf->cursor);
    //usedChannels[buf->cursor] = false;
    buf->flags &= ~SFXBF_PLAYING;
}

void DS_SDLMixer_SFX_Refresh(sfxbuffer_t* buf)
{
    unsigned int        nowTime;

    // Can only be done if there is a sample and the buffer is playing.
    if(!buf || !buf->sample || !(buf->flags & SFXBF_PLAYING))
        return;

    nowTime = Timer_RealMilliseconds();

    /**
     * Have we passed the predicted end of sample?
     * \note This test fails if the game has been running for about 50 days,
     * since the millisecond counter overflows. It only affects sounds that
     * are playing while the overflow happens, though.
     */
    if(!(buf->flags & SFXBF_REPEAT) && nowTime >= buf->endTime)
    {
        // Time for the sound to stop.
        buf->flags &= ~SFXBF_PLAYING;
    }
}

void DS_SDLMixer_SFX_Set(sfxbuffer_t* buf, int prop, float value)
{
    int                 right;

    if(!buf)
        return;

    switch(prop)
    {
    case SFXBP_VOLUME:
        // 'written' is used for storing the volume of the channel.
        buf->written = (unsigned int) (value * MIX_MAX_VOLUME);
        Mix_Volume(buf->cursor, buf->written);
        break;

    case SFXBP_PAN: // -1 ... +1
        right = (int) ((value + 1) * 127);
        Mix_SetPanning(buf->cursor, 254 - right, right);
        break;

    default:
        break;
    }
}

void DS_SDLMixer_SFX_Setv(sfxbuffer_t* buf, int prop, float* values)
{
    // Not supported.
}

void DS_SDLMixer_SFX_Listener(int prop, float value)
{
    // Not supported.
}

void SetEnvironment(float* rev)
{
    // Not supported.
}

void DS_SDLMixer_SFX_Listenerv(int prop, float* values)
{
    // Not supported.
}

int DS_SDLMixer_Music_Init(void)
{
#if _DEBUG
    Mix_HookMusicFinished(musicPlaybackFinished);
#endif

    return sdlInitOk;
}

void DS_SDLMixer_Music_Update(void)
{
    // Nothing to update.
}

void DS_SDLMixer_Music_Set(int prop, float value)
{
    if(!sdlInitOk)
        return;

    switch(prop)
    {
    case MUSIP_VOLUME:
        Mix_VolumeMusic((int) (MIX_MAX_VOLUME * value));
        break;

    default:
        break;
    }
}

int DS_SDLMixer_Music_Get(int prop, void* value)
{
    if(!sdlInitOk)
        return false;

    switch(prop)
    {
    case MUSIP_ID:
        strcpy((char *) value, "SDLMixer::Music");
        break;

    case MUSIP_PLAYING:
        return Mix_PlayingMusic();

    default:
        return false;
    }
    return true;
}

void DS_SDLMixer_Music_Pause(int pause)
{
    if(!sdlInitOk)
        return;

    if(pause)
        Mix_PauseMusic();
    else
        Mix_ResumeMusic();
}

void DS_SDLMixer_Music_Stop(void)
{
    if(!sdlInitOk)
        return;

    Mix_HaltMusic();
}

int DS_SDLMixer_Music_PlayFile(const char* filename, int looped)
{
    if(!sdlInitOk)
        return false;

    // Free any previously loaded music.
    if(lastMusic)
    {
        Mix_HaltMusic();
        Mix_FreeMusic(lastMusic);
    }

    if(!(lastMusic = Mix_LoadMUS(filename)))
    {
        Con_Message("DS_SDLMixer_Music_PlayFile: Error %s.", Mix_GetError());
        return false;
    }

    return !Mix_PlayMusic(lastMusic, looped ? -1 : 1);
}

#endif // DENG_DISABLE_SDLMIXER
