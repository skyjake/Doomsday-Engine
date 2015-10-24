/** @file driver_directsound.cpp  DirectSound (8.0 with EAX 2.0) audio plugin.
 *
 * @note Buffers are created on Load.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2015 Daniel Swanson <danij@dengine.net>
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
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#define DIRECTSOUND_VERSION 0x0800
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <mmsystem.h>
#include <dsound.h>
#ifdef HAVE_EAX
#  include <eax.h>
#endif

#pragma warning (disable: 4035 4244)

#include "doomsday.h"
#include "api_base.h"
#include "api_audiod.h"
#include "api_audiod_sfx.h"

#include <de/App>
#include <de/String>
#include <de/c_wrapper.h>
#include <de/timer.h>
#include <cmath>
#include <cstdlib>

DENG_USING_API(Base);

extern "C" {
int DS_Init(void);
void DS_Shutdown(void);
void DS_Event(int type);

int DS_SFX_Init(void);
sfxbuffer_t *DS_SFX_CreateBuffer(int flags, int bits, int rate);
void DS_SFX_DestroyBuffer(sfxbuffer_t *buf);
void DS_SFX_Load(sfxbuffer_t *buf, struct sfxsample_s *sample);
void DS_SFX_Reset(sfxbuffer_t *buf);
void DS_SFX_Play(sfxbuffer_t *buf);
void DS_SFX_Stop(sfxbuffer_t *buf);
void DS_SFX_Refresh(sfxbuffer_t *buf);
void DS_SFX_Set(sfxbuffer_t *buf, int prop, float value);
void DS_SFX_Setv(sfxbuffer_t *buf, int prop, float *values);
void DS_SFX_Listener(int prop, float value);
void DS_SFX_Listenerv(int prop, float *values);
int DS_SFX_Getv(int prop, void *ptr);
}

using namespace de;

// Accessor utility returning the IDirectSoundBuffer attributed with @a buffer.
static IDirectSoundBuffer &DSBUF(sfxbuffer_t &buffer)
{
    DENG2_ASSERT(buffer.ptr);
    return *reinterpret_cast<IDirectSoundBuffer *>(buffer.ptr);
}

static bool hasDSBUF3D(sfxbuffer_t const &buffer)
{
    return buffer.ptr3D != nullptr;
}

// Accessor utility returning the IDirectSound3DBuffer8 attributed with @a buffer.
static IDirectSound3DBuffer8 &DSBUF3D(sfxbuffer_t &buffer)
{
    DENG2_ASSERT(buffer.ptr3D);
    return *reinterpret_cast<IDirectSound3DBuffer8 *>(buffer.ptr3D);
}

/**
 * Utility for converting a Doomsday world space orientation to the Direct Sound equivalent.
 *
 * @param yaw    World yaw rotation (radians).
 * @param pitch  World pitch rotation (radians).
 */
static void listenerOrientation(dfloat yaw, dfloat pitch, Vector2f &front, Vector2f &up)
{
    // Example front vectors:
    // Yaw 0:(0,0,1), pi/2:(-1,0,0)
    front = Vector3f( cos(yaw) * cos(pitch), sin(pitch),  sin(yaw) * cos(pitch));
    up    = Vector3f(-cos(yaw) * sin(pitch), cos(pitch), -sin(yaw) * sin(pitch));
}

/**
 * Utility for converting linear pan -1..1 to logarithmic -10000..10000.
 */
static dint panLinearToLog(dfloat pan)
{
    if(pan >= 1)  return DSBPAN_RIGHT;
    if(pan <= -1) return DSBPAN_LEFT;
    if(pan == 0)  return 0;

    if(pan > 0)
    {
        return dint( -100 * 20 * log10(1 - pan) );
    }
    else
    {
        return dint(  100 * 20 * log10(1 + pan) );
    }
}

/**
 * Utility for converting linear volume 0..1 to logarithmic -10000..0.
 */
static dint volLinearToLog(dfloat vol)
{
    if(vol <= 0) return DSBVOLUME_MIN;
    if(vol >= 1) return DSBVOLUME_MAX;

    // Straighten the volume curve.
    return de::clamp<dint>(DSBVOLUME_MIN, 100 * 20 * log10(vol), DSBVOLUME_MAX);
}

static HRESULT hr;

static bool initOk;
static bool canSetPSF;
static bool eaxAvailable;

static LPDIRECTSOUND8 dsound;
static LPDIRECTSOUNDBUFFER primary;
static LPDIRECTSOUND3DLISTENER8 listener;

#ifdef HAVE_EAX
static bool eaxEnabled;
static bool eaxIgnoreErrors;
static LPKSPROPERTYSET eaxPropertySet;
static dint const EAX_FAILED_PROPS_MAX = 10;
static DWORD eaxFailedProps[EAX_FAILED_PROPS_MAX];
#endif

/**
 * Debug utility for translating error results returned by the methods of IDirectSound and
 * IDirectSoundBuffer instances to more human-friendly text strings suitable for logging.
 */
