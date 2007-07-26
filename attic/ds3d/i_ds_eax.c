/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2006 Jaakko Keränen <skyjake@dengine.net>
 *\author Copyright © 2006-2007 Daniel Swanson <danij@dengine.net>
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

/*
 * i_ds_eax.c: Playing sfx using DirectSound(3D) and EAX (if available).
 */

// HEADER FILES ------------------------------------------------------------

#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <mmsystem.h>
#include <dsound.h>
#include <eax.h>
#include <math.h>
#include "dd_def.h"
#include "i_win32.h"
#include "i_timer.h"
#include "i_sound.h"
#include "settings.h"

// MACROS -------------------------------------------------------------------

#define MAX_SND_DIST        2025

/*#define SBUFPTR(handle)       (sources+handle-1)
#define SBUFHANDLE(sbptr)   ((sbptr-sources)+1)*/

#define NEEDED_SUPPORT      ( KSPROPERTY_SUPPORT_GET | KSPROPERTY_SUPPORT_SET )

/*#define MAX_PLAYSTACK     16
#define MAX_SOUNDS_PER_FRAME 3*/

/*#ifndef VZ
enum { VX, VY, VZ };
#endif*/

// TYPES --------------------------------------------------------------------

typedef struct
{
    int                     id;
    LPDIRECTSOUNDBUFFER     source;
    LPDIRECTSOUND3DBUFFER   source3D;
    int                     freq;   // The original frequence of the sample.
    int                     startTime;
} sndsource_t;

typedef struct
{
    unsigned short  bitsShift;
    unsigned short  frequency;
    unsigned short  length;
    unsigned short  reserved;
} sampleheader_t;

// PRIVATE FUNCTIONS --------------------------------------------------------

void I2_DestroyAllSources();
int I2_PlaySound(void *data, boolean play3d, sound3d_t *desc, int pan);
int createDSBuffer(DWORD flags, int samples, int freq, int bits, int channels,
                   LPDIRECTSOUNDBUFFER *bufAddr);

// EXTERNAL DATA ------------------------------------------------------------

extern HWND hWndMain;

// PUBLIC DATA --------------------------------------------------------------

// PRIVATE DATA -------------------------------------------------------------

static int                      idGen = 0;
static boolean                  initOk = false;
static LPDIRECTSOUND            dsound = NULL;
static LPDIRECTSOUND3DLISTENER  dsListener = NULL;
static LPKSPROPERTYSET          eaxListener = NULL;
static HRESULT                  hr;
static DSCAPS                   dsCaps;

static sndsource_t              *sndSources = 0;
static int                      numSndSources = 0;

/*
static playstack_t              playstack[MAX_PLAYSTACK];
static int                      playstackpos = 0;
static int                      soundsPerTick = 0;
*/

static float                    listenerYaw = 0, listenerPitch = 0;

// CODE ---------------------------------------------------------------------

