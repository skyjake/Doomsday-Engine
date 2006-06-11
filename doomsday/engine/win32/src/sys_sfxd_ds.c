/* DE1: $Id$
 * Copyright (C) 2003, 2004 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * sys_sfxd_ds.c: Sound Driver for DirectSound
 *
 * DirectSound Sfx Driver, with EAX 2.0.
 *
 * Low-level implementation, with manual sound streaming by the
 * sound refresh thread.
 */

// HEADER FILES ------------------------------------------------------------

#define DIRECTSOUND_VERSION 0x0800
#include <dsound.h>
#include <initguid.h>
#include <eax.h>
#include <math.h>

/*#if _DEBUG
   #define DEBUG
   #include <dprintf.h>
   #endif */

#include "de_base.h"
#include "de_console.h"
#include "de_system.h"
#include "de_misc.h"
#include "sys_sfxd.h"

// MACROS ------------------------------------------------------------------

// DirectSound(3D)Buffer Pointer
#define DSBuf(buf)      ((LPDIRECTSOUNDBUFFER8) buf->ptr)
#define DSBuf3(buf)     ((LPDIRECTSOUND3DBUFFER8) buf->ptr3d)

#define EAXSUP          (KSPROPERTY_SUPPORT_GET|KSPROPERTY_SUPPORT_SET)

#define MAX_FAILED_PROPS    10

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

int     DS_DSoundInit(void);
void    DS_DSoundShutdown(void);
sfxbuffer_t *DS_DSoundCreateBuffer(int flags, int bits, int rate);
void    DS_DSoundDestroyBuffer(sfxbuffer_t * buf);
void    DS_DSoundLoad(sfxbuffer_t * buf, struct sfxsample_s *sample);
void    DS_DSoundReset(sfxbuffer_t * buf);
void    DS_DSoundPlay(sfxbuffer_t * buf);
void    DS_DSoundStop(sfxbuffer_t * buf);
void    DS_DSoundRefresh(sfxbuffer_t * buf);
void    DS_DSoundEvent(int type);
void    DS_DSoundSet(sfxbuffer_t * buf, int property, float value);
void    DS_DSoundSetv(sfxbuffer_t * buf, int property, float *values);
void    DS_DSoundListener(int property, float value);
void    DS_DSoundListenerv(int property, float *values);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

void    DS_EAXCommitDeferred(void);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern HWND hWndMain;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

sfxdriver_t sfxd_dsound = {
    DS_DSoundInit,
    DS_DSoundShutdown,
    DS_DSoundCreateBuffer,
    DS_DSoundDestroyBuffer,
    DS_DSoundLoad,
    DS_DSoundReset,
    DS_DSoundPlay,
    DS_DSoundStop,
    DS_DSoundRefresh,
    DS_DSoundEvent,
    DS_DSoundSet,
    DS_DSoundSetv,
    DS_DSoundListener,
    DS_DSoundListenerv
};

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static IDirectSound8 *dsound = 0;
static IDirectSoundBuffer *primary = 0;
static IDirectSound3DListener *ds_listener = 0;
static IKsPropertySet *eax_listener = 0;
static HRESULT hr;
static boolean ignore_eax_errors = false;

static DWORD failed_props[MAX_FAILED_PROPS];

// CODE --------------------------------------------------------------------

void DS_DSoundError(char *msg)
{
    Con_Message("DS_DSoundError: %s [%x]\n", msg, hr);
}

IDirectSoundBuffer8 *DS_DSoundCreateBuffer8(DSBUFFERDESC * desc)
{
    IDirectSoundBuffer *buf;
    IDirectSoundBuffer8 *buf8;

    // Try to create a secondary buffer with the requested properties.
    if(FAILED(hr = IDirectSound_CreateSoundBuffer(dsound, desc, &buf, NULL)))
        return NULL;

    // Obtain the DirectSoundBuffer8 interface.
    if(FAILED
       (hr =
        IDirectSoundBuffer_QueryInterface(buf, &IID_IDirectSoundBuffer8,
                                          &buf8)))
        buf8 = NULL;

    // Release the old interface, we don't need it.
    IDirectSoundBuffer_Release(buf);
    return buf8;
}

IDirectSound3DBuffer8 *DS_DSoundGet3D(IDirectSoundBuffer8 * buf8)
{
    IDirectSound3DBuffer8 *buf3d;

    if(!buf8)
        return NULL;

    // Query the 3D interface.
    if(FAILED
       (hr =
        IDirectSoundBuffer_QueryInterface(buf8, &IID_IDirectSound3DBuffer8,
                                          &buf3d)))
    {
        DS_DSoundError("Failed to get 3D interface.");
        buf3d = NULL;
    }
    return buf3d;
}