static String dsoundErrorToText(HRESULT hr)
{
    if(hr == DS_OK)
    {
        DENG2_ASSERT(!"This is not an error code...");
        return "";
    }

    static struct { HRESULT code; char const *text; } const es[] =
    {
        { DSERR_ALLOCATED,          "ALLOCATED"          },
        { DSERR_ALREADYINITIALIZED, "ALREADYINITIALIZED" },
        { DSERR_BADFORMAT,          "BADFORMAT"          },
        { DSERR_BUFFERLOST,         "BUFFERLOST"         },
        { DSERR_CONTROLUNAVAIL,     "CONTROLUNAVAIL"     },
        { DSERR_GENERIC,            "GENERIC"            },
        { DSERR_INVALIDCALL,        "INVALIDCALL"        },
        { DSERR_INVALIDPARAM,       "INVALIDPARAM"       },
        { DSERR_NOAGGREGATION,      "NOAGGREGATION"      },
        { DSERR_NODRIVER,           "NODRIVER"           },
        { DSERR_NOINTERFACE,        "NOINTERFACE"        },
        { DSERR_OTHERAPPHASPRIO,    "OTHERAPPHASPRIO"    },
        { DSERR_OUTOFMEMORY,        "OUTOFMEMORY"        },
        { DSERR_PRIOLEVELNEEDED,    "PRIOLEVELNEEDED"    },
        { DSERR_UNINITIALIZED,      "UNINITIALIZED"      },
        { DSERR_UNSUPPORTED,        "UNSUPPORTED"        }
    };
    auto const codeAsText = String("(0x%1)").arg(hr, 0, 16);
    for(auto const &p : es)
    {
        if(p.code == hr) return codeAsText + " " + p.text;
    }
    return "Unknown error " + codeAsText;
}

/**
 * @param desc      Waveform (format) descriptor to configure.
 * @param channels  Number of channels (i.e., 1 for mono or 2 for stereo).
 * @param bits      Number of bits per sample.
 * @param rate      Samples per second / frequency (Hz).
 *
 * @return  Same as @a desc for caller convenience.
 */
static WAVEFORMATEX &configureWaveformDesc(WAVEFORMATEX &desc, dint channels, dint bits, dint rate)
{
    channels = de::clamp(1, channels, 2);

    de::zap(desc);
    desc.wFormatTag      = WAVE_FORMAT_PCM;
    desc.nChannels       = channels;
    desc.nSamplesPerSec  = rate;
    desc.wBitsPerSample  = bits;
    desc.nBlockAlign     = channels * bits / 8;
    desc.nAvgBytesPerSec = rate * channels * bits / 8;
    return desc;
}

/**
 * Determine the size of buffer (in bytes) needed for playing a sample with the given
 * configuration.
 */
static dsize bufferSize(dint bits, dint rate)
{
    auto bytes = bits / 8 * rate / 2; // 500ms buffer.
    // Align to an 8 byte boundary.
    dint i = bytes % 8;
    if(i) bytes += 8 - i;
    return bytes;
}

/**
 * @param desc        Buffer descriptor to configure.
 * @param flags       @ref sfxBufferFlags
 * @param bufferSize  Number of bytes to allocate for the playback buffer.
 *
 * @return  Same as @a desc for caller convenience.
 */
static DSBUFFERDESC &configureBufferDesc(DSBUFFERDESC &desc, dint flags, dsize bufferSize)
{
    de::zap(desc);
    desc.dwSize        = sizeof(desc);
    desc.dwFlags       = DSBCAPS_CTRLFREQUENCY
                       | DSBCAPS_CTRLVOLUME
                       | DSBCAPS_STATIC
                       | (flags & SFXBF_3D ? DSBCAPS_CTRL3D : DSBCAPS_CTRLPAN);
    desc.dwBufferBytes = DWORD( bufferSize );
    if(flags & SFXBF_3D)
    {
        /// @todo Control the selection with a Property! -jk
        desc.guid3DAlgorithm = DS3DALG_HRTF_LIGHT;
    }
    return desc;
}

/// Base class for buffer creation errors. @ingroup errors
DENG2_ERROR(CreateBufferError);

/**
 * Attempt to acquire another "secondary" Direct Sound buffer configured according to the
 * given format @a descriptor.
 */
static IDirectSoundBuffer8 *makeBuffer(DSBUFFERDESC &descriptor)
{
    // Try to create a secondary buffer with the requested properties.
    IDirectSoundBuffer *buf;
    if((hr = dsound->CreateSoundBuffer(&descriptor, &buf, nullptr)) != DS_OK)
    {
        /// @throw CreateBufferError  Invalid buffer format description.
        throw CreateBufferError("makeBuffer", "Failed creating buffer - Error: " + dsoundErrorToText(hr));
    }

    // Acquire the DirectSoundBuffer8 interface.
    IDirectSoundBuffer8 *buf8;
    if((hr = buf->QueryInterface(IID_IDirectSoundBuffer8, (LPVOID *) &buf8)) != DS_OK)
    {
        // Release the old interface, we don't need it.
        buf->Release();

        /// @throw CreateBufferError  The desired interface is unavailable.
        throw CreateBufferError("makeBuffer", "Failed acquring interface - Error: " + dsoundErrorToText(hr));
    }

    // Release the old interface, we don't need it.
    buf->Release();
    return buf8;
}

/**
 * Attempt to aquire a "3D" interface for spatial positioning from the given @a buffer.
 */
static IDirectSound3DBuffer8 *get3DBuffer(IDirectSoundBuffer8 &buffer)
{
    IDirectSound3DBuffer8 *found;
    if(FAILED(hr = buffer.QueryInterface(IID_IDirectSound3DBuffer8, (LPVOID *) &found)))
    {
        LOG_AS("[DirectSound]");
        LOG_AUDIO_WARNING("get3DBuffer: Failed acquiring interface:\n") << dsoundErrorToText(hr);
        return nullptr;
    }
    return found;
}

#ifdef HAVE_EAX

#if 0 // Unused.
/**
 * Utility for translating Direct Sound EAX Listener property identifiers to human-friendly
 * text strings suitable for logging.
 */