int I2_Init()
{
    DSBUFFERDESC        desc;
    LPDIRECTSOUNDBUFFER bufTemp;

    if(initOk) return true; // Don't init a second time.
    if(FAILED(hr = EAXDirectSoundCreate(NULL, &dsound, NULL)))
    {
        // EAX can't be initialized. Use normal DS, then.
        ST_Message("I2_Init: EAX 2 couldn't be initialized (result: %i).\n", hr & 0xffff);
        if(FAILED(hr = DirectSoundCreate(NULL, &dsound, NULL)))
        {
            ST_Message("I2_Init: Couldn't create dsound (result: %i).\n", hr & 0xffff);
            return false;
        }
    }
    // Set the cooperative level.
    if(FAILED(hr = IDirectSound_SetCooperativeLevel(dsound, hWndMain, DSSCL_PRIORITY)))
    {
        ST_Message("I2_Init: Couldn't set dSound cooperative level (result: %i).\n", hr & 0xffff);
        return false;
    }
    // Get the listener.
    memset(&desc, 0, sizeof(desc));
    desc.dwSize = sizeof(desc);
    desc.dwFlags = DSBCAPS_CTRL3D | DSBCAPS_PRIMARYBUFFER;
    if(SUCCEEDED(IDirectSound_CreateSoundBuffer(dsound, &desc, &bufTemp, NULL)))
    {
        // Query the listener interface.
        IDirectSoundBuffer_QueryInterface(bufTemp, &IID_IDirectSound3DListener, &dsListener);
    }
    // Release the primary buffer interface, we won't need it.
    IDirectSoundBuffer_Release(bufTemp);

    // Try to get the EAX listener property set. Create a temporary secondary buffer for it.
    if(SUCCEEDED( createDSBuffer(DSBCAPS_STATIC | DSBCAPS_CTRL3D, DSBSIZE_MIN, 22050, 8, 1, &bufTemp) ))
    {
        // Now try to get the property set.
        if(SUCCEEDED(hr = IDirectSoundBuffer_QueryInterface(bufTemp,
            &IID_IKsPropertySet, &eaxListener)))
        {
            DWORD support = 0, revsize = 0;
            // Check for support.
            if(FAILED(hr = IKsPropertySet_QuerySupport(eaxListener,
                &DSPROPSETID_EAX_ListenerProperties,
                DSPROPERTY_EAXLISTENER_ENVIRONMENT,
                &support))
                || ((support & NEEDED_SUPPORT) != NEEDED_SUPPORT))
            {
                ST_Message("I2_Init: Property set acquired, but EAX 2 not supported.\n  Result:%i, support:%x\n", hr&0xffff, support);
                IKsPropertySet_Release(eaxListener);
                eaxListener = NULL;
            }
            else
            {
                // EAX is supported!
                ST_Message("I2_Init: EAX 2 available.\n");
            }
        }
        // Release the temporary buffer interface.
        IDirectSoundBuffer_Release(bufTemp);
    }

    // Get the caps.
    dsCaps.dwSize = sizeof(dsCaps);
    IDirectSound_GetCaps(dsound, &dsCaps);
    ST_Message("I2_Init: Number of hardware 3D buffers: %i\n", dsCaps.dwMaxHw3DAllBuffers);

    // Configure the DS3D listener.
    if(dsListener)
    {
        IDirectSound3DListener_SetDistanceFactor(dsListener, 1/36.0f, DS3D_DEFERRED);
        IDirectSound3DListener_SetDopplerFactor(dsListener, 2, DS3D_DEFERRED);
    }

    // Success!
    initOk = true;
    return true;
}

void I2_Shutdown()
{
    if(!initOk) return;

    I2_DestroyAllSources();

    // Release the DS objects.
    if(eaxListener) IKsPropertySet_Release(eaxListener);
    if(dsListener) IDirectSound3DListener_Release(dsListener);
    if(dsound) IDirectSound_Release(dsound);
    eaxListener = NULL;
    dsListener = NULL;
    dsound = NULL;
}

static sndsource_t *GetSource(int handle)
{
    int i;

    for(i=0; i<numSndSources; i++)
        if(sndSources[i].id == handle)
            return sndSources + i;
    return NULL;
}

// Converts the linear 0 - 1 to the logarithmic -10000 - 0.
static int volLinearToLog(float zero_to_one)
{
    int val;
    if(zero_to_one <= 0) return -10000;
    if(zero_to_one >= 1) return 0;
    val = 2000 * log10(zero_to_one);
    if(val < -10000) val = -10000;
    return val;
}

void EAX_dwSet(DWORD prop, int value)
{
    if(FAILED(hr = IKsPropertySet_Set(eaxListener,
        &DSPROPSETID_EAX_ListenerProperties,
        prop | DSPROPERTY_EAXLISTENER_DEFERRED, NULL, 0, &value, sizeof(int))))
    {
        I_error("EAX_dwSet (prop:%i value:%i) failed. Result: %i.\n",
            prop, value, hr&0xffff);
    }
}