/*
 * Does the EAX implementation support getting/setting of a propertry.
 *
 * @param:  property     Property id (constant) to be checked.
 * @return: boolean      (true) if supported.
 */
boolean DS_EAXHasSupport(int property)
{
    ULONG   support = 0;
    boolean has_support;

    if(!eax_listener)
        return false;

    has_support = SUCCEEDED(hr =
                            IKsPropertySet_QuerySupport(eax_listener,
                                                        &DSPROPSETID_EAX_ListenerProperties,
                                                        property, &support)) &&
                  (support & EAXSUP) == EAXSUP;

    if(verbose)
        Con_Message("DS_EAXHasSupport: Property %i => %s\n", property,
                    has_support ? "Yes" : "No");

    return has_support;
}

/*
 * Not a driver of its own, but part of the DSound driver.
 *
 * @return: int         (true) if EAX is available.
 */
int DS_EAXInit(void)
{
    IDirectSoundBuffer8 *dummy;
    IDirectSound3DBuffer8 *dummy3d;
    DSBUFFERDESC desc;
    WAVEFORMATEX wave;

    // Clear the failed properties array.
    memset(failed_props, ~0, sizeof(failed_props));

    eax_listener = NULL;
    ignore_eax_errors = ArgExists("-eaxignore");

    if(ArgExists("-noeax"))
        return false;

    // Configure the temporary buffer.
    memset(&desc, 0, sizeof(desc));
    desc.dwSize = sizeof(desc);
    desc.dwBufferBytes = DSBSIZE_MIN;
    desc.dwFlags = DSBCAPS_STATIC | DSBCAPS_CTRL3D;
    desc.lpwfxFormat = &wave;

    memset(&wave, 0, sizeof(wave));
    wave.wFormatTag = WAVE_FORMAT_PCM;
    wave.nChannels = 1;
    wave.nSamplesPerSec = 44100;
    wave.wBitsPerSample = 16;
    wave.nBlockAlign = 2;
    wave.nAvgBytesPerSec = 88200;

    // Create the temporary buffer.
    dummy = DS_DSoundCreateBuffer8(&desc);
    if(!dummy)
        return false;

    // Get the 3D interface.
    dummy3d = DS_DSoundGet3D(dummy);
    if(!dummy3d)
        return false;

    // Query the property set interface.
    if(FAILED
       (hr =
        IDirectSound3DBuffer_QueryInterface(dummy3d, &IID_IKsPropertySet,
                                            &eax_listener)))
        return false;

    // Check for EAX support.
    if(!DS_EAXHasSupport(DSPROPERTY_EAXLISTENER_ENVIRONMENT) ||
       !DS_EAXHasSupport(DSPROPERTY_EAXLISTENER_ROOM) ||
       !DS_EAXHasSupport(DSPROPERTY_EAXLISTENER_DECAYTIME) ||
       !DS_EAXHasSupport(DSPROPERTY_EAXLISTENER_ROOMHF) ||
       !DS_EAXHasSupport(DSPROPERTY_EAXLISTENER_ROOMROLLOFFFACTOR))
    {
        Con_Message("DS_EAXInit: Required EAX support missing.\n");
        IKsPropertySet_Release(eax_listener);
        eax_listener = NULL;
        return false;
    }
    return true;
}

/*
 * Init DirectSound, start playing the primary buffer.
 *
 * @return: int         (true) if successful.
 */
