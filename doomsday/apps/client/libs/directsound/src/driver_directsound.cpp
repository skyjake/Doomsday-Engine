/**\file driver_directsound.cpp
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
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
 * Win32 SFX driver for DirectSound, with EAX 2.0.
 *
 * Uses DirectSound 8.0
 * Buffers created on Load.
 */

// HEADER FILES ------------------------------------------------------------

#define DIRECTSOUND_VERSION 0x0800

#define WIN32_LEAN_AND_MEAN

#include <math.h>
#include <stdlib.h>
#include <windows.h>
#include <mmsystem.h>
#include <dsound.h>
#ifdef DE_HAVE_EAX2
#  include <eax.h>
#endif

#pragma warning (disable: 4035 4244)

#include <de/c_wrapper.h>
#include <de/legacy/timer.h>

#include "doomsday.h"
#include "api_audiod.h"
#include "api_audiod_sfx.h"

DE_DECLARE_API(Base);
DE_DECLARE_API(Con);

// MACROS ------------------------------------------------------------------

// DirectSound(3D)Buffer Pointer
#define DSBUF(buf)          ((LPDIRECTSOUNDBUFFER) buf->ptr)
#define DSBUF3D(buf)        ((LPDIRECTSOUND3DBUFFER8) buf->ptr3D)

#define MAX_FAILED_PROPS    (10)

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

extern "C" {
int             DS_Init(void);
void            DS_Shutdown(void);
void            DS_Event(int type);

int             DS_SFX_Init(void);
sfxbuffer_t*    DS_SFX_CreateBuffer(int flags, int bits, int rate);
void            DS_SFX_DestroyBuffer(sfxbuffer_t* buf);
void            DS_SFX_Load(sfxbuffer_t* buf, struct sfxsample_s* sample);
void            DS_SFX_Reset(sfxbuffer_t* buf);
void            DS_SFX_Play(sfxbuffer_t* buf);
void            DS_SFX_Stop(sfxbuffer_t* buf);
void            DS_SFX_Refresh(sfxbuffer_t* buf);
void            DS_SFX_Set(sfxbuffer_t* buf, int prop, float value);
void            DS_SFX_Setv(sfxbuffer_t* buf, int prop, float* values);
void            DS_SFX_Listener(int prop, float value);
void            DS_SFX_Listenerv(int prop, float* values);
}

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void     commitEAXDeferred(void);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

static dd_bool initOk = false;

static LPDIRECTSOUND8 dsound = NULL;
static LPDIRECTSOUNDBUFFER primary = NULL;
static LPDIRECTSOUND3DLISTENER8 dsListener = NULL;
static LPKSPROPERTYSET propertySet = NULL;
static dd_bool ignoreEAXErrors = false;
static dd_bool canSetPSF = true;

static DWORD failedProps[MAX_FAILED_PROPS];

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

static IDirectSoundBuffer8* createBuffer(DSBUFFERDESC* desc)
{
    IDirectSoundBuffer* buf;
    IDirectSoundBuffer8* buf8;
    HRESULT             hr;

    if(!desc)
        return NULL;

    // Try to create a secondary buffer with the requested properties.
    if(FAILED(hr = dsound->CreateSoundBuffer(desc, &buf, NULL)))
        return NULL;

    // Obtain the DirectSoundBuffer8 interface.
    if(FAILED
       (hr = buf->QueryInterface(IID_IDirectSoundBuffer8, (LPVOID*) &buf8)))
        buf8 = NULL;

    // Release the old interface, we don't need it.
    buf->Release();
    return buf8;
}

static IDirectSound3DBuffer8* get3DBuffer(IDirectSoundBuffer8* buf8)
{
    IDirectSound3DBuffer8* buf3d;
    HRESULT             hr;

    if(!buf8)
        return NULL;

    // Query the 3D interface.
    if(FAILED(hr = buf8->QueryInterface(IID_IDirectSound3DBuffer8,
                                        (LPVOID*) &buf3d)))
    {
        App_Log(DE2_DEV_AUDIO_WARNING, "[DirectSound] get3DBuffer: Failed to get 3D interface (0x%x)", hr);
        buf3d = NULL;
    }

    return buf3d;
}

#ifdef DE_HAVE_EAX2
/**
 * Does the EAX implementation support getting/setting of a propertry.
 *
 * @param prop          Property id (constant) to be checked.
 * @return              @c true, if supported.
 */
static dd_bool queryEAXSupport(int prop)
{
#  define EAXSUP          (KSPROPERTY_SUPPORT_GET | KSPROPERTY_SUPPORT_SET)

    if(propertySet)
    {
        ULONG               support = 0;

        propertySet->QuerySupport(DSPROPSETID_EAX_ListenerProperties, prop,
                                  &support);

        return (support & EAXSUP) == EAXSUP? true : false;
    }

    return false;

#  undef EAXSUP
}
#endif