static String eaxListenerPropertyToText(DSPROPERTY_EAX_LISTENERPROPERTY prop)
{
    static struct { DSPROPERTY_EAX_LISTENERPROPERTY prop; char const *text; } const ps[] =
    {
        { DSPROPERTY_EAXLISTENER_ENVIRONMENT,       "Environment" },
        { DSPROPERTY_EAXLISTENER_ROOM,              "Room"        },
        { DSPROPERTY_EAXLISTENER_ROOMHF,            "Room HF"     },
        { DSPROPERTY_EAXLISTENER_DECAYTIME,         "Decay time"  },
        { DSPROPERTY_EAXLISTENER_ROOMROLLOFFFACTOR, "Room roll-off factor" },
    };
    for(auto const &p : ps)
    {
        if(p.prop == prop) return p.text;
    }
    return "Unknown property " + String::number(prop);
}
#endif

/**
 * Does the EAX implementation support getting/setting of a propertry.
 *
 * @param prop  Property id (constant) to be checked.
 * @return  @c true if supported.
 */
static bool queryEAXSupport(dint prop)
{
#define EAXSUP          ( KSPROPERTY_SUPPORT_GET | KSPROPERTY_SUPPORT_SET )

    if(!::eaxPropertySet) return false;

    ULONG support = 0;
    ::eaxPropertySet->QuerySupport(DSPROPSETID_EAX_ListenerProperties, prop, &support);
    return (support & EAXSUP) == EAXSUP;

#undef EAXSUP
}

/**
 * Returns @c true if the specified property has failed.
 */
static bool hasEAXFailed(DWORD prop)
{
    for(dint i = 0; i < EAX_FAILED_PROPS_MAX; ++i)
    {
        if(::eaxFailedProps[i] == prop)
            return true;
    }
    return false;
}

/**
 * Set the property as 'failed'. No more errors are reported for it.
 */
static void setEAXFailed(DWORD prop)
{
    for(dint i = 0; i < EAX_FAILED_PROPS_MAX; ++i)
    {
        if(::eaxFailedProps[i] == ~0)
        {
            ::eaxFailedProps[i] = prop;
            break;
        }
    }
}

/**
 * Returns @c true if the EAX error should be reported.
 */
static bool reportEAXError(DWORD prop, HRESULT hr)
{
    if(::eaxIgnoreErrors) return false;

    if(hr != DSERR_UNSUPPORTED)
        return true;

    if(hasEAXFailed(prop))
        return false;  // Don't report again.

    setEAXFailed(prop);

    return true;  // First time, do report.
}

static void commitEAXDeferred()
{
    if(!::eaxPropertySet) return;

    ::eaxPropertySet->Set(DSPROPSETID_EAX_ListenerProperties,
                          DSPROPERTY_EAXLISTENER_COMMITDEFERREDSETTINGS, nullptr,
                          0, nullptr, 0);
}

/**
 * Translate a Doomsday audio environment to a suitable EAX environment type.
 */
static dint eaxEnvironment(dfloat space, dfloat decay = 0.f)
{
    if(decay > .5)
    {
        // This much decay needs at least the Generic environment.
        if(space < .2)
            space = .2f;
    }

    if(space >= 1.0f) return EAX_ENVIRONMENT_PLAIN;
    if(space >= 0.8f) return EAX_ENVIRONMENT_CONCERTHALL;
    if(space >= 0.6f) return EAX_ENVIRONMENT_AUDITORIUM;
    if(space >= 0.4f) return EAX_ENVIRONMENT_CAVE;
    if(space >= 0.2f) return EAX_ENVIRONMENT_GENERIC;

    return EAX_ENVIRONMENT_ROOM;
}

static void setEAXdw(DWORD prop, dint value)
{
    if(FAILED(hr = ::eaxPropertySet->Set(DSPROPSETID_EAX_ListenerProperties,
                                         prop | DSPROPERTY_EAXLISTENER_DEFERRED,
                                         nullptr, 0, &value, sizeof(DWORD))))
    {
        if(reportEAXError(prop, hr))
        {
            auto const errorAsText = String("Error: (0x%1)").arg(hr, 0, 16);
            LOG_AS("[DirectSound]");
            LOGDEV_AUDIO_WARNING("setEAXdw (prop:%i value:%i) failed:\n")
                << prop << value << errorAsText;
        }
    }
}

static void setEAXf(DWORD prop, dfloat value)
{
    if(FAILED(hr = ::eaxPropertySet->Set(DSPROPSETID_EAX_ListenerProperties,
                                         prop | DSPROPERTY_EAXLISTENER_DEFERRED,
                                         nullptr, 0, &value, sizeof(dfloat))))
    {
        if(reportEAXError(prop, hr))
        {
            auto const errorAsText = String("Error: (0x%1").arg(hr, 0, 16);
            LOG_AS("[DirectSound]");
            LOGDEV_AUDIO_WARNING("setEAXf (prop:%i value:%f) failed:\n")
                << prop << value << errorAsText;
        }
    }
}

/**
 * Linear multiplication for a logarithmic property.
 */
static void mulEAXdw(DWORD prop, dfloat mul)
{
    LONG value;
    DWORD retBytes;
    if(FAILED(hr = ::eaxPropertySet->Get(DSPROPSETID_EAX_ListenerProperties,
                                         prop, nullptr, 0, &value, sizeof(value),
                                         &retBytes)))
    {
        if(reportEAXError(prop, hr))
        {
            auto const errorAsText = String("Error: (0x%1").arg(hr, 0, 16);
            LOG_AS("[DirectSound]");
            LOGDEV_AUDIO_WARNING("mulEAXdw (prop:%i) get failed:\n")
                << prop << errorAsText;
        }
    }
    setEAXdw(prop, volLinearToLog(pow(10, value / 2000.0f) * mul));
}

/**
 * Linear multiplication for a linear property.
 */