int DS_DSoundInit(void)
{
    DSBUFFERDESC desc;

    if(dsound)
        return true;            // Already initialized?

    // First try to create the DirectSound object with EAX support.
    hr = DSERR_GENERIC;
    if(!ArgExists("-noeax"))
    {
        if(SUCCEEDED
           (hr =
            CoCreateInstance(&CLSID_EAXDirectSound8, NULL,
                             CLSCTX_INPROC_SERVER, &IID_IDirectSound8,
                             &dsound)))
        {
            if(verbose)
                Con_Message("DS_DSoundInit: DS8/EAX instance created.\n");
        }
    }
    if(FAILED(hr))
    {
        // Try plain old DS, then.
        if(FAILED
           (hr =
            CoCreateInstance(&CLSID_DirectSound8, NULL, CLSCTX_INPROC_SERVER,
                             &IID_IDirectSound8, &dsound)))
        {
            DS_DSoundError("Failed to create the DS8 instance.");
            return false;
        }
    }

    // Initialize the DSound instance.
    if(FAILED(hr = IDirectSound_Initialize(dsound, NULL)))
    {
        DS_DSoundError("Failed to init DS8.");
        return false;
    }

    // Set cooperative level.
    if(FAILED
       (hr =
        IDirectSound_SetCooperativeLevel(dsound, hWndMain, DSSCL_PRIORITY)))
    {
        DS_DSoundError("Failed to set cooperative level.");
        return false;
    }

    // Create the primary buffer and try to initialize the 3D listener.
    // If it succeeds, 3D sounds can be played.
    ds_listener = NULL;
    memset(&desc, 0, sizeof(desc));
    desc.dwSize = sizeof(desc);
    desc.dwFlags = DSBCAPS_PRIMARYBUFFER | DSBCAPS_CTRL3D;
    if(FAILED
       (hr = IDirectSound_CreateSoundBuffer(dsound, &desc, &primary, NULL)))
    {
        DS_DSoundError("3D not available.");
        // Create a 2D primary buffer instead.
        desc.dwFlags = DSBCAPS_PRIMARYBUFFER;
        if(FAILED
           (hr =
            IDirectSound_CreateSoundBuffer(dsound, &desc, &primary, NULL)))
        {
            DS_DSoundError("Failed to create 2D primary buffer.");
            return false;
        }
    }
    else
    {
        // Get the listener.
        if(FAILED
           (hr =
            IDirectSoundBuffer_QueryInterface(primary,
                                              &IID_IDirectSound3DListener,
                                              &ds_listener)))
        {
            DS_DSoundError("3D listener not available.");
        }
    }

    // Supposedly can be a bit more efficient not to stop the
    // primary buffer when there are no secondary buffers playing.
    IDirectSoundBuffer_Play(primary, 0, 0, DSBPLAY_LOOPING);

    // How about some EAX?
    if(DS_EAXInit())
    {
        Con_Message("DS_DSoundInit: EAX initialized.\n");
    }
    return true;
}

/*
 * Shut everything down.
 */
void DS_DSoundShutdown(void)
{
    IDirectSound_Release(dsound);
    dsound = NULL;
    primary = NULL;
    ds_listener = NULL;
    eax_listener = NULL;
}

/*
 * The Event function is called to tell the driver about certain critical
 * events like the beginning and end of an update cycle.
 */
void DS_DSoundEvent(int type)
{
    // Do nothing...
}

/*
 * Called using Listener().
 */
void DS_DSoundSetPrimaryFormat(int bits, int rate)
{
    WAVEFORMATEX wave;

    memset(&wave, 0, sizeof(wave));
    wave.wFormatTag = WAVE_FORMAT_PCM;
    wave.nChannels = 2;
    wave.nSamplesPerSec = rate;
    wave.nBlockAlign = wave.nChannels * bits / 8;
    wave.nAvgBytesPerSec = wave.nSamplesPerSec * wave.nBlockAlign;
    wave.wBitsPerSample = bits;

    IDirectSoundBuffer_SetFormat(primary, &wave);
}

