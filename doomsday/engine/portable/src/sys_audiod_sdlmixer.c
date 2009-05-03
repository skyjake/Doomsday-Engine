/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2007-2009 Daniel Swanson <danij@dengine.net>
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
 * sys_audiod_sdlmixer.h: SDL_mixer, for SFX, Ext and Mus interfaces.
 */

// HEADER FILES ------------------------------------------------------------

#include <stdlib.h>
#include <string.h>

#ifdef MACOSX
#  include <SDL.h>
#  include <SDL_mixer.h>
#else
#  include <SDL.h>
#  include <SDL_mixer.h>
#endif

#include "de_base.h"
#include "de_console.h"
#include "de_system.h"
#include "de_misc.h"

#include "sys_audiod.h"
#include "sys_audiod_sfx.h"
#include "sys_audiod_mus.h"
#include "m_mus2midi.h"

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

audiointerface_sfx_t audiod_sdlmixer_sfx = {
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
};

audiointerface_music_t audiod_sdlmixer_music = {
    DS_SDLMixer_Music_Init,
    DS_SDLMixer_Music_Update,
    DS_SDLMixer_Music_Set,
    DS_SDLMixer_Music_Get,
    DS_SDLMixer_Music_Pause,
    DS_SDLMixer_Music_Stop,
    NULL,
    NULL,
    DS_SDLMixer_Music_PlayFile,
};

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static char storage[0x40000];
static int channelCounter = 0;

static Mix_Music* currentMusic;

// CODE --------------------------------------------------------------------

static void msg(const char* msg)
{
    Con_Message("SDLMixer: %s\n", msg);
}

/**
 * This is the hook we ask SDL_mixer to call when music playback finishes.
 */
static void musicPlaybackFinished(void)
{
    if(currentMusic)
    {
        Mix_FreeMusic(currentMusic);
        currentMusic = NULL;
    }
}

void DS_SDLMixerError(void)
{
    char                buf[500];

    sprintf(buf, "ERROR: %s", Mix_GetError());
    msg(buf);
}

int DS_SDLMixerInit(void)
{
    if(sdlInitOk)
        return true;

    // Are we in verbose mode?
    if((verbose = ArgExists("-verbose")))
    {
        msg("Initializing...");
    }

    if(SDL_InitSubSystem(SDL_INIT_AUDIO))
    {
        msg(SDL_GetError());
        return false;
    }

    if(Mix_OpenAudio(MIX_DEFAULT_FREQUENCY, MIX_DEFAULT_FORMAT, 2, 1024))
    {
        DS_SDLMixerError();
        return false;
    }

    // Prepare to play 16 simultaneous sounds.
    Mix_AllocateChannels(MIX_CHANNELS);
    channelCounter = 0;

    // Everything is OK.
    sdlInitOk = true;
    return true;
}

void DS_SDLMixerShutdown(void)
{
    if(!sdlInitOk)
        return;

    Mix_CloseAudio();
    SDL_QuitSubSystem(SDL_INIT_AUDIO);

    if(currentMusic)
        Mix_FreeMusic(currentMusic);
    currentMusic = NULL;

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
    buf = Z_Calloc(sizeof(*buf), PU_STATIC, 0);

    buf->bytes = bits / 8;
    buf->rate = rate;
    buf->flags = flags;
    buf->freq = rate; // Modified by calls to Set(SFXBP_FREQUENCY).

    // The cursor is used to keep track of the channel on which the sample
    // is playing.
    buf->cursor = channelCounter++;

    // Make sure we have enough channels allocated.
    if(channelCounter > MIX_CHANNELS)
    {
        Mix_AllocateChannels(channelCounter);
    }

    return buf;
}

void DS_SDLMixer_SFX_DestroyBuffer(sfxbuffer_t* buf)
{
    // Ugly, but works because the engine creates and destroys buffers
    // only in batches.
    channelCounter = 0;

    if(buf)
        Z_Free(buf);
}

/**
 * This is not thread-safe, but it doesn't matter because only one
 * thread will be calling it.
 */
static char* allocLoadStorage(unsigned int size)
{
    if(size == 0)
        return NULL;

    if(size <= sizeof(storage))
    {
        return storage;
    }

    return malloc(size);
}

static void freeLoadStorage(char* data)
{
    if(data && data != storage)
    {
        free(data);
    }
}

void DS_SDLMixer_SFX_Load(sfxbuffer_t* buf, struct sfxsample_s* sample)
{
    char*               conv = NULL;

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
        Mix_FreeChunk(buf->ptr);
    }

    conv = allocLoadStorage(8 + 4 + 8 + 16 + 8 + sample->size);

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
    if(!sample)
    {
        printf("Mix_LoadWAV_RW: %s\n", Mix_GetError());
    }

    freeLoadStorage(conv);

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
    Mix_FreeChunk(buf->ptr);
    buf->ptr = NULL;
}

void DS_SDLMixer_SFX_Play(sfxbuffer_t* buf)
{
    // Playing is quite impossible without a sample.
    if(!buf || !buf->sample)
        return;

    // Update the volume at which the sample will be played.
    Mix_Volume(buf->cursor, buf->written);

    Mix_PlayChannel(buf->cursor, buf->ptr,
                    (buf->flags & SFXBF_REPEAT ? -1 : 0));

    // The buffer is now playing.
    buf->flags |= SFXBF_PLAYING;
}

void DS_SDLMixer_SFX_Stop(sfxbuffer_t* buf)
{
    if(!buf || !buf->sample)
        return;

    Mix_HaltChannel(buf->cursor);
    buf->flags &= ~SFXBF_PLAYING;
}

void DS_SDLMixer_SFX_Refresh(sfxbuffer_t* buf)
{
    if(!buf || !buf->ptr || !buf->sample)
        return;

    // Has the buffer finished playing?
    if(!Mix_Playing(buf->cursor))
    {   // It has stopped playing.
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
    Mix_HookMusicFinished(musicPlaybackFinished);
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
        strcpy(value, "SDLMixer/Music");
        break;

    case MUSIP_PLAYING:
        return (currentMusic? true : false);

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

    Mix_SetMusicCMD(NULL);

    // Free any previously loaded music.
    if(currentMusic)
        Mix_FreeMusic(currentMusic);

    if(!(currentMusic = Mix_LoadMUS(filename)))
    {
        DS_SDLMixerError();
        return false;
    }

    return !Mix_PlayMusic(currentMusic, looped ? -1 : 1);
}