/**
 * Init DirectSound, start playing the primary buffer.
 *
 * @return              @c true, iff successful.
 */
int DS_Init(void)
{
#define NUMBUFFERS_HW_3D ((uint) dsoundCaps.dwFreeHw3DStreamingBuffers)
#define NUMBUFFERS_HW_2D ((uint) dsoundCaps.dwFreeHwMixingStreamingBuffers)

#ifdef DE_HAVE_EAX2
    typedef struct eaxproperty_s {
        DSPROPERTY_EAX_LISTENERPROPERTY prop;
        char*           name;
    } eaxproperty_t;

    static const eaxproperty_t eaxProps[] = {
        { DSPROPERTY_EAXLISTENER_ENVIRONMENT, "Environment" },
        { DSPROPERTY_EAXLISTENER_ROOM, "Room" },
        { DSPROPERTY_EAXLISTENER_ROOMHF, "Room HF" },
        { DSPROPERTY_EAXLISTENER_DECAYTIME, "Decay time" },
        { DSPROPERTY_EAXLISTENER_ROOMROLLOFFFACTOR, "Room roll-off factor" },
        { DSPROPERTY_EAXLISTENER_NONE, NULL } // terminate.
    };
#endif

    DSBUFFERDESC        desc;
    DSCAPS              dsoundCaps;
    HWND                hWnd;
    HRESULT             hr;
    //uint                numHW3DBuffers = 0;
    dd_bool             useEAX, eaxAvailable = false,
                        primaryBuffer3D = false, primaryBufferHW = false;
    dd_bool             haveInstance = false;

    if(dsound)
        return true; // Already initialized?

    App_Log(DE2_AUDIO_VERBOSE, "[DirectSound] Initializing Direct Sound...");

    // Can we set the Primary Sound Format?
    canSetPSF = !CommandLine_Exists("-nopsf");
    useEAX = !CommandLine_Exists("-noeax");

    if(!(hWnd = (HWND) DD_GetVariable(DD_WINDOW_HANDLE)))
    {
        App_Log(DE2_AUDIO_ERROR, "[DirectSound] Cannot initialize DirectSound: "
                "main window unavailable");
        return false;
    }

    // First try to create the DirectSound8 object with EAX support.
    hr = DSERR_GENERIC;
#ifdef DE_HAVE_EAX2
    if(useEAX)
    {
        if((hr = EAXDirectSoundCreate8(NULL, &dsound, NULL)) == DS_OK)
        {
            haveInstance = true;
            eaxAvailable = true;
        }
        else
        {
            App_Log(DE2_AUDIO_VERBOSE, "[DirectSound] EAX could not be initialized (0x%x)", hr);
        }
    }
#endif

    // Try plain old DS, then.
    if(!haveInstance)
    {
        if((hr = DirectSoundCreate8(NULL, &dsound, NULL)) == DS_OK)
        {
            haveInstance = true;
        }
        else
        {
            App_Log(DE2_AUDIO_ERROR, "[DirectSound] Failed to create the DS8 instance (0x%x)", hr);
        }
    }

    if(!haveInstance)
    {   // Oh dear. Give up.
        return false;
    }

    // Set cooperative level.
    if((hr = dsound->SetCooperativeLevel(hWnd, DSSCL_PRIORITY)) != DS_OK)
    {
        App_Log(DE2_AUDIO_ERROR, "[DirectSound] Failed to set cooperative level (0x%x)", hr);
        return false;
    }

    // Lets query the device caps.
    dsoundCaps.dwSize = sizeof(dsoundCaps);
    if((hr = dsound->GetCaps(&dsoundCaps)) != DS_OK)
    {
        App_Log(DE2_AUDIO_ERROR, "[DirectSound] Failed querying device caps (0x%x)", hr);
        return false;
    }

    dsListener = NULL;
    if(NUMBUFFERS_HW_3D < 4)
        useEAX = false;

    ZeroMemory(&desc, sizeof(DSBUFFERDESC));
    desc.dwSize = sizeof(DSBUFFERDESC);

    /**
     * Create the primary buffer.
     * We prioritize buffer creation as follows:
     * 3D hardware > 3D software > 2D hardware > 2D software.
     */

    // First try for a 3D buffer, hardware or software.
    desc.dwFlags = DSBCAPS_PRIMARYBUFFER | DSBCAPS_CTRL3D | DSBCAPS_CTRLVOLUME;
    desc.dwFlags |= (NUMBUFFERS_HW_3D > 0? DSBCAPS_LOCHARDWARE : DSBCAPS_LOCSOFTWARE);

    hr = dsound->CreateSoundBuffer(&desc, &primary, NULL);
    if(hr != DS_OK && hr != DS_NO_VIRTUALIZATION)
    {   // Not available.
        // Try for a 2D buffer.
        ZeroMemory(&desc, sizeof(DSBUFFERDESC));
        desc.dwSize = sizeof(DSBUFFERDESC);

        desc.dwFlags = DSBCAPS_PRIMARYBUFFER | DSBCAPS_CTRLVOLUME;
        desc.dwFlags |= (NUMBUFFERS_HW_2D > 0? DSBCAPS_LOCHARDWARE : DSBCAPS_LOCSOFTWARE);

        if((hr = dsound->CreateSoundBuffer(&desc, &primary, NULL)) != DS_OK)
        {
            App_Log(DE2_AUDIO_ERROR, "[DirectSound] Failed creating primary (2D) buffer (0x%x)", hr);
            return false;
        }

        primaryBufferHW = (NUMBUFFERS_HW_2D > 0? true : false);
    }
    else
    {   // 3D buffer available.
        primaryBuffer3D = true;
        primaryBufferHW = (NUMBUFFERS_HW_3D > 0? true : false);

        // Get the listener.
        if(FAILED(hr =
            primary->QueryInterface(IID_IDirectSound3DListener,
                                    (LPVOID*) &dsListener)))
        {
            App_Log(DE2_DEV_AUDIO_MSG, "[DirectSound] 3D listener not available (0x%x)", hr);
        }
    }

    // Start playing the primary buffer.
    if(primary)
    {
        // Supposedly can be a bit more efficient not to stop the
        // primary buffer when there are no secondary buffers playing.
        primary->Play(0, 0, DSBPLAY_LOOPING);
    }

    // Try to get the EAX listener property set.
    // Create a temporary secondary buffer for it.
    if(eaxAvailable && useEAX)
    {
        IDirectSoundBuffer8* dummy;
        IDirectSound3DBuffer8* dummy3d;
        DSBUFFERDESC        desc;
        WAVEFORMATEX        wave;

        // Clear the failed properties array.
        memset(failedProps, ~0, sizeof(failedProps));

        propertySet = NULL;
        if(CommandLine_Exists("-eaxignore"))
            ignoreEAXErrors = true;

        // Configure the temporary buffer.
        ZeroMemory(&desc, sizeof(DSBUFFERDESC));
        desc.dwSize = sizeof(DSBUFFERDESC);
        desc.dwBufferBytes = DSBSIZE_MIN;
        desc.dwFlags = DSBCAPS_STATIC | DSBCAPS_CTRL3D;
        desc.lpwfxFormat = &wave;

        ZeroMemory(&wave, sizeof(WAVEFORMATEX));
        wave.wFormatTag = WAVE_FORMAT_PCM;
        wave.nChannels = 1;
        wave.nSamplesPerSec = 44100;
        wave.wBitsPerSample = 16;
        wave.nBlockAlign = 2;
        wave.nAvgBytesPerSec = 88200;

        // Create the temporary buffer.
        if(!(dummy = createBuffer(&desc)))
            return false;

        // Get the 3D interface.
        if(!(dummy3d = get3DBuffer(dummy)))
            return false;

#ifdef DE_HAVE_EAX2
        // Query the property set interface
        dummy3d->QueryInterface(IID_IKsPropertySet, (LPVOID*) &propertySet);
        if(propertySet)
        {
            size_t              i = 0;
            dd_bool             ok = true;

            while(ok && eaxProps[i].prop != DSPROPERTY_EAXLISTENER_NONE)
            {
                const eaxproperty_t* p = &eaxProps[i];

                if(!queryEAXSupport(p->prop))
                    ok = false;
                else
                    i++;
            }

            if(!ok)
            {
                useEAX = false;

                propertySet->Release();
                propertySet = NULL;
            }
        }
        else
#endif
        {
            useEAX = false;

            App_Log(DE2_LOG_DEV | DE2_LOG_AUDIO | DE2_LOG_WARNING,
                    "dsDirectSound::DS_Init: Failed retrieving property set.");
        }
    }

    // Announce capabilites:
    App_Log(DE2_LOG_AUDIO, "DirectSound configuration:");
    App_Log(DE2_LOG_AUDIO, "  Primary Buffer: %s (%s)", (primaryBuffer3D? "3D" : "2D"),
            (primaryBufferHW? "hardware" : "software"));
    App_Log(DE2_LOG_AUDIO, "  Hardware Buffers: %i", (primaryBuffer3D? NUMBUFFERS_HW_3D : NUMBUFFERS_HW_2D));
    LogBuffer_Printf(DE2_LOG_AUDIO, "  DSP: %s", eaxAvailable? "EAX 2.0" : "None");
    if(eaxAvailable)
        LogBuffer_Printf(DE2_LOG_AUDIO, " (%s)", useEAX? "enabled" : "disabled");
    LogBuffer_Printf(DE2_LOG_AUDIO, "\n");

#ifdef DE_HAVE_EAX2
    if(eaxAvailable)
    {
        App_Log(DE2_LOG_AUDIO, "  EAX Listner Environment:");
        for(size_t i = 0; eaxProps[i].prop != DSPROPERTY_EAXLISTENER_NONE; ++i)
        {
            const eaxproperty_t* p = &eaxProps[i];

            App_Log(DE2_LOG_AUDIO, "    %s: %s", p->name,
                    queryEAXSupport(p->prop)? "Present" : "Not available");
        }
    }
#endif

    // Success!
    App_Log(DE2_LOG_AUDIO | DE2_LOG_VERBOSE | DE2_LOG_DEV,
                     "dsDirectSound::DS_Init: Initialization complete, OK.");
    return true;

#undef NUMBUFFERS_HW_3D
#undef NUMBUFFERS_HW_2D
}