sfxbuffer_t *DS_DSoundCreateBuffer(int flags, int bits, int rate)
{
    IDirectSoundBuffer8 *buf_object8;
    IDirectSound3DBuffer8 *buf_object3d = NULL;
    WAVEFORMATEX format;
    DSBUFFERDESC desc;
    sfxbuffer_t *buf;
    int     i;

    // If we don't have the listener, the primary buffer doesn't have 3D
    // capabilities; don't create 3D buffers. DSound should provide software
    // emulation, though, so this is really only a contingency.
    if(!ds_listener && flags & SFXBF_3D)
        return NULL;

    // Setup the buffer descriptor.
    memset(&desc, 0, sizeof(desc));
    desc.dwSize = sizeof(desc);
    desc.dwFlags =
        DSBCAPS_CTRLFREQUENCY | DSBCAPS_CTRLVOLUME | (flags & SFXBF_3D ?
                                                      DSBCAPS_CTRL3D :
                                                      DSBCAPS_CTRLPAN) |
        DSBCAPS_STATIC;

    // Calculate buffer size. Increase it to hit an 8 byte boundary.
    desc.dwBufferBytes = bits / 8 * rate / 2;   // 500ms buffer.
    i = desc.dwBufferBytes % 8;
    if(i)
        desc.dwBufferBytes += 8 - i;

    desc.lpwfxFormat = &format;
    if(flags & SFXBF_3D)
    {
        // Control the selection with a Property!
/*#pragma message("  DS_DSoundCreateBuffer: Reminder: Select sw-3D algo with a Property!") */
        desc.guid3DAlgorithm = DS3DALG_HRTF_LIGHT;
    }

    // And the wave data format.
    format.wFormatTag = WAVE_FORMAT_PCM;
    format.nChannels = 1;
    format.nSamplesPerSec = rate;
    format.wBitsPerSample = bits;
    format.nBlockAlign = bits / 8;
    format.nAvgBytesPerSec = rate * bits / 8;
    format.cbSize = 0;

    buf_object8 = DS_DSoundCreateBuffer8(&desc);
    if(!buf_object8)
    {
        DS_DSoundError("Failed to create buffer.");
        return NULL;
    }

    // How about a 3D interface?
    if(flags & SFXBF_3D)
    {
        buf_object3d = DS_DSoundGet3D(buf_object8);
        if(!buf_object3d)
        {
            DS_DSoundError("Failed to get 3D interface.");
            IDirectSoundBuffer_Release(buf_object8);
            return NULL;
        }
    }

    // Clear the buffer.
    buf = Z_Malloc(sizeof(*buf), PU_STATIC, 0);
    memset(buf, 0, sizeof(*buf));
    buf->ptr = buf_object8;
    buf->ptr3d = buf_object3d;
    buf->bytes = bits / 8;
    buf->rate = rate;
    buf->flags = flags;
    buf->length = desc.dwBufferBytes;
    buf->freq = rate;           // Modified by calls to Set(SFXBP_FREQUENCY).
    return buf;
}

void DS_DSoundDestroyBuffer(sfxbuffer_t * buf)
{
    IDirectSoundBuffer_Release(DSBuf(buf));
    // Free the memory allocated for the buffer.
    Z_Free(buf);
}

/*
 * Prepare the buffer for playing a sample by filling the buffer with
 * as much sample data as fits. The pointer to sample is saved, so
 * the caller mustn't free it while the sample is loaded.
 */
void DS_DSoundLoad(sfxbuffer_t * buf, struct sfxsample_s *sample)
{
    void   *data;
    DWORD   locked_bytes, wrote_bytes;

    // Try to lock the buffer.
    if(FAILED
       (hr =
        IDirectSoundBuffer_Lock(DSBuf(buf), 0, 0, &data, &locked_bytes, 0, 0,
                                DSBLOCK_ENTIREBUFFER)))
        return;                 // Couldn't lock!

    // Write as much data as we can.
    memcpy(data, sample->data, wrote_bytes =
           MIN_OF(locked_bytes, sample->size));

    // Fill the rest with zeroes.
    if(wrote_bytes < locked_bytes)
    {
        // Set the end marker since we already know it.
        buf->cursor = wrote_bytes;
        memset((char *) data + wrote_bytes, buf->bytes == 1 ? 128 : 0,
               locked_bytes - wrote_bytes);
    }
    else
    {
        // The whole buffer was filled, thus leaving the cursor at
        // the beginning.
        buf->cursor = 0;
    }

    IDirectSoundBuffer_Unlock(DSBuf(buf), data, locked_bytes, 0, 0);

    // Now the buffer is ready for playing.
    buf->sample = sample;
    buf->written = wrote_bytes;
    buf->flags &= ~SFXBF_RELOAD;

    // Zero the play cursor.
    IDirectSoundBuffer_SetCurrentPosition(DSBuf(buf), 0);
}

/*
 * Stops the buffer and makes it forget about its sample.
 */
void DS_DSoundReset(sfxbuffer_t * buf)
{
    DS_DSoundStop(buf);
    buf->sample = NULL;
    buf->flags &= ~SFXBF_RELOAD;
}

/*
 * @return: unsigned int    Length of the buffer in milliseconds.
 */
unsigned int DS_DSoundBufferLength(sfxbuffer_t * buf)
{
    return 1000 * buf->sample->numsamples / buf->freq;
}

