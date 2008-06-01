/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2008 Jaakko Keränen <jaakko.keranen@iki.fi>
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

/**
 * driver_sndcmp.cpp: Maximum Compatibility Sfx Driver.
 *
 * Uses DirectSound 6.0
 * Buffers created on Load.
 */

// HEADER FILES ------------------------------------------------------------

#define DIRECTSOUND_VERSION 0x0600

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
#include <dsound.h>
#include <eax.h>
#include <math.h>
#include <stdlib.h>

#pragma warning (disable: 4035 4244)

#include "doomsday.h"
#include "sys_sfxd.h"

// MACROS ------------------------------------------------------------------

// DirectSound(3D)Buffer Pointer
#define DSBuf(buf)      ((LPDIRECTSOUNDBUFFER) buf->ptr)
#define DSBuf3(buf)     ((LPDIRECTSOUND3DBUFFER) buf->ptr3d)

#define NEEDED_SUPPORT  ( KSPROPERTY_SUPPORT_GET | KSPROPERTY_SUPPORT_SET )

#define PI              3.14159265

// TYPES -------------------------------------------------------------------

enum { VX, VY, VZ };

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

extern "C" {

int             DS_Init(void);
void            DS_Shutdown(void);
sfxbuffer_t*    DS_CreateBuffer(int flags, int bits, int rate);
void            DS_DestroyBuffer(sfxbuffer_t *buf);
void            DS_Load(sfxbuffer_t *buf, struct sfxsample_s *sample);
void            DS_Reset(sfxbuffer_t *buf);
void            DS_Play(sfxbuffer_t *buf);
void            DS_Stop(sfxbuffer_t *buf);
void            DS_Refresh(sfxbuffer_t *buf);
void            DS_Event(int type);
void            DS_Set(sfxbuffer_t *buf, int prop, float value);
void            DS_Setv(sfxbuffer_t *buf, int prop, float *values);
void            DS_Listener(int prop, float value);
void            DS_Listenerv(int prop, float *values);

}

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

bool                    initOk = false;
int                     verbose;
HRESULT                 hr;
LPDIRECTSOUND           dsound;
LPDIRECTSOUNDBUFFER     primary;
LPDIRECTSOUND3DLISTENER dsListener;
LPKSPROPERTYSET         eaxListener;
DSCAPS                  dsCaps;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

static void error(const char *where, const char *msg)
{
    Con_Message("%s(Compat): %s [Result = 0x%x]\n", where, msg, hr);
}

static int createDSBuffer(DWORD flags, int samples, int freq, int bits,
                          int channels, LPDIRECTSOUNDBUFFER *bufAddr)
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

    return dsound->CreateSoundBuffer(&bufd, bufAddr, NULL);
}

static void freeDSBuffers(sfxbuffer_t *buf)
{
    if(DSBuf3(buf))
        DSBuf3(buf)->Release();
    if(DSBuf(buf))
        DSBuf(buf)->Release();
    buf->ptr = buf->ptr3d = NULL;
}