/**
 * Shut everything down.
 */
void DS_Shutdown(void)
{
    if(propertySet)
        propertySet->Release();
    propertySet = NULL;

    if(dsListener)
        dsListener->Release();
    dsListener = NULL;

    if(primary)
        primary->Release();
    primary = NULL;

    if(dsound)
        dsound->Release();
    dsound = NULL;
}

/**
 * The Event function is called to tell the driver about certain critical
 * events like the beginning and end of an update cycle.
 */
void DS_Event(int /*type*/)
{
    // Do nothing...
}

int DS_SFX_Init(void)
{
    return true;
}

/**
 * Called using Listener().
 */
static void setPrimaryFormat(int bits, int rate)
{
    WAVEFORMATEX        wave;

    memset(&wave, 0, sizeof(wave));
    wave.wFormatTag = WAVE_FORMAT_PCM;
    wave.nChannels = 2;
    wave.nSamplesPerSec = rate;
    wave.nBlockAlign = wave.nChannels * bits / 8;
    wave.nAvgBytesPerSec = wave.nSamplesPerSec * wave.nBlockAlign;
    wave.wBitsPerSample = bits;

    primary->SetFormat(&wave);
}

sfxbuffer_t* DS_SFX_CreateBuffer(int flags, int bits, int rate)
{
    int                 i;
    WAVEFORMATEX        format;
    DSBUFFERDESC        desc;
    IDirectSoundBuffer8* buf_object8;
    IDirectSound3DBuffer8* buf_object3d = NULL;
    sfxbuffer_t*        buf;

    // If we don't have the listener, the primary buffer doesn't have 3D
    // capabilities; don't create 3D buffers. DSound should provide software
    // emulation, though, so this is really only a contingency.
    if(!dsListener && flags & SFXBF_3D)
        return NULL;

    // Setup the buffer descriptor.
    memset(&desc, 0, sizeof(desc));
    desc.dwSize = sizeof(desc);
    desc.dwFlags = DSBCAPS_CTRLFREQUENCY | DSBCAPS_CTRLVOLUME | DSBCAPS_STATIC |
        (flags & SFXBF_3D ? DSBCAPS_CTRL3D : DSBCAPS_CTRLPAN);

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

    buf_object8 = createBuffer(&desc);
    if(!buf_object8)
    {
        App_Log(DE2_AUDIO_WARNING, "[DirectSound] Failed to create buffer (rate:%i bits:%i)",
                rate, bits);
        return NULL;
    }

    // How about a 3D interface?
    if(flags & SFXBF_3D)
    {
        buf_object3d = get3DBuffer(buf_object8);
        if(!buf_object3d)
        {
            App_Log(DE2_AUDIO_WARNING,"[DirectSound] Failed to get a 3D interface for audio buffer");
            buf_object8->Release();
            return NULL;
        }
    }

    // Clear the buffer.
    buf = (sfxbuffer_t*) Z_Calloc(sizeof(*buf), PU_APPSTATIC, 0);

    buf->ptr = buf_object8;
    buf->ptr3D = buf_object3d;
    buf->bytes = bits / 8;
    buf->rate = rate;
    buf->flags = flags;
    buf->length = desc.dwBufferBytes;
    buf->freq = rate; // Modified by calls to Set(SFXBP_FREQUENCY).

    return buf;
}