void DS_DSoundPlay(sfxbuffer_t * buf)
{
    // Playing is quite impossible without a sample.
    if(!buf->sample)
        return;

    // Do we need to reload?
    if(buf->flags & SFXBF_RELOAD)
        DS_DSoundLoad(buf, buf->sample);

    // The sound starts playing now?
    if(!(buf->flags & SFXBF_PLAYING))
    {
        // Calculate the end time (milliseconds).
        buf->endtime = Sys_GetRealTime() + DS_DSoundBufferLength(buf);
    }

    if(FAILED(hr = IDirectSoundBuffer_Play(DSBuf(buf), 0, 0, DSBPLAY_LOOPING)))
        return;

    // The buffer is now playing.
    buf->flags |= SFXBF_PLAYING;
}

void DS_DSoundStop(sfxbuffer_t * buf)
{
    IDirectSoundBuffer_Stop(DSBuf(buf));

    // Clear the flag that tells the Sfx module about playing buffers.
    buf->flags &= ~SFXBF_PLAYING;

    // If the sound is started again, it needs to be reloaded.
    buf->flags |= SFXBF_RELOAD;
}

static boolean InRange(uint pos, uint start, uint end)
{
    if(end > start)
    {
        // This is the "normal" scenario: the write cursor is running
        // ahead of the play cursor.
        return (pos >= start && pos <= end);
    }

    // This is the "wrapping" scenario: the write cursor has wrapped
    // back to the beginning, with the play cursor left at the end
    // of the buffer. (The range is split in two.)
    return (pos >= start || pos <= end);
}

/*
 * Buffer streamer. Called by the Sfx refresh thread.
 * Copy sample data into the buffer, and if the sample has ended,
 * stop playing the buffer. If the buffer has been lost for some reason,
 * restore it. Don't do anything time-consuming...
 */
void DS_DSoundRefresh(sfxbuffer_t * buf)
{
    DWORD   play, bytes[2], dose, fill;
    void   *data[2];
    int     write_bytes, i;
    float   usedsec;
    unsigned int usedtime, nowtime = Sys_GetRealTime();

    // Can only be done if there is a sample and the buffer is playing.
    if(!buf->sample || !(buf->flags & SFXBF_PLAYING))
        return;

    // Have we passed the predicted end of sample?
    // Note: this test fails if the game has been running for about
    // 50 days, since the millisecond counter overflows. It only affects
    // sounds that are playing while the overflow happens, though.
    if(!(buf->flags & SFXBF_REPEAT) && nowtime >= buf->endtime)
    {
        // Time for the sound to stop.
        DS_DSoundStop(buf);
        return;
    }

    // Slightly redundant... (used = now - start)
    usedtime = nowtime - (buf->endtime - DS_DSoundBufferLength(buf));

    // Approximate the current playing position (-0.1 sec for safety; we don't
    // want to overwrite stuff before it gets played).
    usedsec = usedtime / 1000.0f - 0.1f;
    if(usedsec <= 0)
    {
        // This means the update is a bit early; let's wait for the
        // next one.
        return;
    }
    play = (int) (usedsec * buf->freq * buf->bytes) % buf->length;

    // Calculate how many bytes we must write (from buffer cursor up to
    // play cursor).
    if(buf->cursor < play)
        write_bytes = play - buf->cursor;
    else                        // Play has looped back to the beginning.
        write_bytes = buf->length - buf->cursor + play;

    // Try to lock the region, restoring if failed.
    for(i = 0; i < 2; i++)
    {
        if(FAILED
           (hr =
            IDirectSoundBuffer_Lock(DSBuf(buf), buf->cursor, write_bytes,
                                    &data[0], &bytes[0], &data[1], &bytes[1],
                                    0)))
        {
            if(hr == DSERR_BUFFERLOST)
            {
                IDirectSoundBuffer_Restore(DSBuf(buf));
                continue;
            }
        }
        break;
    }
    if(FAILED(hr))
    {
        return;                 // Bugrit.
    }

    //dprintf("C%i, B%i, W%i (p%i)", buf->cursor, write_bytes, buf->written, play);
    //dprintf("  (d1=%p b=%i d2=%p b=%i)", data[0], bytes[0], data[1], bytes[1]);

    // Copy in two parts: as much sample data as we've got, and then zeros.
    for(i = 0; i < 2 && data[i]; i++)
    {
        // The dose is limited to the number of bytes we can write to this
        // pointer and the number of bytes we've got left.
        dose = MIN_OF(bytes[i], buf->sample->size - buf->written);

        if(dose)
        {
            // Copy from the sample data and advance cursor & written.
            memcpy((byte *) data[i], (byte *) buf->sample->data + buf->written,
                   dose);
            buf->written += dose;
            buf->cursor += dose;
        }
        if(dose < bytes[i])
        {
            // Repeating samples just rewind the 'written' counter when the
            // end is reached.
            if(!(buf->flags & SFXBF_REPEAT))
            {
                // The whole block was not filled. Write zeros in the rest.
                fill = bytes[i] - dose;
                // Filling an 8-bit sample with zeroes produces a nasty click.
                memset((byte *) data[i] + dose, buf->bytes == 1 ? 128 : 0,
                       fill);
                buf->cursor += fill;
            }
        }
        // Wrap the cursor back to the beginning if needed. The wrap
        // can only happen after the first write, really (where the
        // buffer "breaks").
        if(buf->cursor >= buf->length)
            buf->cursor -= buf->length;
    }

    // And we're done! Unlock and get out of here.
    IDirectSoundBuffer_Unlock(DSBuf(buf), data[0], bytes[0], data[1],
                              bytes[1]);

    // If the buffer is in repeat mode, go back to the beginning once the
    // end has been reached.
    if(buf->flags & SFXBF_REPEAT && buf->written == buf->sample->size)
        buf->written = 0;
}