static void mulEAXf(DWORD prop, dfloat mul, dfloat min, dfloat max)
{
    dfloat value;
    DWORD retBytes;
    if(FAILED(hr = ::eaxPropertySet->Get(DSPROPSETID_EAX_ListenerProperties,
                                         prop, nullptr, 0, &value, sizeof(value),
                                         &retBytes)))
    {
        if(reportEAXError(prop, hr))
        {
            auto const errorAsText = String("Error: (0x%1").arg(hr, 0, 16);
            LOG_AS("[DirectSound]");
            LOGDEV_AUDIO_WARNING("mulEAXf (prop:%i) get failed:\n")
                << prop << errorAsText;
        }
    }
    setEAXf(prop, de::clamp(min, value * mul, max));
}

#endif  // HAVE_EAX

/**
 * Init DirectSound, start playing the primary buffer.
 *
 * @return  @c true if successful.
 */
int DS_Init()
{
    // Already been here?
    if(::dsound) return true;

    LOG_AUDIO_VERBOSE("Initializing Direct Sound...");

    ::dsound    = nullptr;
    ::listener  = nullptr;
    ::canSetPSF = !DENG2_APP->commandLine().has("-nopsf");

    ::eaxAvailable    = false;
#ifdef HAVE_EAX
    ::eaxEnabled      = !DENG2_APP->commandLine().has("-noeax");
    ::eaxIgnoreErrors = DENG2_APP->commandLine().has("-eaxignore");
    ::eaxPropertySet  = nullptr;
    std::memset(::eaxFailedProps, ~0, sizeof(::eaxFailedProps));
#endif

    auto hWnd = (HWND) DD_GetVariable(DD_WINDOW_HANDLE);
    if(!hWnd)
    {
        LOG_AS("[DirectSound]");
        LOG_AUDIO_ERROR("Main window unavailable - cannot initialize");
        return false;
    }

#ifdef HAVE_EAX
    // First try to create the DirectSound8 object with EAX support.
    if((hr = EAXDirectSoundCreate8(nullptr, &::dsound, nullptr)) == DS_OK)
    {
        ::eaxAvailable = true;
    }
    else
    {
        LOG_AS("[DirectSound]");
        LOG_AUDIO_VERBOSE("EAX could not be initialized: ")
            << String("(0x%1)").arg(hr, 0, 16);
    }
#endif

    // Try plain old DS, then.
    if(!::dsound)
    {
        if((hr = DirectSoundCreate8(nullptr, &::dsound, nullptr)) != DS_OK)
        {
            LOG_AS("[DirectSound]");
            LOG_AUDIO_ERROR("Failed to create the DS8 instance:\n")
                << dsoundErrorToText(hr);
        }
    }

    // Still no interface?
    if(!::dsound) return false;  // Give up.

    // Set cooperative level.
    if((hr = ::dsound->SetCooperativeLevel(hWnd, DSSCL_PRIORITY)) != DS_OK)
    {
        LOG_AS("[DirectSound]");
        LOG_AUDIO_ERROR("Failed to set cooperative level:\n")
            << dsoundErrorToText(hr);

        /// @todo fixme: release all acquired resources! -ds
        return false;
    }

    // Lets query the device caps.
    DSCAPS dsoundCaps;
    dsoundCaps.dwSize = sizeof(dsoundCaps);
    if((hr = ::dsound->GetCaps(&dsoundCaps)) != DS_OK)
    {
        LOG_AS("[DirectSound]");
        LOG_AUDIO_ERROR("Failed querying device caps:\n")
            << dsoundErrorToText(hr);

        /// @todo fixme: release all acquired resources! -ds
        return false;
    }
#ifdef HAVE_EAX
    if(::eaxAvailable && duint( dsoundCaps.dwFreeHw3DStreamingBuffers ) < 4)
    {
        LOG_AS("[DirectSound]");
        LOG_AUDIO_NOTE("Insufficient 3D sound buffers - EAX disabled");
        ::eaxEnabled = false;
    }
#endif

    /*
     * Create the primary buffer.
     * We prioritize buffer creation as follows:
     * 3D hardware > 3D software > 2D hardware > 2D software.
     */
    bool primaryBuffer3D = false;
    bool primaryBufferHW = false;

    DSBUFFERDESC desc; ZeroMemory(&desc, sizeof(DSBUFFERDESC));
    desc.dwSize = sizeof(DSBUFFERDESC);

    // First try for a 3D buffer, hardware or software.
    desc.dwFlags = DSBCAPS_PRIMARYBUFFER
                 | DSBCAPS_CTRL3D
                 | DSBCAPS_CTRLVOLUME
                 | (duint( dsoundCaps.dwFreeHw3DStreamingBuffers ) > 0 ? DSBCAPS_LOCHARDWARE
                                                                       : DSBCAPS_LOCSOFTWARE);

    hr = ::dsound->CreateSoundBuffer(&desc, &primary, nullptr);
    if(hr != DS_OK && hr != DS_NO_VIRTUALIZATION)
    {   
        // Not available.
        // Try for a 2D buffer.
        ZeroMemory(&desc, sizeof(DSBUFFERDESC));
        desc.dwSize  = sizeof(DSBUFFERDESC);
        desc.dwFlags = DSBCAPS_PRIMARYBUFFER
                     | DSBCAPS_CTRLVOLUME
                     | (duint( dsoundCaps.dwFreeHwMixingStreamingBuffers ) > 0 ? DSBCAPS_LOCHARDWARE
                                                                               : DSBCAPS_LOCSOFTWARE);

        if((hr = ::dsound->CreateSoundBuffer(&desc, &primary, nullptr)) != DS_OK)
        {
            LOG_AS("[DirectSound]");
            LOG_AUDIO_ERROR("Failed creating primary (2D) buffer:\n")
                << dsoundErrorToText(hr);

            /// @todo fixme: release all acquired resources! -ds
            return false;
        }

        primaryBufferHW = duint( dsoundCaps.dwFreeHwMixingStreamingBuffers ) > 0;
    }
    else
    {   
        // 3D buffer available.
        primaryBuffer3D = true;
        primaryBufferHW = duint( dsoundCaps.dwFreeHw3DStreamingBuffers ) > 0;

        // Get the listener.
        if(FAILED(hr = ::primary->QueryInterface(IID_IDirectSound3DListener, (LPVOID *) &::listener)))
        {
            LOG_AS("[DirectSound]");
            LOGDEV_AUDIO_MSG("3D listener not available:\n")
                << dsoundErrorToText(hr);

            /// @todo Does this plugin really work without a listener? -ds
        }
    }

    // Start playing the primary buffer.
    if(::primary)
    {
        // Supposedly can be a bit more efficient not to stop the primary buffer when there
        // are no secondary buffers playing.
        ::primary->Play(0, 0, DSBPLAY_LOOPING);
    }

#ifdef HAVE_EAX
    if(::eaxEnabled)
    {
        try
        {
            // We expect minimum set of listener properties to use EAX.
            // Check availability using a temporary, secondary buffer.
            WAVEFORMATEX wave; ZeroMemory(&wave, sizeof(WAVEFORMATEX));
            wave.wFormatTag      = WAVE_FORMAT_PCM;
            wave.nChannels       = 1;
            wave.nSamplesPerSec  = 44100;
            wave.wBitsPerSample  = 16;
            wave.nBlockAlign     = 2;
            wave.nAvgBytesPerSec = 88200;

            DSBUFFERDESC desc; ZeroMemory(&desc, sizeof(DSBUFFERDESC));
            desc.dwSize        = sizeof(DSBUFFERDESC);
            desc.dwBufferBytes = DSBSIZE_MIN;
            desc.dwFlags       = DSBCAPS_STATIC | DSBCAPS_CTRL3D;
            desc.lpwfxFormat   = &wave;

            IDirectSoundBuffer8 *dummy = makeBuffer(desc);

            // Get the 3D interface.
            if(IDirectSound3DBuffer8 *dummy3d = get3DBuffer(*dummy))
            {
                // Query the property set interface.
                dummy3d->QueryInterface(IID_IKsPropertySet, (LPVOID *) &::eaxPropertySet);
                if(!::eaxPropertySet)
                {
                    LOG_AS("[DirectSound]");
                    LOGDEV_AUDIO_WARNING("Failed retrieving property set");
                    ::eaxEnabled = false;
                }
                // We require at least the following Listener capabilities:
                else if(   !queryEAXSupport(DSPROPERTY_EAXLISTENER_ENVIRONMENT)
                        || !queryEAXSupport(DSPROPERTY_EAXLISTENER_ROOM)
                        || !queryEAXSupport(DSPROPERTY_EAXLISTENER_ROOMHF)
                        || !queryEAXSupport(DSPROPERTY_EAXLISTENER_DECAYTIME)
                        || !queryEAXSupport(DSPROPERTY_EAXLISTENER_ROOMROLLOFFFACTOR))
                {
                    // Sorry, no good.
                    ::eaxPropertySet->Release();
                    ::eaxPropertySet = nullptr;
                    ::eaxEnabled = false;
                }
            }

            dummy->Release();
        }
        catch(CreateBufferError const &er)
        {
            LOG_AS("[DirectSound]");
            LOG_AUDIO_WARNING("") << er.asText();
            ::eaxEnabled = false;
        }
    }
#endif  // HAVE_EAX

    // Log an overview of the DirectSound configuration:
#define TABBED(A, B)  _E(Ta) "  " _E(l) A _E(.) " " _E(Tb) << B << "\n"

    String str;
    QTextStream os(&str);

    os << _E(b) "DirectSound information:\n" << _E(.);

    os << TABBED("Primary Buffer:", String("%1 (%2)").arg(primaryBuffer3D ? "3D" : "2D")
                                                     .arg(primaryBufferHW ? "hardware" : "software"));
    os << TABBED("Hardware Buffers:", (primaryBuffer3D ? duint( dsoundCaps.dwFreeHw3DStreamingBuffers )
                                                       : duint( dsoundCaps.dwFreeHwMixingStreamingBuffers )));
    String environmentModel = ::eaxAvailable ? "EAX 2.0" : "None";
#ifdef HAVE_EAX
    if(::eaxAvailable && !::eaxEnabled)
        environmentModel += " (disabled)";
#endif
    os << TABBED("Environment model:", environmentModel);

    LOG_AUDIO_MSG(str.rightStrip());

#undef TABBED

    // Everything is OK.
    return true;
}

