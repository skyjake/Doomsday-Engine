/* DE1: $Id$
 * Copyright (C) 2004 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * along with this program; if not: http://www.opensource.org/
 */

/*
 * driver_sdlmixer.c: SDL_mixer sound driver for Doomsday
 */

// HEADER FILES ------------------------------------------------------------

#include "driver.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

int     DS_Init(void);
void    DS_Shutdown(void);
sfxbuffer_t *DS_CreateBuffer(int flags, int bits, int rate);
void    DS_DestroyBuffer(sfxbuffer_t * buf);
void    DS_Load(sfxbuffer_t * buf, struct sfxsample_s *sample);
void    DS_Reset(sfxbuffer_t * buf);
void    DS_Play(sfxbuffer_t * buf);
void    DS_Stop(sfxbuffer_t * buf);
void    DS_Refresh(sfxbuffer_t * buf);
void    DS_Event(int type);
void    DS_Set(sfxbuffer_t * buf, int property, float value);
void    DS_Setv(sfxbuffer_t * buf, int property, float *values);
void    DS_Listener(int property, float value);
void    DS_Listenerv(int property, float *values);

// The Ext music interface.
int     DM_Ext_Init(void);
void    DM_Ext_Update(void);
void    DM_Ext_Set(int property, float value);
int     DM_Ext_Get(int property, void *value);
void    DM_Ext_Pause(int pause);
void    DM_Ext_Stop(void);
void   *DM_Ext_SongBuffer(int length);
int     DM_Ext_PlayFile(const char *filename, int looped);
int     DM_Ext_PlayBuffer(int looped);

// The MUS music interface.
int     DM_Mus_Init(void);
void    DM_Mus_Update(void);
void    DM_Mus_Set(int property, float value);
int     DM_Mus_Get(int property, void *value);
void    DM_Mus_Pause(int pause);
void    DM_Mus_Stop(void);
void   *DM_Mus_SongBuffer(int length);
int     DM_Mus_Play(int looped);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

boolean sdlInitOk = false;

static int verbose;

static char storage[0x40000];
static int channelCounter = 0;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

static void Msg(const char *msg)
{
    Con_Message("SDLMixer: %s\n", msg);
}

void DS_Error(void)
{
    char    buf[500];

    sprintf(buf, "ERROR: %s", Mix_GetError());
    Msg(buf);
}

int DS_Init(void)
{
    if(sdlInitOk)
        return true;

    // Are we in verbose mode?
    if((verbose = ArgExists("-verbose")))
    {
        Msg("Initializing...");
    }

    if(SDL_InitSubSystem(SDL_INIT_AUDIO))
    {
        Msg(SDL_GetError());
        return false;
    }

    if(Mix_OpenAudio(MIX_DEFAULT_FREQUENCY, MIX_DEFAULT_FORMAT, 2, 1024))
    {
        DS_Error();
        return false;
    }

    // Prepare to play 16 simultaneous sounds.
    Mix_AllocateChannels(MIX_CHANNELS);
    channelCounter = 0;

    // Everything is OK.
    sdlInitOk = true;
    return true;
}

//===========================================================================
// DS_Shutdown
//===========================================================================
void DS_Shutdown(void)
{
    if(!sdlInitOk)
        return;

    Mix_CloseAudio();
    SDL_QuitSubSystem(SDL_INIT_AUDIO);

    ExtMus_Shutdown();

    sdlInitOk = false;
}

//===========================================================================
// DS_CreateBuffer
//===========================================================================
sfxbuffer_t *DS_CreateBuffer(int flags, int bits, int rate)
{
    sfxbuffer_t *buf;

    // Create the buffer.
    buf = Z_Malloc(sizeof(*buf), PU_STATIC, 0);
    memset(buf, 0, sizeof(*buf));
    buf->bytes = bits / 8;
    buf->rate = rate;
    buf->flags = flags;
    buf->freq = rate;           // Modified by calls to Set(SFXBP_FREQUENCY).

    // The cursor is used to keep track of the channel on which the
    // sample is playing.
    buf->cursor = channelCounter++;

    // Make sure we have enough channels allocated.
    if(channelCounter > MIX_CHANNELS)
    {
        Mix_AllocateChannels(channelCounter);
    }

    return buf;
}

//===========================================================================
// DS_DestroyBuffer
//===========================================================================
void DS_DestroyBuffer(sfxbuffer_t * buf)
{
    // Ugly, but works because the engine creates and destroys buffers
    // only in batches.
    channelCounter = 0;

    Z_Free(buf);
}

// This is not thread-safe, but it doesn't matter because only one
// thread will be calling it.
static char *AllocLoadStorage(unsigned int size)
{
    if(size <= sizeof(storage))
    {
        return storage;
    }
    return malloc(size);
}

static void FreeLoadStorage(char *data)
{
    if(data != storage)
    {
        free(data);
    }
}