void EAX_fSet(DWORD prop, float value)
{
    if(FAILED(hr = IKsPropertySet_Set(eaxListener,
        &DSPROPSETID_EAX_ListenerProperties,
        prop | DSPROPERTY_EAXLISTENER_DEFERRED, NULL, 0, &value, sizeof(float))))
    {
        I_error("EAX_fSet (prop:%i value:%f) failed. Result: %i.\n",
            prop, value, hr&0xffff);
    }
}

void EAX_dwMul(DWORD prop, float mul)
{
    DWORD   retBytes;
    LONG    value;

    if(FAILED(hr = IKsPropertySet_Get(eaxListener, &DSPROPSETID_EAX_ListenerProperties,
        prop, NULL, 0, &value, sizeof(value), &retBytes)))
    {
        I_error("EAX_fMul (prop:%i) get failed. Result: %i.\n", prop, hr & 0xffff);
    }
    EAX_dwSet(prop, volLinearToLog(pow(10, value/2000.0f) * mul));
}

void EAX_fMul(DWORD prop, float mul, float min, float max)
{
    DWORD retBytes;
    float value;

    if(FAILED(hr = IKsPropertySet_Get(eaxListener, &DSPROPSETID_EAX_ListenerProperties,
        prop, NULL, 0, &value, sizeof(value), &retBytes)))
    {
        I_error("EAX_fMul (prop:%i) get failed. Result: %i.\n", prop, hr & 0xffff);
    }
    value *= mul;
    if(value < min) value = min;
    if(value > max) value = max;
    EAX_fSet(prop, value);
}

void EAX_CommitDeferred()
{
    if(!eaxListener) return;
    if(FAILED(IKsPropertySet_Set(eaxListener, &DSPROPSETID_EAX_ListenerProperties,
        DSPROPERTY_EAXLISTENER_COMMITDEFERREDSETTINGS, NULL, 0,
        NULL, 0)))
    {
        I_error("EAX_CommitDeferred failed.\n");
    }
}

void I2_BeginSoundFrame()
{
/*  int     i;

    if(!initOk) return;

    soundsPerFrame = 0;
    // Play sounds on the stack.
    for(i=0; i<MAX_SOUNDS_PER_FRAME && playstackpos > 0; i++, soundsPerFrame++)
    {
        playstack_t *pst = playstack + playstackpos-1;
        I2_PlaySound(pst->data, pst->play3d, &pst->desc, pst->pan);
    }*/
}

void I2_EndSoundFrame()
{
    if(!initOk || !dsListener) return;
    IDirectSound3DListener_CommitDeferredSettings(dsListener);
}

void I2_KillSource(sndsource_t *src)
{
    if(src->source3D) IDirectSound3DBuffer_Release(src->source3D);
    if(src->source) IDirectSoundBuffer_Release(src->source);
    memset(src, 0, sizeof(*src));
}

int I_Play2DSound(void *data, int volume, int pan, int pitch)
{
    sound3d_t desc;
    int handle;

    desc.flags = DDSOUNDF_VOLUME | DDSOUNDF_PITCH;
    desc.volume = volume;
    desc.pitch = pitch;
    handle = I2_PlaySound(data, false, &desc, pan);
    return handle;
}

int I_Play3DSound(void *data, sound3d_t *desc)
{
    return I2_PlaySound(data, true, desc, 0);
}

void I_StopSound(int handle)
{
    sndsource_t *buf = GetSource(handle);

    if(!initOk || !buf) return;
    if(buf->source)
    {
        IDirectSoundBuffer_Stop(buf->source);
        IDirectSoundBuffer_SetCurrentPosition(buf->source, 0);
    }
}