/*
 * Convert linear volume 0..1 to logarithmic -10000..0.
 */
int DS_DSoundLinLog(float vol)
{
    int     ds_vol;

    if(vol <= 0)
        return DSBVOLUME_MIN;
    if(vol >= 1)
        return DSBVOLUME_MAX;

    // Straighten the volume curve.
    ds_vol = 100 * 20 * log10(vol);
    if(ds_vol < DSBVOLUME_MIN)
        ds_vol = DSBVOLUME_MIN;
    return ds_vol;
}

/*
 * Convert linear pan -1..1 to logarithmic -10000..10000.
 */
int DS_DSoundLogPan(float pan)
{
    if(pan >= 1)
        return DSBPAN_RIGHT;
    if(pan <= -1)
        return DSBPAN_LEFT;
    if(pan == 0)
        return 0;
    if(pan > 0)
        return -100 * 20 * log10(1 - pan);
    return 100 * 20 * log10(1 + pan);
}

/*
 * SFXBP_VOLUME (if negative, interpreted as attenuation)
 * SFXBP_FREQUENCY
 * SFXBP_PAN (-1..1)
 * SFXBP_MIN_DISTANCE
 * SFXBP_MAX_DISTANCE
 * SFXBP_RELATIVE_MODE
 */
void DS_DSoundSet(sfxbuffer_t * buf, int property, float value)
{
    unsigned int f;

    switch (property)
    {
    case SFXBP_VOLUME:
        if(value <= 0)  // Use logarithmic attenuation.
            IDirectSoundBuffer_SetVolume(DSBuf(buf), (-1 - value) * 10000);
        else   // Linear volume.
            IDirectSoundBuffer_SetVolume(DSBuf(buf), DS_DSoundLinLog(value));
        break;

    case SFXBP_FREQUENCY:
        f = buf->rate * value;
        // Don't set redundantly.
        if(f != buf->freq)
        {
            buf->freq = f;
            IDirectSoundBuffer_SetFrequency(DSBuf(buf), f);
        }
        break;

    case SFXBP_PAN:
        IDirectSoundBuffer_SetPan(DSBuf(buf), DS_DSoundLogPan(value));
        break;

    case SFXBP_MIN_DISTANCE:
        if(!DSBuf3(buf))
            return;
        IDirectSound3DBuffer_SetMinDistance(DSBuf3(buf), value, DS3D_DEFERRED);
        break;

    case SFXBP_MAX_DISTANCE:
        if(!DSBuf3(buf))
            return;
        IDirectSound3DBuffer_SetMaxDistance(DSBuf3(buf), value, DS3D_DEFERRED);
        break;

    case SFXBP_RELATIVE_MODE:
        if(!DSBuf3(buf))
            return;
        IDirectSound3DBuffer_SetMode(DSBuf3(buf),
                                     value ? DS3DMODE_HEADRELATIVE :
                                     DS3DMODE_NORMAL, DS3D_DEFERRED);
        break;
    }
}

/*
 * SFXBP_POSITION
 * SFXBP_VELOCITY
 * Coordinates specified in world coordinate system, converted to DSound's:
 * +X to the right, +Y up and +Z away (Y and Z swapped, i.e.).
 */