void DS_SFX_DestroyBuffer(sfxbuffer_t* buf)
{
    if(!buf)
        return;

    DSBUF(buf)->Release();
    // Free the memory allocated for the buffer.
    Z_Free(buf);
}

/**
 * Prepare the buffer for playing a sample by filling the buffer with as
 * much sample data as fits. The pointer to sample is saved, so the caller
 * mustn't free it while the sample is loaded.
 */
void DS_SFX_Load(sfxbuffer_t* buf, struct sfxsample_s* sample)
{
    void*               data;
    DWORD               lockedBytes, wroteBytes;
    HRESULT             hr;

    if(!buf || !sample)
        return;

    // Try to lock the buffer.
    if(FAILED
       (hr = DSBUF(buf)->Lock(0, 0, &data, &lockedBytes, 0, 0,
                              DSBLOCK_ENTIREBUFFER)))
        return; // Couldn't lock!

    // Write as much data as we can.
    wroteBytes = MIN_OF(lockedBytes, sample->size);
    memcpy(data, sample->data, wroteBytes);

    // Fill the rest with zeroes.
    if(wroteBytes < lockedBytes)
    {
        // Set the end marker since we already know it.
        buf->cursor = wroteBytes;
        memset((char *) data + wroteBytes, buf->bytes == 1 ? 128 : 0,
               lockedBytes - wroteBytes);
    }
    else
    {
        // The whole buffer was filled, thus leaving the cursor at
        // the beginning.
        buf->cursor = 0;
    }

    DSBUF(buf)->Unlock(data, lockedBytes, 0, 0);

    // Now the buffer is ready for playing.
    buf->sample = sample;
    buf->written = wroteBytes;
    buf->flags &= ~SFXBF_RELOAD;

    // Zero the play cursor.
    DSBUF(buf)->SetCurrentPosition(0);
}