void I_UpdateListener(listener3d_t *desc)
{
    float   temp[3], val;
    int     i;

    if(!initOk || !dsListener || !desc) return;
    if(desc->flags & DDLISTENERF_POS)
    {
        for(i=0; i<3; i++) temp[i] = FIX2FLT(desc->pos[i]);
        IDirectSound3DListener_SetPosition(dsListener, temp[VX], temp[VY], temp[VZ], DS3D_DEFERRED);
    }
    if(desc->flags & DDLISTENERF_MOV)
    {
        for(i=0; i<3; i++) temp[i] = FIX2FLT(desc->mov[i]);
        IDirectSound3DListener_SetVelocity(dsListener, temp[VX], temp[VY], temp[VZ], DS3D_DEFERRED);
    }
    if(desc->flags & DDLISTENERF_YAW || desc->flags & DDLISTENERF_PITCH)
    {
        float y, p;
        float top[3];   // Top vectors.
        if(desc->flags & DDLISTENERF_YAW) listenerYaw = desc->yaw;
        if(desc->flags & DDLISTENERF_PITCH) listenerPitch = desc->pitch;
        // The angles in radians.
        y = listenerYaw / 180 * PI;
        p = (listenerPitch-90) / 180 * PI;
        // Calculate the front vector.
        temp[VX] = sin(y) * cos(p);
        temp[VZ] = cos(y) * cos(p);
        temp[VY] = sin(p);
        /*temp[VX] = sin(y);
        temp[VZ] = cos(y);
        temp[VY] = 0;*/
        // And the top vector.
        top[VX] = -sin(y) * sin(p);
        top[VZ] = -cos(y) * sin(p);
        top[VY] = cos(p);
        /*top[VX] = 0;
        top[VY] = 1;
        top[VZ] = 0;*/
        IDirectSound3DListener_SetOrientation(dsListener,
            temp[VX], temp[VY], temp[VZ],   // Front vector.
            top[VX], top[VY], top[VZ], DS3D_DEFERRED);
    }
    if(desc->flags & DDLISTENERF_SET_REVERB && eaxListener)
    {
        val = desc->reverb.space;
        if(desc->reverb.decay > .5)
        {
            // This much decay needs at least the Generic environment.
            if(val < .2) val = .2f;
        }
        EAX_dwSet(DSPROPERTY_EAXLISTENER_ENVIRONMENT,
            val >= 1? EAX_ENVIRONMENT_PLAIN
            : val >= .8? EAX_ENVIRONMENT_CONCERTHALL
            : val >= .6? EAX_ENVIRONMENT_AUDITORIUM
            : val >= .4? EAX_ENVIRONMENT_CAVE
            : val >= .2? EAX_ENVIRONMENT_GENERIC
            : EAX_ENVIRONMENT_ROOM);

        EAX_dwSet(DSPROPERTY_EAXLISTENER_ROOM, volLinearToLog(desc->reverb.volume));

        val = (desc->reverb.decay - .5)*1.5 + 1;
        EAX_fMul(DSPROPERTY_EAXLISTENER_DECAYTIME, val, EAXLISTENER_MINDECAYTIME,
            EAXLISTENER_MAXDECAYTIME);

        val = 1.1 * (1.2 - desc->reverb.damping);
        if(val < .1) val = .1f;
        EAX_dwMul(DSPROPERTY_EAXLISTENER_ROOMHF, val);

        EAX_fSet(DSPROPERTY_EAXLISTENER_ROOMROLLOFFFACTOR, 1.3f);

        EAX_CommitDeferred();
    }
    if(desc->flags & DDLISTENERF_DISABLE_REVERB && eaxListener)
    {
        // Turn off all reverb by setting the room value to -100 dB.
        EAX_dwSet(DSPROPERTY_EAXLISTENER_ROOM, EAXLISTENER_MINROOM);
        EAX_CommitDeferred();
    }
}

int I2_IsSourcePlaying(sndsource_t *buf)
{
    DWORD status;
    IDirectSoundBuffer_GetStatus(buf->source, &status);
    // Restore the buffer if it's lost, but that shouldn't really matter since
    // it'll be released when it stops playing.
    if(status & DSBSTATUS_BUFFERLOST) IDirectSoundBuffer_Restore(buf->source);
    return (status & DSBSTATUS_PLAYING) != 0;
}

int I_SoundIsPlaying(int handle)
{
    sndsource_t *src = GetSource(handle);

    if(!initOk || !src) return false;
    if(src->source == NULL) return false;
    return I2_IsSourcePlaying(src);
}