void DS_DSoundSetv(sfxbuffer_t * buf, int property, float *values)
{
    if(!DSBuf3(buf))
        return;

    switch (property)
    {
    case SFXBP_POSITION:
        IDirectSound3DBuffer_SetPosition(DSBuf3(buf), values[VX], values[VZ],
                                         values[VY], DS3D_DEFERRED);
        break;

    case SFXBP_VELOCITY:
        IDirectSound3DBuffer_SetVelocity(DSBuf3(buf), values[VX], values[VZ],
                                         values[VY], DS3D_DEFERRED);
        break;
    }
}

/*
 * SFXLP_UNITS_PER_METER
 * SFXLP_DOPPLER
 * SFXLP_UPDATE
 */
void DS_DSoundListener(int property, float value)
{
    if(!ds_listener)
        return;

    switch (property)
    {
    case SFXLP_UPDATE:
        // Commit any deferred settings.
        IDirectSound3DListener_CommitDeferredSettings(ds_listener);
        DS_EAXCommitDeferred();
        break;

    case SFXLP_UNITS_PER_METER:
        IDirectSound3DListener_SetDistanceFactor(ds_listener, 1 / value,
                                                 DS3D_IMMEDIATE);
        break;

    case SFXLP_DOPPLER:
        IDirectSound3DListener_SetDopplerFactor(ds_listener, value,
                                                DS3D_IMMEDIATE);
        break;
    }
}

/*
 * Parameters are in radians.
 * Example front vectors:
 *   Yaw 0:(0,0,1), pi/2:(-1,0,0)
 */
void DS_DSoundListenerOrientation(float yaw, float pitch)
{
    float   front[3], up[3];

    front[VX] = cos(yaw) * cos(pitch);
    front[VZ] = sin(yaw) * cos(pitch);
    front[VY] = sin(pitch);

    up[VX] = -cos(yaw) * sin(pitch);
    up[VZ] = -sin(yaw) * sin(pitch);
    up[VY] = cos(pitch);

    IDirectSound3DListener_SetOrientation(ds_listener, front[VX], front[VY],
                                          front[VZ], up[VX], up[VY], up[VZ],
                                          DS3D_DEFERRED);
}

/*
 * Set the property as 'failed'. No more errors are reported for it.
 */
void DS_EAXSetFailed(DWORD prop)
{
    int     i;

    for(i = 0; i < MAX_FAILED_PROPS; i++)
        if(failed_props[i] == ~0)
        {
            failed_props[i] = prop;
            break;
        }
}

/*
 * @return: boolean         (true) if the specified property has failed.
 */
boolean DS_EAXHasFailed(DWORD prop)
{
    int     i;

    for(i = 0; i < MAX_FAILED_PROPS; i++)
        if(failed_props[i] == prop)
            return true;
    return false;
}

/*
 * @return: boolean         (true) if an EAX error should be reported.
 *                          NOTE: hr must be set.
 */
boolean DS_EAXReportError(DWORD prop)
{
    if(ignore_eax_errors)
        return false;
    if(hr != DSERR_UNSUPPORTED)
        return true;
    if(DS_EAXHasFailed(prop))
        return false;           // Don't report again.
    DS_EAXSetFailed(prop);
    return true;                // First time, do report.
}

void DS_EAXSetdw(DWORD prop, int value)
{
    if(FAILED
       (hr =
        IKsPropertySet_Set(eax_listener, &DSPROPSETID_EAX_ListenerProperties,
                           prop | DSPROPERTY_EAXLISTENER_DEFERRED, NULL, 0,
                           &value, sizeof(DWORD))))
    {
        if(DS_EAXReportError(prop))
            Con_Message("DS_EAXSetdw (prop:%i value:%i) failed. Result: %x.\n",
                        prop, value, hr);
    }
}

void DS_EAXSetf(DWORD prop, float value)
{
    if(FAILED
       (hr =
        IKsPropertySet_Set(eax_listener, &DSPROPSETID_EAX_ListenerProperties,
                           prop | DSPROPERTY_EAXLISTENER_DEFERRED, NULL, 0,
                           &value, sizeof(float))))
    {
        if(DS_EAXReportError(prop))
            Con_Message("DS_EAXSetf (prop:%i value:%f) failed. Result: %x.\n",
                        prop, value, hr);
    }
}

/*
 * Linear multiplication for a logarithmic property.
 */
void DS_EAXMuldw(DWORD prop, float mul)
{
    DWORD   retBytes;
    LONG    value;

    if(FAILED
       (hr =
        IKsPropertySet_Get(eax_listener, &DSPROPSETID_EAX_ListenerProperties,
                           prop, NULL, 0, &value, sizeof(value), &retBytes)))
    {
        if(DS_EAXReportError(prop))
            Con_Message("DS_EAXMuldw (prop:%i) get failed. Result: %x.\n",
                        prop, hr & 0xffff);
    }
    DS_EAXSetdw(prop, DS_DSoundLinLog(pow(10, value / 2000.0f) * mul));
}