int DS_Init(void)
{
    HWND                hWnd;
    DSBUFFERDESC        desc;
    LPDIRECTSOUNDBUFFER bufTemp;

    if(initOk)
        return true; // Already initialized.

    // Are we in verbose mode?
    if((verbose = ArgExists("-verbose")))
        Con_Message("DS_Init(Compat): Initializing sound driver...\n");

    // Get Doomsday's window handle.
    hWnd = (HWND) DD_GetVariable(DD_WINDOW_HANDLE);

    hr = DS_OK;
    if(ArgExists("-noeax") ||
       FAILED(hr = EAXDirectSoundCreate(NULL, &dsound, NULL)))
    {   // EAX can't be initialized. Use normal DS, then.
        if(FAILED(hr))
            error("DS_Init", "EAX 2 couldn't be initialized.");

        if(FAILED(hr = DirectSoundCreate(NULL, &dsound, NULL)))
        {
            error("DS_Init", "Failed to create dsound interface.");
            return false;
        }
    }

    // Set the cooperative level.
    if(FAILED(hr = dsound->SetCooperativeLevel(hWnd, DSSCL_PRIORITY)))
    {
        error("DS_Init", "Couldn't set dSound coop level.");
        return false;
    }

    // Get the primary buffer and the listener.
    primary = NULL;
    memset(&desc, 0, sizeof(desc));
    desc.dwSize = sizeof(desc);
    desc.dwFlags = DSBCAPS_CTRL3D | DSBCAPS_PRIMARYBUFFER;
    dsListener = NULL;
    if(SUCCEEDED(dsound->CreateSoundBuffer(&desc, &primary, NULL)))
    {
        // Query the listener interface.
        primary->QueryInterface(IID_IDirectSound3DListener,
            (void**) &dsListener);
    }
    else
    {   // Failure; get a 2D primary buffer, then.
        desc.dwFlags = DSBCAPS_PRIMARYBUFFER;
        dsound->CreateSoundBuffer(&desc, &primary, NULL);
    }

    // Start playing the primary buffer.
    if(primary)
    {
        if(FAILED(hr = primary->Play(0, 0, DSBPLAY_LOOPING)))
            error("DS_Init", "Can't play primary buffer.");
    }

    // Try to get the EAX listener property set.
    // Create a temporary secondary buffer for it.
    eaxListener = NULL;
    if(SUCCEEDED(createDSBuffer(DSBCAPS_STATIC | DSBCAPS_CTRL3D,
                                DSBSIZE_MIN, 22050, 8, 1, &bufTemp)))
    {
        // Now try to get the property set.
        if(SUCCEEDED(hr = bufTemp->QueryInterface(IID_IKsPropertySet,
                                                  (void**) &eaxListener)))
        {
            DWORD support = 0, revsize = 0;

            // Check for support.
            if(FAILED(hr = eaxListener->QuerySupport(
                DSPROPSETID_EAX_ListenerProperties,
                DSPROPERTY_EAXLISTENER_ENVIRONMENT,
                &support)) ||
               ((support & NEEDED_SUPPORT) != NEEDED_SUPPORT))
            {
                error("DS_Init", "Sufficient EAX2 support not present.");
                eaxListener->Release();
                eaxListener = NULL;
            }
            else
            {   // EAX is supported!
                if(verbose)
                    Con_Message("DS_Init(Compat): EAX2 is available.\n");
            }
        }

        // Release the temporary buffer interface.
        bufTemp->Release();
    }

    // Get the caps.
    dsCaps.dwSize = sizeof(dsCaps);
    dsound->GetCaps(&dsCaps);
    if(verbose)
        Con_Message("DS_Init(Compat): Number of hardware 3D buffers: %i\n",
                    dsCaps.dwMaxHw3DAllBuffers);

    // Configure the DS3D listener.
    if(dsListener)
    {
        dsListener->SetDistanceFactor(1/36.0f, DS3D_DEFERRED);
        dsListener->SetDopplerFactor(2, DS3D_DEFERRED);
    }

    // Success!
    initOk = true;
    return true;
}

void DS_Shutdown(void)
{
    if(!initOk)
        return;

    if(eaxListener)
        eaxListener->Release();
    if(dsListener)
        dsListener->Release();
    if(primary)
        primary->Release();
    if(dsound)
        dsound->Release();

    eaxListener = NULL;
    dsListener = NULL;
    primary = NULL;
    dsound = NULL;

    initOk = false;
}

sfxbuffer_t* DS_CreateBuffer(int flags, int bits, int rate)
{
    sfxbuffer_t *buf;

    // Since we don't know how long the buffer must be, we'll just create
    // an sfxbuffer_t with no DSound buffer attached. The DSound buffer
    // will be created when a sample is loaded.

    buf = (sfxbuffer_t*) Z_Calloc(sizeof(*buf), PU_STATIC, 0);
    buf->bytes = bits/8;
    buf->rate = rate;
    buf->flags = flags;
    buf->freq = rate; // Modified by calls to Set(SFXBP_FREQUENCY).

    return buf;
}