// Returns a free buffer.
sndsource_t *I2_GetFreeSource(boolean play3d)
{
    int             i;
    unsigned int    num3D = 0;
    sndsource_t     *suitable = NULL, *oldest = NULL;

    if(numSndSources) oldest = sndSources;
    // We are not going to have software 3D sounds, of all things.
    // Release stopped sources and count the number of playing 3D sources.
    for(i=0; i<numSndSources; i++)
    {
        if(!sndSources[i].source)
        {
            if(!suitable) suitable = sndSources + i;
            continue;
        }
        // See if this is the oldest buffer.
        if(sndSources[i].startTime < oldest->startTime)
        {
            // This buffer will be stopped if need be.
            oldest = sndSources + i;
        }
        if(I2_IsSourcePlaying(sndSources + i))
        {
            if(sndSources[i].source3D) num3D++;
        }
        else
        {
            I2_KillSource(sndSources + i); // All stopped sources will be killed on sight.
            // This buffer is not playing.
            if(!suitable) suitable = sndSources + i;
        }
    }
    if(play3d && num3D >= dsCaps.dwMaxHw3DAllBuffers && oldest)
    {
        // There are as many 3D sources as there can be.
        // Stop the oldest sound.
        I2_KillSource(oldest);
        return oldest;
    }
    if(suitable) return suitable;

    // Ah well. We need to allocate a new one.
    sndSources = realloc(sndSources, sizeof(sndsource_t) * ++numSndSources);
    // Clear it.
    memset(sndSources + numSndSources-1, 0, sizeof(sndsource_t));
    // Return a pointer to it.
    return sndSources + numSndSources-1;
}

// Vol is linear, from 0 to 1.
void I2_SetVolume(sndsource_t *src, float vol)
{
    extern int snd_SfxVolume;
    int ds_vol;

    vol *= snd_SfxVolume / 255.0f;
    if(vol <= 0)
        ds_vol = DSBVOLUME_MIN;
    else if(vol >= 1)
        ds_vol = DSBVOLUME_MAX;
    else    // Straighten the volume curve.
    {
        ds_vol = 100 * 20 * log10(vol);
        if(ds_vol < DSBVOLUME_MIN) ds_vol = DSBVOLUME_MIN;
    }
    IDirectSoundBuffer_SetVolume(src->source, ds_vol);
}

void I2_SetPitch(sndsource_t *src, float pitch)
{
    int newfreq = src->freq * pitch;
    if(newfreq < DSBFREQUENCY_MIN) newfreq = DSBFREQUENCY_MIN;
    if(newfreq > DSBFREQUENCY_MAX) newfreq = DSBFREQUENCY_MAX;
    IDirectSoundBuffer_SetFrequency(src->source, newfreq);
}

// Pan is linear, from 0 to 1. 0.5 is in the center.
void I2_SetPan(sndsource_t *src, float pan)
{
    int ds_pan = 0;

    if(pan < 0) pan = 0;
    if(pan > 1) pan = 1;

    pan *= 2;
    pan -= 1;
    if(pan >= 1)
        ds_pan = DSBPAN_RIGHT;
    else if(pan <= -1)
        ds_pan = DSBPAN_LEFT;
    else if(pan == 0)
        ds_pan = 0;
    else if(pan > 0)
        ds_pan = -100 * 20 * log10(1-pan);// - log10(1-pan));
    else
        ds_pan = 100 * 20 * log10(1+pan);
    IDirectSoundBuffer_SetPan(src->source, ds_pan);
}