/**
 * Shut everything down.
 */
void DS_Shutdown()
{
#ifdef HAVE_EAX
    if(::eaxPropertySet)
        ::eaxPropertySet->Release();
    ::eaxPropertySet = nullptr;
#endif

    if(::listener)
        ::listener->Release();
    ::listener = nullptr;

    if(::primary)
        ::primary->Release();
    ::primary = nullptr;

    if(::dsound)
        ::dsound->Release();
    ::dsound = nullptr;
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
        if(idKey) Str_Set(idKey, "directsound;dsound");
        return true; }

    case AUDIOP_TITLE: {
        auto *title = reinterpret_cast<AutoStr *>(ptr);
        DENG2_ASSERT(title);
        if(title) Str_Set(title, "DirectSound");
        return true; }

    default: DENG2_ASSERT("[DirectSound]DS_Get: Unknown property"); break;
    }
    return false;
}

int DS_SFX_Init()
{
    return true;
}

sfxbuffer_t *DS_SFX_CreateBuffer(int flags, int bits, int rate)
{
    // If we don't have the listener, the primary buffer doesn't have 3D capabilities;
    // don't create 3D buffers. DSound should provide software emulation, though, so this
    // is really only a contingency.
    if(!::listener && (flags & SFXBF_3D))
        return nullptr;

    try
    {
        // Setup the buffer descriptor.
        DSBUFFERDESC desc;
        configureBufferDesc(desc, flags, bufferSize(bits, rate));
        WAVEFORMATEX format;
        desc.lpwfxFormat = &configureWaveformDesc(format, 1, bits, rate);

        IDirectSoundBuffer8 *buf_object8 = makeBuffer(desc);

        // How about a 3D interface?
        IDirectSound3DBuffer8 *buf_object3d = nullptr;
        if(flags & SFXBF_3D)
        {
            buf_object3d = get3DBuffer(*buf_object8);
            if(!buf_object3d)
            {
                LOG_AS("[DirectSound]");
                LOG_AUDIO_WARNING("Failed to get a 3D interface for audio buffer");

                buf_object8->Release();
                return nullptr;
            }
        }

        // Allocate and configure another (shared) Doomsday sound buffer.
        sfxbuffer_t *buf = (sfxbuffer_t *) Z_Calloc(sizeof(*buf), PU_APPSTATIC, 0);
        buf->ptr    = buf_object8;
        buf->ptr3D  = buf_object3d;
        buf->flags  = flags;

        buf->bytes  = bits / 8;
        buf->freq   = rate;      // Modified by calls to Set(SFXBP_FREQUENCY).
        buf->rate   = rate;
        buf->length = duint( desc.dwBufferBytes );

        return buf;
    }
    catch(CreateBufferError const &er)
    {
        LOG_AS("[DirectSound]");
        LOG_AUDIO_WARNING("Failed creating buffer (rate:%i bits:%i):\n")
            << rate << bits
            << er.asText();
    }

    return nullptr;
}