void DS_DestroyBuffer(sfxbuffer_t *buf)
{
    freeDSBuffers(buf);
    Z_Free(buf);
}

void DS_Load(sfxbuffer_t *buf, struct sfxsample_s *sample)
{
#define SAMPLE_SILENCE  16 // Samples to interpolate to silence.
#define SAMPLE_ROUNDOFF 32 // The length is a multiple of this.

    LPDIRECTSOUNDBUFFER newSound = NULL;
    LPDIRECTSOUND3DBUFFER newSound3D = NULL;
    bool        play3d = !(buf->flags & SFXBF_3D);
    void       *writePtr1 = NULL, *writePtr2 = NULL;
    DWORD       writeBytes1, writeBytes2;
    unsigned int safeNumSamples = sample->numsamples + SAMPLE_SILENCE;
    unsigned int safeSize, i;
    int         last, delta;

    // Does the buffer already have a sample loaded?
    if(buf->sample)
    {
        // Is the same one?
        if(buf->sample->id == sample->id)
            return;
    }

    // The safe number of samples is rounded to the next highest
    // count of SAMPLE_ROUNDOFF.
    if((i = safeNumSamples % SAMPLE_ROUNDOFF) != 0)
    {
        safeNumSamples += SAMPLE_ROUNDOFF - i;
/*#if _DEBUG
Con_Message("Safelen = %i\n", safeNumSamples);
#endif*/
    }
    safeSize = safeNumSamples * sample->bytesper;
/*#if _DEBUG
Con_Message("Safesize = %i\n", safeSize);
#endif*/

    // If a sample has already been loaded, unload it.
    freeDSBuffers(buf);

    // Create the DirectSound buffer. Its length will match the sample
    // exactly.
    if(FAILED(hr =
           createDSBuffer(DSBCAPS_CTRLVOLUME | DSBCAPS_CTRLFREQUENCY |
                          DSBCAPS_STATIC |
                          (play3d? (DSBCAPS_CTRL3D | DSBCAPS_MUTE3DATMAXDISTANCE) : DSBCAPS_CTRLPAN),
                          safeNumSamples, buf->freq, buf->bytes*8, 1,
                          &newSound)))
    {
        if(verbose)
            error("DS_Load", "Couldn't create a new buffer.");

        return;
    }

    if(play3d)
    {
        // Query the 3D interface.
        if(FAILED(hr = newSound->QueryInterface(IID_IDirectSound3DBuffer,
                                                (void**) &newSound3D)))
        {
            if(verbose)
                error("DS_Load", "Couldn't get 3D buffer interface.");
            newSound->Release();

            return;
        }
    }

    // Lock and load!
    newSound->Lock(0, 0, &writePtr1, &writeBytes1,
        &writePtr2, &writeBytes2, DSBLOCK_ENTIREBUFFER);
    if(writePtr2 && verbose)
        error("DS_Load", "Unexpected buffer lock behavior.");

    // Copy the sample data.
    memcpy(writePtr1, sample->data, sample->size);

/*#if _DEBUG
Con_Message("Fill %i bytes (orig=%i).\n", safeSize - sample->size,
            sample->size);
#endif*/

    // Interpolate to silence.
    // numSafeSamples includes at least SAMPLE_ROUNDOFF extra samples.
    if(sample->bytesper == 1)
        last = ((byte*)sample->data)[sample->numsamples - 1];
    else
        last = ((short*)sample->data)[sample->numsamples - 1];

    delta = (sample->bytesper==1? 0x80 - last : -last);

    for(i = 0; i < safeNumSamples - sample->numsamples; ++i)
    {
        float       pos = i/(float)SAMPLE_SILENCE;

        if(pos > 1)
            pos = 1;

        if(sample->bytesper == 1)
        {   // 8-bit sample.
            ((byte*)writePtr1)[sample->numsamples + i] =
                byte( last + delta*pos );
        }
        else
        {   // 16-bit sample.
            ((short*)writePtr1)[sample->numsamples + i] =
                short( last + delta*pos );
        }
    }

    // Unlock the buffer.
    newSound->Unlock(writePtr1, writeBytes1, writePtr2, writeBytes2);

    buf->ptr = newSound;
    buf->ptr3d = newSound3D;
    buf->sample = sample;

#undef SAMPLE_SILENCE
#undef SAMPLE_ROUNDOFF
}