void I2_UpdateSource(sndsource_t *buf, sound3d_t *desc)
{
    int     i;
    float   temp[3];

    if(desc->flags & DDSOUNDF_VOLUME)
    {
        I2_SetVolume(buf, desc->volume/1000.0f);
    }
    if(desc->flags & DDSOUNDF_PITCH)
    {
        I2_SetPitch(buf, desc->pitch/1000.0f);
    }
    if(desc->flags & DDSOUNDF_POS)
    {
        for(i=0; i<3; i++) temp[i] = FIX2FLT(desc->pos[i]);
        IDirectSound3DBuffer_SetPosition(buf->source3D, temp[VX], temp[VY], temp[VZ], DS3D_DEFERRED);
    }
    if(desc->flags & DDSOUNDF_MOV)
    {
        for(i=0; i<3; i++) temp[i] = FIX2FLT(desc->mov[i]);
        IDirectSound3DBuffer_SetVelocity(buf->source3D, temp[VX], temp[VY], temp[VZ], DS3D_DEFERRED);
    }
}

int createDSBuffer(DWORD flags, int samples, int freq, int bits, int channels,
                   LPDIRECTSOUNDBUFFER *bufAddr)
{
    DSBUFFERDESC    bufd;
    WAVEFORMATEX    form;
    DWORD           dataBytes = samples * bits/8 * channels;

    // Prepare the buffer description.
    memset(&bufd, 0, sizeof(bufd));
    bufd.dwSize = sizeof(bufd);
    bufd.dwFlags = flags;
    bufd.dwBufferBytes = dataBytes;
    bufd.lpwfxFormat = &form;

    // Prepare the format description.
    memset(&form, 0, sizeof(form));
    form.wFormatTag = WAVE_FORMAT_PCM;
    form.nChannels = channels;
    form.nSamplesPerSec = freq;
    form.nBlockAlign = channels * bits/8;
    form.nAvgBytesPerSec = form.nSamplesPerSec * form.nBlockAlign;
    form.wBitsPerSample = bits;

    return IDirectSound_CreateSoundBuffer(dsound, &bufd, bufAddr, NULL);
}