/**
 * Stops the buffer and makes it forget about its sample.
 */
void DS_SFX_Reset(sfxbuffer_t* buf)
{
    if(!buf)
        return;

    DS_SFX_Stop(buf);
    buf->sample = NULL;
    buf->flags &= ~SFXBF_RELOAD;
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

void DS_SFX_Play(sfxbuffer_t* buf)
{
    HRESULT             hr;

    // Playing is quite impossible without a sample.
    if(!buf || !buf->sample)
        return;

    // Do we need to reload?
    if(buf->flags & SFXBF_RELOAD)
        DS_SFX_Load(buf, buf->sample);

    // The sound starts playing now?
    if(!(buf->flags & SFXBF_PLAYING))
    {
        // Calculate the end time (milliseconds).
        buf->endTime = Timer_RealMilliseconds() + getBufLength(buf);
    }

    if(FAILED(hr = DSBUF(buf)->Play(0, 0, DSBPLAY_LOOPING)))
        return;

    // The buffer is now playing.
    buf->flags |= SFXBF_PLAYING;
}

void DS_SFX_Stop(sfxbuffer_t* buf)
{
    if(!buf)
        return;

    DSBUF(buf)->Stop();

    // Clear the flag that tells the Sfx module about playing buffers.
    buf->flags &= ~SFXBF_PLAYING;

    // If the sound is started again, it needs to be reloaded.
    buf->flags |= SFXBF_RELOAD;
}

/*
static dd_bool InRange(uint pos, uint start, uint end)
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
*/

/**
 * Buffer streamer. Called by the Sfx refresh thread.
 * Copy sample data into the buffer, and if the sample has ended, stop
 * playing the buffer. If the buffer has been lost for some reason, restore
 * it. Don't do anything time-consuming...
 */
void DS_SFX_Refresh(sfxbuffer_t* buf)
{
    DWORD               play, bytes[2], dose, fill;
    void*               data[2];
    int                 writeBytes, i;
    float               usedSec;
    unsigned int        usedTime, nowTime;
    HRESULT             hr;

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
        DS_SFX_Stop(buf);
        return;
    }

    // Slightly redundant... (used = now - start)
    usedTime = nowTime - (buf->endTime - getBufLength(buf));

    // Approximate the current playing position (-0.1 sec for safety; we
    // don't want to overwrite stuff before it gets played).
    usedSec = usedTime / 1000.0f - 0.1f;
    if(usedSec <= 0)
    {   // The update is a bit early; let's wait for the next one.
        return;
    }

    play = (int) (usedSec * buf->freq * buf->bytes) % buf->length;

    // How many bytes we must write (from buffer cursor up to play cursor).
    if(buf->cursor < play)
        writeBytes = play - buf->cursor;
    else // Play has looped back to the beginning.
        writeBytes = buf->length - buf->cursor + play;

    // Try to lock the region, restoring if failed.
    for(i = 0; i < 2; ++i)
    {
        if(FAILED
           (hr = DSBUF(buf)->Lock(buf->cursor, writeBytes, &data[0],
                                  &bytes[0], &data[1], &bytes[1], 0)))
        {
            if(hr == DSERR_BUFFERLOST)
            {
                DSBUF(buf)->Restore();
                continue;
            }
        }

        break;
    }

    if(FAILED(hr))
        return; // Give up.

    // Copy in two parts: as much sample data as we've got, and then zeros.
    for(i = 0; i < 2 && data[i]; ++i)
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

        // Wrap the cursor back to the beginning if needed. The wrap can
        // only happen after the first write, really (where the buffer
        // "breaks").
        if(buf->cursor >= buf->length)
            buf->cursor -= buf->length;
    }

    // And we're done! Unlock and get out of here.
    DSBUF(buf)->Unlock(data[0], bytes[0], data[1], bytes[1]);

    // If the buffer is in repeat mode, go back to the beginning once the
    // end has been reached.
    if((buf->flags & SFXBF_REPEAT) && buf->written == buf->sample->size)
        buf->written = 0;
}