/*
 * Linear multiplication for a linear property.
 */
void DS_EAXMulf(DWORD prop, float mul, float min, float max)
{
    DWORD   retBytes;
    float   value;

    if(FAILED
       (hr =
        IKsPropertySet_Get(eax_listener, &DSPROPSETID_EAX_ListenerProperties,
                           prop, NULL, 0, &value, sizeof(value), &retBytes)))
    {
        if(DS_EAXReportError(prop))
            Con_Message("DS_EAXMulf (prop:%i) get failed. Result: %x.\n", prop,
                        hr & 0xffff);
    }
    value *= mul;
    if(value < min)
        value = min;
    if(value > max)
        value = max;
    DS_EAXSetf(prop, value);
}

void DS_EAXCommitDeferred(void)
{
    if(!eax_listener)
        return;
    IKsPropertySet_Set(eax_listener, &DSPROPSETID_EAX_ListenerProperties,
                       DSPROPERTY_EAXLISTENER_COMMITDEFERREDSETTINGS, NULL, 0,
                       NULL, 0);
}

/*
 * Values use SRD_* for indices.
 */
void DS_DSoundListenerEnvironment(float *rev)
{
    float   val;

    // This can only be done if EAX is available.
    if(!eax_listener)
        return;

    val = rev[SRD_SPACE];
    if(rev[SRD_DECAY] > .5)
    {
        // This much decay needs at least the Generic environment.
        if(val < .2)
            val = .2f;
    }

    // Set the environment. Other properties are updated automatically.
    DS_EAXSetdw(DSPROPERTY_EAXLISTENER_ENVIRONMENT,
                val >= 1 ? EAX_ENVIRONMENT_PLAIN : val >=
                .8 ? EAX_ENVIRONMENT_CONCERTHALL : val >=
                .6 ? EAX_ENVIRONMENT_AUDITORIUM : val >=
                .4 ? EAX_ENVIRONMENT_CAVE : val >=
                .2 ? EAX_ENVIRONMENT_GENERIC : EAX_ENVIRONMENT_ROOM);

    // General reverb volume adjustment.
    DS_EAXSetdw(DSPROPERTY_EAXLISTENER_ROOM, DS_DSoundLinLog(rev[SRD_VOLUME]));

    // Reverb decay.
    val = (rev[SRD_DECAY] - .5) * 1.5 + 1;
    DS_EAXMulf(DSPROPERTY_EAXLISTENER_DECAYTIME, val, EAXLISTENER_MINDECAYTIME,
               EAXLISTENER_MAXDECAYTIME);

    // Damping.
    val = 1.1f * (1.2f - rev[SRD_DAMPING]);
    if(val < .1)
        val = .1f;
    DS_EAXMuldw(DSPROPERTY_EAXLISTENER_ROOMHF, val);

    // A slightly increased roll-off.
    DS_EAXSetf(DSPROPERTY_EAXLISTENER_ROOMROLLOFFFACTOR, 1.3f);
}

/*
 * Call SFXLP_UPDATE at the end of every channel update.
 */
void DS_DSoundListenerv(int property, float *values)
{
    switch (property)
    {
    case SFXLP_PRIMARY_FORMAT:
        if(!ArgExists("-nopsf"))    // Can we set the Primary Sound Format?
            DS_DSoundSetPrimaryFormat(values[0], values[1]);
        break;

    case SFXLP_POSITION:
        if(!ds_listener)
            return;
        IDirectSound3DListener_SetPosition(ds_listener, values[VX], values[VZ],
                                           values[VY], DS3D_DEFERRED);
        break;

    case SFXLP_VELOCITY:
        if(!ds_listener)
            return;
        IDirectSound3DListener_SetVelocity(ds_listener, values[VX], values[VZ],
                                           values[VY], DS3D_DEFERRED);
        break;

    case SFXLP_ORIENTATION:
        if(!ds_listener)
            return;
        DS_DSoundListenerOrientation(values[VX] / 180 * PI,
                                     values[VY] / 180 * PI);
        break;

    case SFXLP_REVERB:
        if(!ds_listener)
            return;
        DS_DSoundListenerEnvironment(values);
        break;

    default:
        DS_DSoundListener(property, 0);
    }
}