void DS_SFX_DestroyBuffer(sfxbuffer_t *buf)
{
    if(!buf) return;

    DSBUF(*buf).Release();
    // Free the memory allocated for the buffer.
    Z_Free(buf);
}

/**
 * Prepare the buffer for playing a sample by filling the buffer with as much sample data
 * as fits. The pointer to sample is saved, so the caller mustn't free it while the sample
 * is loaded.
 */
void DS_SFX_Load(sfxbuffer_t *buf, sfxsample_t *sample)
{
    if(!buf || !sample) return;

    // Try to lock the buffer.
    void *data;
    DWORD lockedBytes;
    if(FAILED(hr = DSBUF(*buf).Lock(0, 0, &data, &lockedBytes, 0, 0, DSBLOCK_ENTIREBUFFER)))
        return;  // Couldn't lock!

    // Write as much data as we can.
    auto const wroteBytes = de::min<DWORD>(lockedBytes, sample->size);
    std::memcpy(data, sample->data, wroteBytes);

    // Fill the rest with zeroes.
    if(wroteBytes < lockedBytes)
    {
        // Set the end marker since we already know it.
        buf->cursor = wroteBytes;
        std::memset((char *) data + wroteBytes, buf->bytes == 1 ? 128 : 0,
                    lockedBytes - wroteBytes);
    }
    else
    {
        // The whole buffer was filled, thus leaving the cursor at
        // the beginning.
        buf->cursor = 0;
    }

    DSBUF(*buf).Unlock(data, lockedBytes, 0, 0);

    // Now the buffer is ready for playing.
    buf->sample  = sample;
    buf->written = wroteBytes;
    buf->flags  &= ~SFXBF_RELOAD;

    // Zero the play cursor.
    DSBUF(*buf).SetCurrentPosition(0);
}

/**
 * Stops the buffer and makes it forget about its sample.
 */
void DS_SFX_Reset(sfxbuffer_t *buf)
{
    if(!buf) return;

    DS_SFX_Stop(buf);
    buf->sample = nullptr;
    buf->flags &= ~SFXBF_RELOAD;
}

void DS_SFX_Play(sfxbuffer_t *buf)
{
    // Playing is quite impossible without a sample.
    if(!buf || !buf->sample) return;

    // Do we need to reload?
    if(buf->flags & SFXBF_RELOAD)
        DS_SFX_Load(buf, buf->sample);

    // The sound starts playing now?
    if(!(buf->flags & SFXBF_PLAYING))
    {
        // Calculate the end time (milliseconds).
        buf->endTime = Timer_RealMilliseconds() + buf->milliseconds();
    }

    if(FAILED(hr = DSBUF(*buf).Play(0, 0, DSBPLAY_LOOPING)))
        return;

    // The buffer is now playing.
    buf->flags |= SFXBF_PLAYING;
}

void DS_SFX_Stop(sfxbuffer_t *buf)
{
    if(!buf) return;

    DSBUF(*buf).Stop();

    // Clear the flag that tells the Sfx module about playing buffers.
    buf->flags &= ~SFXBF_PLAYING;

    // If the sound is started again, it needs to be reloaded.
    buf->flags |= SFXBF_RELOAD;
}

/**
 * Buffer streamer. Called by the Sfx refresh thread. Copy sample data into the buffer,
 * and if the sample has ended, stop playing the buffer. If the buffer has been lost for
 * some reason, restore it.
 */