/**
 * Convert linear volume 0..1 to logarithmic -10000..0.
 */
static int volLinearToLog(float vol)
{
    if(vol <= 0)
        return DSBVOLUME_MIN;

    if(vol >= 1)
        return DSBVOLUME_MAX;

    // Straighten the volume curve.
    return MINMAX_OF(DSBVOLUME_MIN, (int) (100 * 20 * log10(vol)),
                     DSBVOLUME_MAX);
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
 * SFXBP_VOLUME (if negative, interpreted as attenuation)
 * SFXBP_FREQUENCY
 * SFXBP_PAN (-1..1)
 * SFXBP_MIN_DISTANCE
 * SFXBP_MAX_DISTANCE
 * SFXBP_RELATIVE_MODE
 */
void DS_SFX_Set(sfxbuffer_t* buf, int prop, float value)
{
    if(!buf)
        return;

    switch(prop)
    {
    default:
#if _DEBUG
Con_Error("dsDS9::DS_DSoundSet: Unknown prop %i.", prop);
#endif
        break;

    case SFXBP_VOLUME:
        {
        LONG            volume;

        if(value <= 0) // Use logarithmic attenuation.
            volume = (LONG) ((-1 - value) * 10000);
        else // Linear volume.
            volume = (LONG) volLinearToLog(value);

        DSBUF(buf)->SetVolume(volume);
        break;
        }
    case SFXBP_FREQUENCY:
        {
        unsigned int    freq = (unsigned int) (buf->rate * value);

        // Don't set redundantly.
        if(freq != buf->freq)
        {
            buf->freq = freq;
            DSBUF(buf)->SetFrequency(freq);
        }
        break;
        }
    case SFXBP_PAN:
        DSBUF(buf)->SetPan(panLinearToLog(value));
        break;

    case SFXBP_MIN_DISTANCE:
        if(!DSBUF3D(buf))
            return;
        DSBUF3D(buf)->SetMinDistance(value, DS3D_DEFERRED);
        break;

    case SFXBP_MAX_DISTANCE:
        if(!DSBUF3D(buf))
            return;
        DSBUF3D(buf)->SetMaxDistance(value, DS3D_DEFERRED);
        break;

    case SFXBP_RELATIVE_MODE:
        if(!DSBUF3D(buf))
            return;
        DSBUF3D(buf)->SetMode(value? DS3DMODE_HEADRELATIVE :
                             DS3DMODE_NORMAL, DS3D_DEFERRED);
        break;
    }
}

/**
 * Coordinates specified in world coordinate system, converted to DSound's:
 * +X to the right, +Y up and +Z away (Y and Z swapped, i.e.).
 *
 * @param prop          SFXBP_POSITION
 *                      SFXBP_VELOCITY
 */
void DS_SFX_Setv(sfxbuffer_t* buf, int prop, float* values)
{
    if(!buf || !values)
        return;

    if(!DSBUF3D(buf))
        return;

    switch(prop)
    {
    default:
#if _DEBUG
Con_Error("dsDS9::DS_DSoundSetv: Unknown prop %i.", prop);
#endif
        break;

    case SFXBP_POSITION:
        DSBUF3D(buf)->SetPosition(values[VX], values[VZ], values[VY],
                                 DS3D_DEFERRED);
        break;

    case SFXBP_VELOCITY:
        DSBUF3D(buf)->SetVelocity(values[VX], values[VZ], values[VY],
                                 DS3D_DEFERRED);
        break;
    }
}

/**
 * Parameters are in radians.
 * Example front vectors:
 *   Yaw 0:(0,0,1), pi/2:(-1,0,0)
 */
static void listenerOrientation(float yaw, float pitch)
{
    float               front[3], up[3];

    front[VX] = cos(yaw) * cos(pitch);
    front[VZ] = sin(yaw) * cos(pitch);
    front[VY] = sin(pitch);

    up[VX] = -cos(yaw) * sin(pitch);
    up[VZ] = -sin(yaw) * sin(pitch);
    up[VY] = cos(pitch);

    dsListener->SetOrientation(front[VX], front[VY], front[VZ],
                               up[VX], up[VY], up[VZ], DS3D_DEFERRED);
}

#if DE_HAVE_EAX2

/**
 * Set the property as 'failed'. No more errors are reported for it.
 */
static void setEAXFailed(DWORD prop)
{
    int                 i;

    for(i = 0; i < MAX_FAILED_PROPS; ++i)
        if(failedProps[i] == ~0)
        {
            failedProps[i] = prop;
            break;
        }
}

/**
 * @return              @c true, if the specified property has failed.
 */
static dd_bool hasEAXFailed(DWORD prop)
{
    int                 i;

    for(i = 0; i < MAX_FAILED_PROPS; ++i)
        if(failedProps[i] == prop)
            return true;

    return false;
}

/**
 * @return              @c true, if an EAX error should be reported.
 */
static dd_bool reportEAXError(DWORD prop, HRESULT hr)
{
    if(ignoreEAXErrors)
        return false;
    if(hr != DSERR_UNSUPPORTED)
        return true;
    if(hasEAXFailed(prop))
        return false; // Don't report again.

    setEAXFailed(prop);

    return true; // First time, do report.
}

static void setEAXdw(DWORD prop, int value)
{
    HRESULT             hr;

    if(FAILED
       (hr = propertySet->Set(DSPROPSETID_EAX_ListenerProperties,
                              prop | DSPROPERTY_EAXLISTENER_DEFERRED, NULL,
                              0, &value, sizeof(DWORD))))
    {
        if(reportEAXError(prop, hr))
            App_Log(DE2_DEV_AUDIO_WARNING, "setEAXdw (prop:%i value:%i) failed. Result: %x",
                    prop, value, hr);
    }
}

static void setEAXf(DWORD prop, float value)
{
    HRESULT             hr;

    if(FAILED
       (hr = propertySet->Set(DSPROPSETID_EAX_ListenerProperties,
                              prop | DSPROPERTY_EAXLISTENER_DEFERRED, NULL,
                              0, &value, sizeof(float))))
    {
        if(reportEAXError(prop, hr))
            App_Log(DE2_DEV_AUDIO_WARNING,"setEAXf (prop:%i value:%f) failed. Result: %x",
                    prop, value, hr);
    }
}

/**
 * Linear multiplication for a logarithmic property.
 */
static void mulEAXdw(DWORD prop, float mul)
{
    DWORD               retBytes;
    LONG                value;
    HRESULT             hr;

    if(FAILED
       (hr = propertySet->Get(DSPROPSETID_EAX_ListenerProperties,
                              prop, NULL, 0, &value, sizeof(value),
                              &retBytes)))
    {
        if(reportEAXError(prop, hr))
            App_Log(DE2_DEV_AUDIO_WARNING, "mulEAXdw (prop:%i) get failed. Result: %x",
                    prop, hr & 0xffff);
    }

    setEAXdw(prop, volLinearToLog(pow(10, value / 2000.0f) * mul));
}

/**
 * Linear multiplication for a linear property.
 */
static void mulEAXf(DWORD prop, float mul, float min, float max)
{
    DWORD               retBytes;
    HRESULT             hr;
    float               value;

    if(FAILED
       (hr = propertySet->Get(DSPROPSETID_EAX_ListenerProperties,
                              prop, NULL, 0, &value, sizeof(value),
                              &retBytes)))
    {
        if(reportEAXError(prop, hr))
            App_Log(DE2_DEV_AUDIO_WARNING,
                    "mulEAXf (prop:%i) get failed. Result: %x", prop, hr & 0xffff);
    }

    value *= mul;
    if(value < min)
        value = min;
    if(value > max)
        value = max;

    setEAXf(prop, value);
}

#endif

/**
 * Set a property of a listener.
 *
 * @param prop          SFXLP_UNITS_PER_METER
 *                      SFXLP_DOPPLER
 *                      SFXLP_UPDATE
 * @param value         Value to be set.
 */
void DS_SFX_Listener(int prop, float value)
{
    if(!dsListener)
        return;

    switch(prop)
    {
    default:
#if _DEBUG
Con_Error("dsDS9::DS_DSoundListener: Unknown prop %i.", prop);
#endif
        break;

    case SFXLP_UPDATE:
        // Commit any deferred settings.
        dsListener->CommitDeferredSettings();
#ifdef DE_HAVE_EAX2
        commitEAXDeferred();
#endif
        break;

    case SFXLP_UNITS_PER_METER:
        dsListener->SetDistanceFactor(1 / value, DS3D_IMMEDIATE);
        break;

    case SFXLP_DOPPLER:
        dsListener->SetDopplerFactor(value, DS3D_IMMEDIATE);
        break;
    }
}

#ifdef DE_HAVE_EAX2
static void commitEAXDeferred(void)
{
    if(!propertySet)
        return;

    propertySet->Set(DSPROPSETID_EAX_ListenerProperties,
                     DSPROPERTY_EAXLISTENER_COMMITDEFERREDSETTINGS, NULL,
                     0, NULL, 0);
}

/**
 * If EAX is available, set the listening environmental properties.
 * Values use SRD_* for indices.
 */
static void listenerEnvironment(float* rev)
{
    float               val;
    int                 eaxVal;

    if(!rev)
        return;

    // This can only be done if EAX is available.
    if(!propertySet)
        return;

    val = rev[SFXLP_REVERB_SPACE];
    if(rev[SFXLP_REVERB_DECAY] > .5)
    {
        // This much decay needs at least the Generic environment.
        if(val < .2)
            val = .2f;
    }

    // Set the environment. Other properties are updated automatically.
    if(val >= 1)
        eaxVal = EAX_ENVIRONMENT_PLAIN;
    else if(val >= .8)
        eaxVal = EAX_ENVIRONMENT_CONCERTHALL;
    else if(val >= .6)
        eaxVal = EAX_ENVIRONMENT_AUDITORIUM;
    else if(val >= .4)
        eaxVal = EAX_ENVIRONMENT_CAVE;
    else if(val >= .2)
        eaxVal = EAX_ENVIRONMENT_GENERIC;
    else
        eaxVal = EAX_ENVIRONMENT_ROOM;
    setEAXdw(DSPROPERTY_EAXLISTENER_ENVIRONMENT, eaxVal);

    // General reverb volume adjustment.
    setEAXdw(DSPROPERTY_EAXLISTENER_ROOM, volLinearToLog(rev[SFXLP_REVERB_VOLUME]));

    // Reverb decay.
    val = (rev[SFXLP_REVERB_DECAY] - .5f) * 1.5f + 1;
    mulEAXf(DSPROPERTY_EAXLISTENER_DECAYTIME, val, EAXLISTENER_MINDECAYTIME,
               EAXLISTENER_MAXDECAYTIME);

    // Damping.
    val = 1.1f * (1.2f - rev[SFXLP_REVERB_DAMPING]);
    if(val < .1)
        val = .1f;
    mulEAXdw(DSPROPERTY_EAXLISTENER_ROOMHF, val);

    // A slightly increased roll-off.
    setEAXf(DSPROPERTY_EAXLISTENER_ROOMROLLOFFFACTOR, 1.3f);
}
#endif

/**
 * Call SFXLP_UPDATE at the end of every channel update.
 */
void DS_SFX_Listenerv(int prop, float* values)
{
    if(!values)
        return;

    switch(prop)
    {
    case SFXLP_PRIMARY_FORMAT:
        if(canSetPSF)
            setPrimaryFormat((int) values[0], (int) values[1]);
        break;

    case SFXLP_POSITION:
        if(!dsListener)
            return;
        dsListener->SetPosition(values[VX], values[VZ], values[VY],
                                DS3D_DEFERRED);
        break;

    case SFXLP_VELOCITY:
        if(!dsListener)
            return;
        dsListener->SetVelocity(values[VX], values[VZ], values[VY],
                                DS3D_DEFERRED);
        break;

    case SFXLP_ORIENTATION:
        if(!dsListener)
            return;
        listenerOrientation(values[VX] / 180 * DD_PI, values[VY] / 180 * DD_PI);
        break;

    case SFXLP_REVERB:
#ifdef DE_HAVE_EAX2
        if(!dsListener)
            return;
        listenerEnvironment(values);
#endif
        break;

    default:
        DS_SFX_Listener(prop, 0);
        break;
    }
}

/**
 * Declares the type of the plugin so the engine knows how to treat it. Called
 * automatically when the plugin is loaded.
 */
DE_EXTERN_C const char* deng_LibraryType(void)
{
    return "deng-plugin/audio";
}

DE_API_EXCHANGE(
    DE_GET_API(DE_API_BASE, Base);
    DE_GET_API(DE_API_CONSOLE, Con);
)