/**
 * Stops the buffer and makes it forget about its sample.
 */
void DS_Reset(sfxbuffer_t *buf)
{
    DS_Stop(buf);
    buf->sample = NULL;
    freeDSBuffers(buf);
}

void DS_Play(sfxbuffer_t *buf)
{
    if(!buf->sample || !DSBuf(buf))
        return; // Playing is quite impossible without a sample.

    DSBuf(buf)->SetCurrentPosition(0);
    DSBuf(buf)->Play(0, 0, buf->flags & SFXBF_REPEAT? DSBPLAY_LOOPING : 0);
    // The buffer is now playing.
    buf->flags |= SFXBF_PLAYING;
}

void DS_Stop(sfxbuffer_t *buf)
{
    if(!buf->sample || !DSBuf(buf))
        return; // Nothing to stop.

    DSBuf(buf)->Stop();
    buf->flags &= ~SFXBF_PLAYING;
}

void DS_Refresh(sfxbuffer_t *buf)
{
    LPDIRECTSOUNDBUFFER sound = DSBuf(buf);
    DWORD status;

    if(sound)
    {
        // Has the buffer finished playing?
        sound->GetStatus(&status);
        if(!(status & DSBSTATUS_PLAYING) && buf->flags & SFXBF_PLAYING)
        {   // It has stopped playing.
            buf->flags &= ~SFXBF_PLAYING;
        }
    }
}

void DS_Event(int type)
{
    // Not supported.
}

/**
 * Convert linear volume 0..1 to logarithmic -10000..0.
 */
static int volLinearToLog(float vol)
{
    int         dsVol;

    if(vol <= 0)
        return DSBVOLUME_MIN;
    if(vol >= 1)
        return DSBVOLUME_MAX;

    // Straighten the volume curve.
    dsVol = (int) (100 * 20 * log10(vol));
    if(dsVol < DSBVOLUME_MIN)
        dsVol = DSBVOLUME_MIN;

    return dsVol;
}

/**
 * Convert linear pan -1..1 to logarithmic -10000..10000.
 */
static int panLinearToLog(float pan)
{
    if(pan >= 1)
        return DSBPAN_RIGHT;
    if(pan <= -1)
        return DSBPAN_LEFT;
    if(pan == 0)
        return 0;
    if(pan > 0)
        return (int) (-100 * 20 * log10(1 - pan));

    return (int) (100 * 20 * log10(1 + pan));
}

/**
 * @param prop          SFXBP_VOLUME (negative = interpreted as attenuation)
 *                      SFXBP_FREQUENCY
 *                      SFXBP_PAN (-1..1)
 *                      SFXBP_MIN_DISTANCE
 *                      SFXBP_MAX_DISTANCE
 *                      SFXBP_RELATIVE_MODE
 */