void DS_SFX_Refresh(sfxbuffer_t *buf)
{
    /// @note Don't do anything time-consuming...

    if(!buf) return;

    // Can only be done if there is a sample and the buffer is playing.
    if(!buf->sample || !(buf->flags & SFXBF_PLAYING)) return;

    duint const nowTime = Timer_RealMilliseconds();

    /**
     * Have we passed the predicted end of sample?
     * @note This test fails if the game has been running for about 50 days, since the
     * millisecond counter overflows. It only affects sounds that are playing while the
     * overflow happens, though.
     */
    if(!(buf->flags & SFXBF_REPEAT) && nowTime >= buf->endTime)
    {
        // Time for the sound to stop.
        DS_SFX_Stop(buf);
        return;
    }

    // Slightly redundant... (used = now - start)
    duint const usedTime = nowTime - (buf->endTime - buf->milliseconds());

    // Approximate the current playing position (-0.1 sec for safety; we don't want to
    // overwrite stuff before it gets played).
    dfloat const usedSec = usedTime / 1000.0f - 0.1f;

    // To early for an update?
    if(usedSec <= 0) return;  // Let's wait for the next one.

    DWORD play = dint( usedSec * buf->freq * buf->bytes ) % buf->length;

    // How many bytes we must write (from buffer cursor up to play cursor)?
    dint writeBytes;
    if(buf->cursor < play)
    {
        writeBytes = play - buf->cursor;
    }
    // Play has looped back to the beginning.
    else
    {
        writeBytes = buf->length - buf->cursor + play;
    }

    // Try to lock the region, restoring if failed.
    void *data[2];
    DWORD bytes[2];
    for(dint i = 0; i < 2; ++i)
    {
        if(FAILED(hr = DSBUF(*buf).Lock(buf->cursor, writeBytes, &data[0], &bytes[0],
                                        &data[1], &bytes[1], 0)))
        {
            if(hr == DSERR_BUFFERLOST)
            {
                DSBUF(*buf).Restore();
                continue;
            }
        }
        break;
    }

    if(FAILED(hr))
        return;  // Give up.

    // Copy in two parts: as much sample data as we've got, and then zeros.
    for(dint i = 0; i < 2 && data[i]; ++i)
    {
        // The dose is limited to the number of bytes we can write to this pointer and
        // the number of bytes we've got left.
        auto const dose = de::min<DWORD>(bytes[i], buf->sample->size - buf->written);
        if(dose)
        {
            // Copy from the sample data and advance cursor & written.
            std::memcpy((byte *) data[i], (byte *) buf->sample->data + buf->written, dose);
            buf->written += dose;
            buf->cursor += dose;
        }

        if(dose < bytes[i])
        {
            // Repeating samples just rewind the 'written' counter when the end is reached.
            if(!(buf->flags & SFXBF_REPEAT))
            {
                // The whole block was not filled. Write zeros in the rest.
                DWORD const fill = bytes[i] - dose;
                // Filling an 8-bit sample with zeroes produces a nasty click.
                std::memset((byte *) data[i] + dose, buf->bytes == 1 ? 128 : 0, fill);
                buf->cursor += fill;
            }
        }

        // Wrap the cursor back to the beginning if needed. The wrap can only happen after
        // the first write, really (where the buffer "breaks").
        if(buf->cursor >= buf->length)
            buf->cursor -= buf->length;
    }

    // And we're done! Unlock and get out of here.
    DSBUF(*buf).Unlock(data[0], bytes[0], data[1], bytes[1]);

    // If the buffer is in repeat mode, go back to the beginning once the end is reached.
    if((buf->flags & SFXBF_REPEAT) && buf->written == buf->sample->size)
    {
        buf->written = 0;
    }
}

/**
 * @param prop  - SFXBP_VOLUME (if negative, interpreted as attenuation)
 *              - SFXBP_FREQUENCY
 *              - SFXBP_PAN (-1..1)
 *              - SFXBP_MIN_DISTANCE
 *              - SFXBP_MAX_DISTANCE
 *              - SFXBP_RELATIVE_MODE
 */
void DS_SFX_Set(sfxbuffer_t *buf, int prop, float value)
{
    if(!buf) return;

    switch(prop)
    {
    case SFXBP_VOLUME: {
        LONG volume;
        // Use logarithmic attenuation?
        if(value <= 0)
        {
            volume = LONG( (-1 - value) * 10000 );
        }
        // Linear volume.
        else
        {
            volume = LONG( volLinearToLog(value) );
        }

        DSBUF(*buf).SetVolume(volume);
        break; }

    case SFXBP_FREQUENCY: {
        auto const freq = duint( buf->rate * value );
        // Don't set redundantly.
        if(freq != buf->freq)
        {
            buf->freq = freq;
            DSBUF(*buf).SetFrequency(freq);
        }
        break; }

    case SFXBP_PAN:
        DSBUF(*buf).SetPan(panLinearToLog(value));
        break;

    case SFXBP_MIN_DISTANCE:
        if(hasDSBUF3D(*buf))
        {
            DSBUF3D(*buf).SetMinDistance(value, DS3D_DEFERRED);
        }
        break;

    case SFXBP_MAX_DISTANCE:
        if(hasDSBUF3D(*buf))
        {
            DSBUF3D(*buf).SetMaxDistance(value, DS3D_DEFERRED);
        }
        break;

    case SFXBP_RELATIVE_MODE:
        if(hasDSBUF3D(*buf))
        {
            DSBUF3D(*buf).SetMode(value ? DS3DMODE_HEADRELATIVE : DS3DMODE_NORMAL, DS3D_DEFERRED);
        }
        break;

    default:
        LOG_AS("[DirectSound]");
        LOGDEV_AUDIO_ERROR("DS_SFX_Set: Unknown prop %i") << prop;
        break;
    }
}