//===========================================================================
// DS_Load
//===========================================================================
void DS_Load(sfxbuffer_t * buf, struct sfxsample_s *sample)
{
    char   *conv = NULL;

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

    conv = AllocLoadStorage(8 + 4 + 8 + 16 + 8 + sample->size);

    // We will transfer the sample to SDL_mixer by converting it to
    // WAVE format.
    strcpy(conv, "RIFF");
    *(Uint32 *) (conv + 4) = ULONG(4 + 8 + 16 + 8 + sample->size);
    strcpy(conv + 8, "WAVE");

    // Format chunk.
    strcpy(conv + 12, "fmt ");
    *(Uint32 *) (conv + 16) = ULONG(16);
    /*  WORD    wFormatTag;         // Format category
       WORD wChannels;          // Number of channels
       uint dwSamplesPerSec;    // Sampling rate
       uint dwAvgBytesPerSec;   // For buffer estimation
       WORD wBlockAlign;        // Data block size
       WORD wBitsPerSample;     // Sample size
     */
    *(Uint16 *) (conv + 20) = USHORT(1);
    *(Uint16 *) (conv + 22) = USHORT(1);
    *(Uint32 *) (conv + 24) = ULONG(sample->rate);
    *(Uint32 *) (conv + 28) = ULONG(sample->rate * sample->bytesper);
    *(Uint16 *) (conv + 32) = USHORT(sample->bytesper);
    *(Uint16 *) (conv + 34) = USHORT(sample->bytesper * 8);

    // Data chunk.
    strcpy(conv + 36, "data");
    *(Uint32 *) (conv + 40) = ULONG(sample->size);
    memcpy(conv + 44, sample->data, sample->size);

    buf->ptr = Mix_LoadWAV_RW(SDL_RWFromMem(conv, 44 + sample->size), 1);
    if(!sample)
    {
        printf("Mix_LoadWAV_RW: %s\n", Mix_GetError());
    }

    FreeLoadStorage(conv);

    buf->sample = sample;
}

//===========================================================================
// DS_Reset
//  Stops the buffer and makes it forget about its sample.
//===========================================================================
void DS_Reset(sfxbuffer_t * buf)
{
    DS_Stop(buf);
    buf->sample = NULL;

    // Unallocate the resources of the source.
    Mix_FreeChunk(buf->ptr);
    buf->ptr = NULL;
}

//===========================================================================
// DS_Play
//===========================================================================
void DS_Play(sfxbuffer_t * buf)
{
    // Playing is quite impossible without a sample.
    if(!buf->sample)
        return;

    // Update the volume at which the sample will be played.
    Mix_Volume(buf->cursor, buf->written);

    Mix_PlayChannel(buf->cursor, buf->ptr,
                    (buf->flags & SFXBF_REPEAT ? -1 : 0));

    // The buffer is now playing.
    buf->flags |= SFXBF_PLAYING;
}

//===========================================================================
// DS_Stop
//===========================================================================
void DS_Stop(sfxbuffer_t * buf)
{
    if(!buf->sample)
        return;
    Mix_HaltChannel(buf->cursor);
    buf->flags &= ~SFXBF_PLAYING;
}

//===========================================================================
// DS_Refresh
//===========================================================================
void DS_Refresh(sfxbuffer_t * buf)
{
    if(!buf->ptr || !buf->sample)
        return;

    // Has the buffer finished playing?
    if(!Mix_Playing(buf->cursor))
    {
        // It has stopped playing.
        buf->flags &= ~SFXBF_PLAYING;
    }
}

//===========================================================================
// DS_Event
//===========================================================================
void DS_Event(int type)
{
}

#if 0
//===========================================================================
// SetPan
//  Pan is linear, from -1 to 1. 0 is in the middle.
//===========================================================================
void SetPan(IA3dSource2 * source, float pan)
{
    /*  float gains[2];

       if(pan < -1) pan = -1;
       if(pan > 1) pan = 1;

       if(pan == 0)
       {
       gains[0] = gains[1] = 1; // In the center.
       }
       else if(pan < 0) // On the left?
       {
       gains[0] = 1;
       //gains[1] = pow(.5, (100*(1 - 2*pan))/6);
       gains[1] = 1 + pan;
       }
       else if(pan > 0) // On the right?
       {
       //gains[0] = pow(.5, (100*(2*(pan-0.5)))/6);
       gains[0] = 1 - pan;
       gains[1] = 1;
       }
       source->SetPanValues(2, gains); */
}
#endif

//===========================================================================
// DS_Set
//===========================================================================
void DS_Set(sfxbuffer_t * buf, int property, float value)
{
    /*  DWORD dw;
       IA3dSource2 *s = Src(buf);
       float minDist, maxDist;
     */
    int     right;

    switch (property)
    {
    case SFXBP_VOLUME:
        // 'written' is used for storing the volume of the channel.
        buf->written = value * MIX_MAX_VOLUME;
        Mix_Volume(buf->cursor, buf->written);
        break;

        /*  case SFXBP_FREQUENCY:
           dw = DWORD(buf->rate * value);
           if(dw != buf->freq)  // Don't set redundantly.
           {
           buf->freq = dw;
           s->SetPitch(value);
           }
           break; */

    case SFXBP_PAN:         // -1 ... +1
        right = (value + 1) * 127;
        Mix_SetPanning(buf->cursor, 254 - right, right);
        break;
    }
}

void DS_Setv(sfxbuffer_t * buf, int property, float *values)
{
    // Not supported.
}

void DS_Listener(int property, float value)
{
    // Not supported.
}

void SetEnvironment(float *rev)
{
    // Not supported.
}

void DS_Listenerv(int property, float *values)
{
    // Not supported.
}