void DS_Set(sfxbuffer_t *buf, int prop, float value)
{
    LPDIRECTSOUNDBUFFER sound = DSBuf(buf);

    if(!sound)
        return;

    switch(prop)
    {
    case SFXBP_VOLUME:
        {
        long        volume;

        if(value <= 0)
            volume = (-1-value) * 10000;
        else
            volume = volLinearToLog(value);

        // Use logarithmic attenuation.
        sound->SetVolume(volume);   // Linear volume.
        break;
        }

    case SFXBP_FREQUENCY:
        {
        unsigned int f = (unsigned int) (buf->rate * value);

        // Don't set redundantly.
        if(f != buf->freq)
        {
            buf->freq = f;
            sound->SetFrequency(buf->freq);
        }
        break;
        }

    case SFXBP_PAN:
        sound->SetPan(panLinearToLog(value));
        break;

    case SFXBP_MIN_DISTANCE:
        if(!DSBuf3(buf))
            return;

        DSBuf3(buf)->SetMinDistance(value, DS3D_DEFERRED);
        break;

    case SFXBP_MAX_DISTANCE:
        if(!DSBuf3(buf))
            return;

        DSBuf3(buf)->SetMaxDistance(value, DS3D_DEFERRED);
        break;

    case SFXBP_RELATIVE_MODE:
        {
        DWORD       mode;

        if(!DSBuf3(buf))
            return;

        mode = (value? DS3DMODE_HEADRELATIVE : DS3DMODE_NORMAL);
        DSBuf3(buf)->SetMode(mode, DS3D_DEFERRED);
        break;
        }

    default:
        break;
    }
}

/**
 * @param prop          SFXBP_POSITION
 *                      SFXBP_VELOCITY
 * @param values        Coordinates specified in world coordinate system,
 *                      converted to DSound's:
 *                      +X = right, +Y = up, +Z = away.
 */
void DS_Setv(sfxbuffer_t *buf, int prop, float *values)
{
    if(!DSBuf3(buf))
        return;

    switch(prop)
    {
    case SFXBP_POSITION:
        IDirectSound3DBuffer_SetPosition(DSBuf3(buf),
            values[VX], values[VZ], values[VY], DS3D_DEFERRED);
        break;

    case SFXBP_VELOCITY:
        IDirectSound3DBuffer_SetVelocity(DSBuf3(buf),
            values[VX], values[VZ], values[VY], DS3D_DEFERRED);
        break;

    default:
        break;
    }
}

static void eaxCommitDeferred(void)
{
    if(!eaxListener)
        return;

    eaxListener->Set(DSPROPSETID_EAX_ListenerProperties,
                     DSPROPERTY_EAXLISTENER_COMMITDEFERREDSETTINGS,
                     NULL, 0, NULL, 0);
}

/**
 * @param yaw           Yaw in radians.
 * @param pitch         Pitch in radians.
 */
static void listenerOrientation(float yaw, float pitch)
{
    float       front[3], up[3];

    if(!dsListener)
        return;

    front[VX] = cos(yaw) * cos(pitch);
    front[VZ] = sin(yaw) * cos(pitch);
    front[VY] = sin(pitch);

    up[VX] = -cos(yaw) * sin(pitch);
    up[VZ] = -sin(yaw) * sin(pitch);
    up[VY] = cos(pitch);

    dsListener->SetOrientation(front[VX], front[VY], front[VZ],
                               up[VX], up[VY], up[VZ], DS3D_DEFERRED);
}

static void eaxSetdw(DWORD prop, int value)
{
    if(!eaxListener)
        return;

    eaxListener->Set(DSPROPSETID_EAX_ListenerProperties,
                     prop | DSPROPERTY_EAXLISTENER_DEFERRED, NULL, 0,
                     &value, sizeof(DWORD));
}

static void eaxSetf(DWORD prop, float value)
{
    if(!eaxListener)
        return;

    eaxListener->Set(DSPROPSETID_EAX_ListenerProperties,
                     prop | DSPROPERTY_EAXLISTENER_DEFERRED, NULL, 0,
                     &value, sizeof(float));
}

/**
 * Linear multiplication for a logarithmic property.
 */
static void eaxMuldw(DWORD prop, float mul)
{
    DWORD       retBytes;
    LONG        value;

    if(!eaxListener)
        return;

    if(FAILED(hr =
       eaxListener->Get(DSPROPSETID_EAX_ListenerProperties,
                        prop, NULL, 0, &value, sizeof(value), &retBytes)))
    {
        return;
    }

    eaxSetdw(prop, volLinearToLog(pow(10.0f, value/2000.0f) * mul));
}

/**
 * Linear multiplication for a linear property.
 */