/**
 * Coordinates specified in world coordinate system, converted to DSound's:
 * +X to the right, +Y up and +Z away (Y and Z swapped, i.e.).
 *
 * @param prop  - SFXBP_POSITION
 *              - SFXBP_VELOCITY
 */
void DS_SFX_Setv(sfxbuffer_t *buf, int prop, float *values)
{
    if(!buf || !values) return;

    if(!hasDSBUF3D(*buf)) return;

    switch(prop)
    {
    case SFXBP_POSITION:
        DSBUF3D(*buf).SetPosition(values[0], values[2], values[1], DS3D_DEFERRED);
        break;

    case SFXBP_VELOCITY:
        DSBUF3D(*buf).SetVelocity(values[0], values[2], values[1], DS3D_DEFERRED);
        break;

    default:
        LOG_AS("[DirectSound]");
        LOGDEV_AUDIO_ERROR("DS_SFX_Setv: Unknown prop %i") << prop;
        break;
    }
}

/**
 * Set a property of a listener.
 *
 * @param prop   - SFXLP_UNITS_PER_METER
 *               - SFXLP_DOPPLER
 *               - SFXLP_UPDATE
 * @param value  Value to be set.
 */
void DS_SFX_Listener(int prop, float value)
{
    if(!::listener) return;

    switch(prop)
    {
    case SFXLP_UPDATE:
        // Commit any deferred settings.
        ::listener->CommitDeferredSettings();
#ifdef HAVE_EAX
        if(::eaxEnabled)
        {
            commitEAXDeferred();
        }
#endif
        break;

    case SFXLP_UNITS_PER_METER:
        ::listener->SetDistanceFactor(1 / value, DS3D_IMMEDIATE);
        break;

    case SFXLP_DOPPLER:
        ::listener->SetDopplerFactor(value, DS3D_IMMEDIATE);
        break;

    default:
        LOG_AS("[DirectSound]");
        LOGDEV_AUDIO_ERROR("DS_SFX_Listener: Unknown prop %i") << prop;
        break;
    }
}

/**
 * Call SFXLP_UPDATE at the end of every channel update.
 */
void DS_SFX_Listenerv(int prop, float *values)
{
    if(!values) return;

    switch(prop)
    {
    case SFXLP_PRIMARY_FORMAT:
        if(::canSetPSF && ::primary)
        {
            auto const bits = dint( values[0] );
            auto const rate = dint( values[1] );

            WAVEFORMATEX desc;
            configureWaveformDesc(desc, 2/*channels*/, bits, rate);

            if((hr = ::primary->SetFormat(&desc)) != DS_OK)
            {
                LOG_AS("[DirectSound]");
                LOG_AUDIO_WARNING("Failing setting primary buffer format (bits:%i rate:%i):\n")
                    << bits << rate << dsoundErrorToText(hr);
            }
        }
        break;

    case SFXLP_POSITION:
        if(::listener)
        {
            ::listener->SetPosition(values[0], values[2], values[1], DS3D_DEFERRED);
        }
        break;

    case SFXLP_VELOCITY:
        if(::listener)
        {
            ::listener->SetVelocity(values[0], values[2], values[1], DS3D_DEFERRED);
        }
        break;

    case SFXLP_ORIENTATION:
        if(::listener)
        {
            Vector3f front, up;
            listenerOrientation(values[0] / 180 * DD_PI, values[1] / 180 * DD_PI, front, up);
            listener->SetOrientation(front.x, front.y, front.z, up.x, up.y, up.z, DS3D_DEFERRED);
        }
        break;

#ifdef HAVE_EAX
    case SFXLP_REVERB:
        // If EAX is available, set the listening environmental properties.
        if(::listener && ::eaxPropertySet)
        {
            // @var values  Uses SRD_* for indices.
            dfloat const *env = values;

            // Set the environment.
            setEAXdw(DSPROPERTY_EAXLISTENER_ENVIRONMENT, eaxEnvironment(env[SRD_SPACE], env[SRD_DECAY]));

            // General reverb volume adjustment.
            setEAXdw(DSPROPERTY_EAXLISTENER_ROOM, volLinearToLog(env[SRD_VOLUME]));

            // Reverb decay.
            mulEAXf(DSPROPERTY_EAXLISTENER_DECAYTIME, (env[SRD_DECAY] - .5f) * 1.5f + 1,
                    EAXLISTENER_MINDECAYTIME, EAXLISTENER_MAXDECAYTIME);

            // Damping.
            mulEAXdw(DSPROPERTY_EAXLISTENER_ROOMHF, de::max(.1f, 1.1f * (1.2f - env[SRD_DAMPING])));

            // A slightly increased roll-off.
            setEAXf(DSPROPERTY_EAXLISTENER_ROOMROLLOFFFACTOR, 1.3f);
        }
        break;
#endif

    default:
        DS_SFX_Listener(prop, 0);
        break;
    }
}

int DS_SFX_Getv(int prop, void *ptr)
{
    switch(prop)
    {
    case SFXIP_IDENTITYKEY: {
        auto *identityKey = reinterpret_cast<char *>(ptr);
        if(identityKey)
        {
            qstrcpy(identityKey, "sfx");
            return true;
        }
        break; }

    default: break;
    }

    return false;
}

/**
 * Declares the type of the plugin so the engine knows how to treat it. Called
 * automatically when the plugin is loaded.
 */
DENG_EXTERN_C char const *deng_LibraryType()
{
    return "deng-plugin/audio";
}

DENG_DECLARE_API(Base);
DENG_DECLARE_API(Con);

DENG_API_EXCHANGE(
    DENG_GET_API(DE_API_BASE, Base);
    DENG_GET_API(DE_API_CONSOLE, Con);
)