int I2_PlaySound(void *data, boolean play3d, sound3d_t *desc, int pan)
{
    sampleheader_t  *header = data;
    byte            *sample = (byte*) data + sizeof(sampleheader_t);
    sndsource_t     *sndbuf;
    void            *writePtr1=NULL, *writePtr2=NULL;
    DWORD           writeBytes1, writeBytes2;
    int             samplelen = header->length, freq = header->frequency, bits = 8;

    // Can we play sounds?
    if(!initOk || data == NULL) return 0;   // Sorry...

    if(snd_Resample != 1 && snd_Resample != 2 && snd_Resample != 4)
    {
        ST_Message("I2_PlaySound: invalid resample factor.\n");
        snd_Resample = 1;
    }

    // Get a buffer that's doing nothing.
    sndbuf = I2_GetFreeSource(play3d);
    sndbuf->startTime = I_GetTime();
    // Prepare the audio data.
    if(snd_Resample == 1 && !snd_16bits) // Play sounds as normal?
    {
        // No resampling necessary.
        sndbuf->freq = header->frequency;
    }
    else
    {
        // Resample the sound.
        sample = I_Resample8bitSound(sample, header->length, freq,
            snd_Resample, snd_16bits, &samplelen);
        if(snd_16bits) bits = 16;
        freq *= snd_Resample;
        sndbuf->freq = freq;
    }
    if(sndbuf->source)
    {
        // Release the old source.
        I2_KillSource(sndbuf);
    }
    // Create a new source.
    if(FAILED(hr = createDSBuffer(play3d? DSBCAPS_CTRLVOLUME | DSBCAPS_CTRLFREQUENCY
        | DSBCAPS_CTRL3D | DSBCAPS_MUTE3DATMAXDISTANCE | DSBCAPS_STATIC
        : DSBCAPS_CTRLPAN | DSBCAPS_CTRLVOLUME | DSBCAPS_CTRLFREQUENCY
        | DSBCAPS_STATIC,
        header->length * snd_Resample, freq, bits, 1, &sndbuf->source)))
    {
        ST_Message("I2_PlaySound: couldn't create a new buffer (result = %d).\n", hr & 0xffff);
        memset(sndbuf, 0, sizeof(*sndbuf));
        return 0;
    }
    if(play3d)
    {
        // Query the 3D interface.
        if(FAILED(hr = IDirectSoundBuffer_QueryInterface(sndbuf->source, &IID_IDirectSound3DBuffer,
            &sndbuf->source3D)))
        {
            ST_Message("I2_PlaySound: couldn't get 3D buffer interface (result = %d).\n", hr & 0xffff);
            IDirectSoundBuffer_Release(sndbuf->source);
            memset(sndbuf, 0, sizeof(*sndbuf));
            return 0;
        }
    }

    // Lock the buffer.
    if(FAILED(hr = IDirectSoundBuffer_Lock(sndbuf->source, 0, samplelen,
        &writePtr1, &writeBytes1, &writePtr2, &writeBytes2, 0)))
    {
        I_error("I2_PlaySound: couldn't lock source (result = %d).\n", hr & 0xffff);
    }

    // Copy the data over.
    memcpy(writePtr1, sample, writeBytes1);
    if(writePtr2) memcpy(writePtr2, (char*) sample + writeBytes1, writeBytes2);

    // Unlock the buffer.
    if(FAILED(hr = IDirectSoundBuffer_Unlock(sndbuf->source, writePtr1, writeBytes1,
        writePtr2, writeBytes2)))
    {
        I_error("I2_PlaySound: couldn't unlock source (result = %d).\n", hr & 0xffff);
        return 0;
    }

    if(play3d)
    {
        // Set the 3D parameters of the source.
        if(desc->flags & DDSOUNDF_VERY_LOUD)
        {
            // You can hear this from very far away (e.g. thunderclap).
            IDirectSound3DBuffer_SetMinDistance(sndbuf->source3D, 10000, DS3D_DEFERRED);
            IDirectSound3DBuffer_SetMaxDistance(sndbuf->source3D, 20000, DS3D_DEFERRED);
        }
        else
        {
            IDirectSound3DBuffer_SetMinDistance(sndbuf->source3D, 100, DS3D_DEFERRED);
            IDirectSound3DBuffer_SetMaxDistance(sndbuf->source3D, MAX_SND_DIST, DS3D_DEFERRED);
        }
        if(desc->flags & DDSOUNDF_LOCAL)
            IDirectSound3DBuffer_SetMode(sndbuf->source3D, DS3DMODE_DISABLE, DS3D_DEFERRED);
    }
    else
    {
        // If playing in 2D mode, set the pan.
        I2_SetPan(sndbuf, pan/1000.0f);
    }
    I2_UpdateSource(sndbuf, desc);
    // Start playing the buffer.
    if(FAILED(hr = IDirectSoundBuffer_Play(sndbuf->source, 0, 0, 0)))
    {
        I_error("I2_PlaySound: couldn't start source (result = %d).\n", hr & 0xffff);
        return 0;
    }
    // Return the handle to the sound source.
    return (sndbuf->id = idGen++);
}

void I2_DestroyAllSources()
{
    int     i;

    for(i=0; i<numSndSources; i++) I2_KillSource(sndSources + i);
    free(sndSources);
    sndSources = NULL;
    numSndSources = 0;
}

void I_Update2DSound(int handle, int volume, int pan, int pitch)
{
    sndsource_t *buf = GetSource(handle);

    if(!initOk || !buf) return;
    if(buf->source == NULL) return;
    if(buf->source3D) return;
    if(!I2_IsSourcePlaying(buf)) IDirectSoundBuffer_Play(buf->source, 0, 0, 0);
    I2_SetVolume(buf, volume/1000.0f);
    I2_SetPan(buf, pan/1000.0f);
    I2_SetPitch(buf, pitch/1000.0f);
}

void I_Update3DSound(int handle, sound3d_t *desc)
{
    sndsource_t *buf = GetSource(handle);

    if(!initOk || !buf) return;
    if(buf->source == NULL) return;
    if(buf->source3D == NULL) return;
    if(!I2_IsSourcePlaying(buf)) IDirectSoundBuffer_Play(buf->source, 0, 0, 0);
    I2_UpdateSource(buf, desc);
}