static void eaxMulf(DWORD prop, float mul, float min, float max)
{
    DWORD retBytes;
    float value;

    if(!eaxListener)
        return;

    if(FAILED(hr =
       eaxListener->Get(DSPROPSETID_EAX_ListenerProperties,
                        prop, NULL, 0, &value, sizeof(value), &retBytes)))
    {
        return;
    }

    value *= mul;

    // Clamp.
    if(value < min)
        value = min;
    if(value > max)
        value = max;

    eaxSetf(prop, value);
}

/**
 * Set a property of a listener.
 *
 * @param prop          SFXLP_UNITS_PER_METER
 *                      SFXLP_DOPPLER
 *                      SFXLP_UPDATE
 * @param value         Value to be set.
 */
void DS_Listener(int prop, float value)
{
    if(!dsListener)
        return;

    switch(prop)
    {
    case SFXLP_UPDATE:
        // Commit any deferred settings.
        dsListener->CommitDeferredSettings();
        eaxCommitDeferred();
        break;

    case SFXLP_UNITS_PER_METER:
        dsListener->SetDistanceFactor(1/value, DS3D_IMMEDIATE);
        break;

    case SFXLP_DOPPLER:
        dsListener->SetDopplerFactor(value, DS3D_IMMEDIATE);
        break;

    default:
        break;
    }
}

/**
 * If EAX is available, set the listening environmental properties.
 * Values use SRD_* for indices.
 */
static void listenerEnvironment(float *rev)
{
    float       val;

    // This can only be done if EAX is available.
    if(!eaxListener)
        return;

    val = rev[SRD_SPACE];
    if(rev[SRD_DECAY] > .5)
    {   // This much decay needs at least the Generic environment.
        if(val < .2)
            val = .2f;
    }

    // Set the environment. Other properties are updated automatically.
    eaxSetdw(DSPROPERTY_EAXLISTENER_ENVIRONMENT,
             val >= 1? EAX_ENVIRONMENT_PLAIN
             : val >= .8? EAX_ENVIRONMENT_CONCERTHALL
             : val >= .6? EAX_ENVIRONMENT_AUDITORIUM
             : val >= .4? EAX_ENVIRONMENT_CAVE
             : val >= .2? EAX_ENVIRONMENT_GENERIC
             : EAX_ENVIRONMENT_ROOM);

    // General reverb volume adjustment.
    eaxSetdw(DSPROPERTY_EAXLISTENER_ROOM, volLinearToLog(rev[SRD_VOLUME]));

    // Reverb decay.
    val = (rev[SRD_DECAY] - .5) * 1.5 + 1;
    eaxMulf(DSPROPERTY_EAXLISTENER_DECAYTIME, val,
        EAXLISTENER_MINDECAYTIME, EAXLISTENER_MAXDECAYTIME);

    // Damping.
    val = 1.1f * (1.2f - rev[SRD_DAMPING]);
    if(val < .1)
        val = .1f;
    eaxMuldw(DSPROPERTY_EAXLISTENER_ROOMHF, val);

    // A slightly increased roll-off.
    eaxSetf(DSPROPERTY_EAXLISTENER_ROOMROLLOFFFACTOR, 1.3f);
}

/**
 * Call SFXLP_UPDATE at the end of every channel update.
 */
void DS_Listenerv(int prop, float *values)
{
    if(!dsListener)
        return;

    switch(prop)
    {
    case SFXLP_POSITION:
        dsListener->SetPosition(values[VX], values[VZ], values[VY],
                                DS3D_DEFERRED);
        break;

    case SFXLP_VELOCITY:
        dsListener->SetVelocity(values[VX], values[VZ], values[VY],
                                DS3D_DEFERRED);
        break;

    case SFXLP_ORIENTATION:
        listenerOrientation(values[VX] / 180 * PI, values[VY] / 180 * PI);
        break;

    case SFXLP_REVERB:
        listenerEnvironment(values);
        break;

    default:
        DS_Listener(prop, 0);
        break;
    }
}
