/** @file driver_openal.cpp  OpenAL audio plugin.
 *
 * @bug Not 64bit clean: In function 'DS_SFX_CreateBuffer': cast to pointer from integer of different size
 * @bug Not 64bit clean: In function 'DS_SFX_DestroyBuffer': cast to pointer from integer of different size
 * @bug Not 64bit clean: In function 'DS_SFX_Play': cast to pointer from integer of different size
 * @bug Not 64bit clean: In function 'DS_SFX_Stop': cast to pointer from integer of different size
 * @bug Not 64bit clean: In function 'DS_SFX_Refresh': cast to pointer from integer of different size
 * @bug Not 64bit clean: In function 'DS_SFX_Set': cast to pointer from integer of different size
 * @bug Not 64bit clean: In function 'DS_SFX_Setv': cast to pointer from integer of different size
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

#ifdef WIN32
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#endif

#ifdef MSVC
#  pragma warning (disable: 4244)
#endif

#ifdef HAVE_AL_H
#  include <al.h>
#  include <alc.h>
#else
#  include <AL/al.h>
#  include <AL/alc.h>
#endif
#ifdef HAVE_EAX
#  include <eax.h>
#endif

#include "doomsday.h"
#include "api_audiod.h"
#include "api_audiod_sfx.h"

#include <de/App>
#include <de/Error>
#include <de/Log>
#include <QStringList>
#include <QTextStream>

#include <de/memoryzone.h>  /// @todo remove me
#include <cstdio>
#include <iostream>
#include <cstring>
#include <cmath>

// Doomsday expects symbols to be exported without mangling.
extern "C" {

int DS_Init(void);
void DS_Shutdown(void);
void DS_Event(int type);
int DS_Get(int prop, void *ptr);

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

}  // extern "C"

using namespace de;

#define SRC(buf) ( (ALuint) PTR2INT(buf->ptr3D) )
#define BUF(buf) ( (ALuint) PTR2INT(buf->ptr)   )

static bool initOk;
static bool eaxAvailable;

#ifdef HAVE_EAX
static bool eaxDisabled;

static EAXGet alEAXGet;
static EAXSet alEAXSet;
#endif

static dfloat headYaw, headPitch;  ///< In radians.
static dfloat unitsPerMeter;

static ALCdevice *device;
static ALCcontext *context;

static ALenum ec;

static String alErrorToText(ALenum errorCode, String const &file = "", dint line = -1)
{
    if(errorCode == AL_NO_ERROR)
    {
        DENG2_ASSERT(!"This is not an error code...");
        return "";
    }
    auto text = String("(0x%1)").arg(errorCode, 0, 16) + " " + alGetString(errorCode);
    if(!file.isEmpty())
    {
        text += " at " + file + ", line " + String::number(line);
    }
    return text;
}

#ifdef DENG2_DEBUG
#define alErrorToText(errorcode) alErrorToText((errorcode), __FILE__, __LINE__)
#endif

static void logAvailableDevices()
{
    if(!alcIsExtensionPresent(nullptr, "ALC_ENUMERATION_EXT"))
        return;

    // Extract the unique device names from the encoded device specifier list.
    // Device specifiers end with a single @c NULL. List is terminated with double @c NULL.
    QStringList allDeviceNames;
    for(auto *devices = (char const *)alcGetString(nullptr, ALC_DEVICE_SPECIFIER);
        (*devices); devices += qstrlen(devices) + 1)
    {
        auto const deviceName = String(devices);
        if(!deviceName.isEmpty())
        {
            allDeviceNames << deviceName;
        }
    }
    //allDeviceNames.removeDuplicates();
    if(allDeviceNames.isEmpty())
        return;

    auto const defaultDeviceName = String((char const *)alcGetString(nullptr, ALC_DEFAULT_DEVICE_SPECIFIER));

    // Summarize the available devices.
    LOG_AUDIO_MSG("OpenAL Devices Available (%i):") << allDeviceNames.count();
    dint idx = 0;
    for(String const deviceName : allDeviceNames)
    {
        ALCdevice *device = alcOpenDevice(deviceName.toLatin1().constData());
        if(!device) continue;

        // Create context so we can query more specific information.
        dint verMajor = 1, verMinor = 0;
        if(ALCcontext *context = alcCreateContext(device, nullptr))
        {
            alcMakeContextCurrent(context);

            alcGetIntegerv(device, ALC_MAJOR_VERSION, sizeof(verMajor), &verMajor);
            alcGetIntegerv(device, ALC_MINOR_VERSION, sizeof(verMinor), &verMinor);

            alcMakeContextCurrent(nullptr);
            alcDestroyContext(context);
        }

        // We're done with this device (for now at least).
        alcCloseDevice(device);

        LOG_AUDIO_MSG("%i: %s%s (API %i.%i)")
            << (idx++) << deviceName
            << (deviceName.compareWithoutCase(defaultDeviceName) == 0 ? " (default)" : "")
            << verMajor << verMinor;
    }
}

static void loadExtensions()
{
#ifdef HAVE_EAX
    // Check for EAX 2.0.
    ::eaxAvailable = alIsExtensionPresent((ALchar *)"EAX2.0") == AL_TRUE;
    if(::eaxAvailable)
    {
        ::alEAXGet = de::function_cast<EAXGet>(alGetProcAddress((ALchar *)"EAXGet"));
        ::alEAXSet = de::function_cast<EAXSet>(alGetProcAddress((ALchar *)"EAXSet"));
        if(!::alEAXGet || !::alEAXSet)
            ::eaxAvailable = false;

        /*if(::eaxAvailable)
        {
            ::eaxUseXRAM = alIsExtensionPresent((ALchar *)"EAX-RAM") == AL_TRUE;
        }*/
    }
#else
    ::eaxAvailable = false;
#endif
}

int DS_Init()
{
    // Already been here?
    if(::initOk) return true;

    LOG_AUDIO_VERBOSE("Initializing OpenAL...");
    ::device  = nullptr;
    ::context = nullptr;

    ::unitsPerMeter = 1;
    ::headYaw       = 0;
    ::headPitch     = 0;

    ::eaxAvailable = false;
#ifdef HAVE_EAX
    ::eaxDisabled  = DENG2_APP->commandLine().has("-noeax");
    ::alEAXGet     = nullptr;
    ::alEAXSet     = nullptr;
#endif

    // Lets enumerate the available devices to provide a useful summary for the user.
    logAvailableDevices();

    // Lookup the default playback device.
    auto defaultDeviceName = String((char const *)alcGetString(nullptr, ALC_DEFAULT_DEVICE_SPECIFIER));

    // The -oaldevice option can be used to override the default.
    /// @todo Store this persistently in Config. -ds
    if(auto preferredDevice = DENG2_APP->commandLine().check("-oaldevice", 1))
    {
        String const otherName = preferredDevice.params.at(0).strip();
        if(otherName.compareWithoutCase(defaultDeviceName))
            defaultDeviceName.prepend(otherName + ';');
    }

    // Try to open the preferred playback device.
    for(String const deviceName : defaultDeviceName.split(';'))
    {
        if(deviceName.isEmpty()) continue;

        ::device = alcOpenDevice(deviceName.toLatin1().constData());
        if(::device) break;

        LOG_AS("[OpenAL]");
        LOG_AUDIO_ERROR("Failed opening device \"%s\"") << deviceName;
    }

    // We cannot continue without an OpenAL device...
    if(!::device) return false;

    // Create a new context and make it current.
    alcMakeContextCurrent(::context = alcCreateContext(::device, nullptr));
    if((ec = alGetError()) != AL_NO_ERROR)
    {
        LOG_AS("[OpenAL]");
        LOG_AUDIO_ERROR("Failed making context for device \"%s\":\n")
            << alcGetString(NULL, ALC_DEVICE_SPECIFIER)
            << alErrorToText(ec);
        return false;
    }

    // Determine the OpenAL API version we are working with.
    dint verMajor, verMinor;
    alcGetIntegerv(::device, ALC_MAJOR_VERSION, sizeof(verMajor), &verMajor);
    alcGetIntegerv(::device, ALC_MINOR_VERSION, sizeof(verMinor), &verMinor);

    // Attempt to load and configure the EAX extensions.
    loadExtensions();

    // Configure global soundstage properties/state
    ::headYaw = 0;
    ::headPitch = 0;
    ::unitsPerMeter = 36;
    alDistanceModel(AL_INVERSE_DISTANCE_CLAMPED);
    if((ec = alGetError()) != AL_NO_ERROR)
    {
        LOG_AS("[OpenAL]");
        LOG_AUDIO_ERROR("Failed configuring soundstage:\n") << alErrorToText(ec);
    }

    // Configure the listener.
    alListenerf(AL_GAIN, 1);
    if((ec = alGetError()) != AL_NO_ERROR)
    {
        LOG_AS("[OpenAL]");
        LOG_AUDIO_ERROR("Failed configuring listener:\n") << alErrorToText(ec);
    }

    // Log an overview of the OpenAL configuration:
#define TABBED(A, B)  _E(Ta) "  " _E(l) A _E(.) " " _E(Tb) << B << "\n"

    String str;
    QTextStream os(&str);

    os << _E(b) "OpenAL information:\n" << _E(.);

    os << TABBED("Version:", String("%1.%2.0").arg(verMajor).arg(verMinor));
    os << TABBED("Renderer:", alcGetString(::device, ALC_DEVICE_SPECIFIER));
    String environmentModel = ::eaxAvailable ? "EAX 2.0" : "None";
#ifdef HAVE_EAX
    if(::eaxAvailable && ::eaxDisabled)
        environmentModel += " (disabled)";
#endif
    os << TABBED("Environment model:", environmentModel);

    LOG_AUDIO_MSG(str.rightStrip());

#undef TABBED

    // Everything is OK.
    return ::initOk = true;
}

void DS_Shutdown()
{
    if(!::initOk) return;

    alcMakeContextCurrent(nullptr);
    alcDestroyContext(::context);
    ::context = nullptr;

    alcCloseDevice(::device);
    ::device = nullptr;

    initOk = false;
}

void DS_Event(int)
{
    // Not supported.
}

int DS_Get(int prop, void *ptr)
{
    switch(prop)
    {
    case AUDIOP_IDENTITYKEY: {
        auto *idKey = reinterpret_cast<AutoStr *>(ptr);
        DENG2_ASSERT(idKey);
        if(idKey) Str_Set(idKey, "openal;oal");
        return true; }

    case AUDIOP_TITLE: {
        auto *title = reinterpret_cast<AutoStr *>(ptr);
        DENG2_ASSERT(title);
        if(title) Str_Set(title, "OpenAL");
        return true; }

    default: DENG2_ASSERT("[OpenAL]DS_Get: Unknown property"); break;
    }
    return false;
}

int DS_SFX_Init()
{
    return true;
}

sfxbuffer_t *DS_SFX_CreateBuffer(int flags, int bits, int rate)
{
    ALuint bufName;
    alGenBuffers(1, &bufName);
    if((ec = alGetError()) != AL_NO_ERROR)
    {
        LOG_AS("[OpenAL]");
        LOG_AUDIO_ERROR("Failed creating buffer (bits:%i rate:%i):\n")
            << bits << rate
            << alErrorToText(ec);
        return nullptr;
    }

    ALuint srcName;
    alGenSources(1, &srcName);
    if((ec = alGetError()) != AL_NO_ERROR)
    {
        alDeleteBuffers(1, &bufName);

        LOG_AS("[OpenAL]");
        LOG_AUDIO_ERROR("Failed generating sources (1):\n") << alErrorToText(ec);
        return nullptr;
    }

    // Attach the buffer to the source.
    alSourcei(srcName, AL_BUFFER, bufName);
    if((ec = alGetError()) != AL_NO_ERROR)
    {
        alDeleteSources(1, &srcName);
        alDeleteBuffers(1, &bufName);

        LOG_AS("[OpenAL]");
        LOG_AUDIO_ERROR("Failed attaching buffer to source:\n") << alErrorToText(ec);
        return nullptr;
    }

    if(!(flags & SFXBF_3D))
    {
        // 2D sounds are around the listener.
        alSourcei(srcName, AL_SOURCE_RELATIVE, AL_TRUE);
        alSourcef(srcName, AL_ROLLOFF_FACTOR, 0);
        if((ec = alGetError()) != AL_NO_ERROR)
        {
            LOG_AS("[OpenAL]");
            LOGDEV_AUDIO_WARNING("Failed configuring source:\n") << alErrorToText(ec);
        }
    }

    // Create the buffer object.
    sfxbuffer_t *buf = static_cast<sfxbuffer_t *>(Z_Calloc(sizeof(*buf), PU_APPSTATIC, 0));

    buf->ptr   = INT2PTR(void, bufName);
    buf->ptr3D = INT2PTR(void, srcName);
    buf->bytes = bits / 8;
    buf->rate  = rate;
    buf->flags = flags;
    buf->freq  = rate;  // Modified by calls to Set(SFXBP_FREQUENCY).

    return buf;
}

void DS_SFX_DestroyBuffer(sfxbuffer_t *buf)
{
    if(!buf) return;

    ALuint srcName = SRC(buf);
    alDeleteSources(1, &srcName);

    ALuint bufName = BUF(buf);
    alDeleteBuffers(1, &bufName);

    Z_Free(buf);
}

void DS_SFX_Load(sfxbuffer_t *buf, struct sfxsample_s *sample)
{
    if(!buf || !sample) return;

    // Does the buffer already have a sample loaded?
    if(buf->sample)
    {
        // Is the same one?
        if(buf->sample->soundId == sample->soundId)
            return;  // No need to reload.
    }

    // Make sure its not bound right now.
    alSourcei(SRC(buf), AL_BUFFER, 0);

    alBufferData(BUF(buf),
                 sample->bytesPer == 1 ? AL_FORMAT_MONO8 : AL_FORMAT_MONO16,
                 sample->data, sample->size, sample->rate);

    if((ec = alGetError()) != AL_NO_ERROR)
    {
        /// @todo What to do? -jk

        LOG_AS("[OpenAL]");
        LOG_AUDIO_ERROR("Failed to buffer sample:\n") << alErrorToText(ec);
    }

    buf->sample = sample;
}

/**
 * Stops the buffer and makes it forget about its sample.
 */
void DS_SFX_Reset(sfxbuffer_t *buf)
{
    if(!buf) return;

    DS_SFX_Stop(buf);
    alSourcei(SRC(buf), AL_BUFFER, 0);
    if((ec = alGetError()) != AL_NO_ERROR)
    {
        LOG_AS("[OpenAL]");
        LOG_AUDIO_ERROR("Failed resetting buffer:\n") << alErrorToText(ec);
    }

    buf->sample = nullptr;
}

void DS_SFX_Play(sfxbuffer_t *buf)
{
    // Playing is quite impossible without a sample.
    if(!buf || !buf->sample) return;

    ALuint source = SRC(buf);
    alSourcei(source, AL_BUFFER, BUF(buf));
    alSourcei(source, AL_LOOPING, (buf->flags & SFXBF_REPEAT) != 0);
    alSourcePlay(source);
    if((ec = alGetError()) != AL_NO_ERROR)
    {
        LOG_AS("[OpenAL]");
        LOG_AUDIO_ERROR("Failed to play buffer:\n") << alErrorToText(ec);
    }

    // The buffer is now playing.
    buf->flags |= SFXBF_PLAYING;
}

void DS_SFX_Stop(sfxbuffer_t *buf)
{
    if(!buf || !buf->sample) return;

    alSourceRewind(SRC(buf));
    if((ec = alGetError()) != AL_NO_ERROR)
    {
        LOG_AS("[OpenAL]");
        LOG_AUDIO_ERROR("Failed rewinding buffer:\n") << alErrorToText(ec);
    }

    buf->flags &= ~SFXBF_PLAYING;
}

void DS_SFX_Refresh(sfxbuffer_t *buf)
{
    if(!buf || !buf->sample) return;

    ALint state;
    alGetSourcei(SRC(buf), AL_SOURCE_STATE, &state);
    if((ec = alGetError()) != AL_NO_ERROR)
    {
        LOG_AS("[OpenAL]");
        LOG_AUDIO_ERROR("Failed querying source state:\n") << alErrorToText(ec);
    }

    if(state == AL_STOPPED)
    {
        buf->flags &= ~SFXBF_PLAYING;
    }
}

/**
 * @param yaw    Yaw in radians.
 * @param pitch  Pitch in radians.
 * @param front  Front vector is written here.
 * @param up     Up vector is written here.
 */
static void vectors(dfloat yaw, dfloat pitch, Vector3f &front, Vector3f &up)
{
    front = Vector3f( cos(yaw) * cos(pitch), sin(pitch),  sin(yaw) * cos(pitch));
    up    = Vector3f(-cos(yaw) * sin(pitch), cos(pitch), -sin(yaw) * sin(pitch));
}

void DS_SFX_Set(sfxbuffer_t *buf, int prop, float value)
{
    if(!buf) return;

    switch(prop)
    {
    case SFXBP_VOLUME:
        alSourcef(SRC(buf), AL_GAIN, value);
        if((ec = alGetError()) != AL_NO_ERROR)
        {
            LOG_AS("[OpenAL]");
            LOG_AUDIO_ERROR("Failed setting source volume:\n") << alErrorToText(ec);
        }
        break;

    case SFXBP_FREQUENCY: {
        auto dw = duint(buf->rate * value);
        if(dw != buf->freq)  // Don't set redundantly.
        {
            buf->freq = dw;
            alSourcef(SRC(buf), AL_PITCH, value);
            if((ec = alGetError()) != AL_NO_ERROR)
            {
                LOG_AS("[OpenAL]");
                LOG_AUDIO_ERROR("Failed setting source pitch:\n") << alErrorToText(ec);
            }
        }
        break; }

    case SFXBP_PAN: {  // Pan is linear, from -1 to 1. 0 is in the middle.
        if(buf->flags & SFXBF_3D)
        {
            Vector3f front, up;
            vectors(dfloat( ::headYaw - value * DD_PI / 2 ), ::headPitch, front, up);

            dfloat vec[3]; front.decompose(vec);
            alSourcefv(SRC(buf), AL_POSITION, vec);
        }
        else
        {
            dfloat vec[3] = { value, 0, 0};
            alSourcefv(SRC(buf), AL_POSITION, vec);
        }

        if((ec = alGetError()) != AL_NO_ERROR)
        {
            LOG_AS("[OpenAL]");
            LOG_AUDIO_ERROR("Failed setting source panning:\n") << alErrorToText(ec);
        }
        break; }

    case SFXBP_MIN_DISTANCE:
        alSourcef(SRC(buf), AL_REFERENCE_DISTANCE, value / ::unitsPerMeter);
        if((ec = alGetError()) != AL_NO_ERROR)
        {
            LOG_AS("[OpenAL]");
            LOG_AUDIO_ERROR("Failed setting source min-distance:\n") << alErrorToText(ec);
        }
        break;

    case SFXBP_MAX_DISTANCE:
        alSourcef(SRC(buf), AL_MAX_DISTANCE, value / ::unitsPerMeter);
        if((ec = alGetError()) != AL_NO_ERROR)
        {
            LOG_AS("[OpenAL]");
            LOG_AUDIO_ERROR("Failed setting source max-distance:\n") << alErrorToText(ec);
        }
        break;

    case SFXBP_RELATIVE_MODE:
        alSourcei(SRC(buf), AL_SOURCE_RELATIVE, value ? AL_TRUE : AL_FALSE);
        if((ec = alGetError()) != AL_NO_ERROR)
        {
            LOG_AS("[OpenAL]");
            LOG_AUDIO_ERROR("Failed setting source relative-mode:\n") << alErrorToText(ec);
        }
        break;

    default: break;
    }
}

void DS_SFX_Setv(sfxbuffer_t *buf, int prop, float *values)
{
    if(!buf || !values) return;

    switch(prop)
    {
    case SFXBP_POSITION: {
        auto const position = Vector3f(values).xzy() / ::unitsPerMeter;
        alSource3f(SRC(buf), AL_POSITION, position.x, position.y, position.z);
        if((ec = alGetError()) != AL_NO_ERROR)
        {
            LOG_AS("[OpenAL]");
            LOG_AUDIO_ERROR("Failed setting source position:\n") << alErrorToText(ec);
        }
        break; }

    case SFXBP_VELOCITY: {
        auto const velocity = Vector3f(values).xzy() / ::unitsPerMeter;
        alSource3f(SRC(buf), AL_VELOCITY, velocity.x, velocity.y, velocity.z);
        if((ec = alGetError()) != AL_NO_ERROR)
        {
            LOG_AS("[OpenAL]");
            LOG_AUDIO_ERROR("Failed setting source velocity:\n") << alErrorToText(ec);
        }
        break; }

    default: break;
    }
}

#ifdef HAVE_EAX

/**
 * Utility for converting linear volume 0..1 to logarithmic -10000..0.
 */
static dint volLinearToLog(dfloat vol)
{
    if(vol <= 0) return EAXLISTENER_MINROOM;
    if(vol >= 1) return EAXLISTENER_MAXROOM;

    // Straighten the volume curve.
    return de::clamp<dint>(EAXLISTENER_MINROOM, 100 * 20 * log10(vol), EAXLISTENER_MAXROOM);
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

static void setEAXdw(ALuint prop, dint value)
{
    alEAXSet(&DSPROPSETID_EAX20_ListenerProperties, prop | DSPROPERTY_EAXLISTENER_DEFERRED, 0, &value, sizeof(value));
    if((ec = alGetError()) != AL_NO_ERROR)
    {
        LOG_AS("[OpenAL]");
        LOGDEV_AUDIO_WARNING("setEAXdw (prop:%i value:%i) failed:\n")
            << prop << value << alErrorToText(ec);
    }
}

static void setEAXf(ALuint prop, dfloat value)
{
    alEAXSet(&DSPROPSETID_EAX_ListenerProperties, prop | DSPROPERTY_EAXLISTENER_DEFERRED, 0, &value, sizeof(value));
    if((ec = alGetError()) != AL_NO_ERROR)
    {
        LOG_AS("[OpenAL]");
        LOGDEV_AUDIO_WARNING("setEAXf (prop:%i value:%f) failed:\n")
            << prop << value << alErrorToText(ec);
    }
}

/**
 * Linear multiplication for a logarithmic property.
 */
static void mulEAXdw(ALuint prop, dfloat mul)
{
    ulong value;
    alEAXGet(&DSPROPSETID_EAX_ListenerProperties, prop, 0, &value, sizeof(value));
    if((ec = alGetError()) != AL_NO_ERROR)
    {
        LOG_AS("[OpenAL]");
        LOGDEV_AUDIO_WARNING("mulEAXdw (prop:%i) get failed:\n")
            << prop << alErrorToText(ec);
    }
    setEAXdw(prop, volLinearToLog(pow(10, value / 2000.0f) * mul));
}

/**
 * Linear multiplication for a linear property.
 */
static void mulEAXf(ALuint prop, dfloat mul, dfloat min, dfloat max)
{
    dfloat value;
    alEAXGet(&DSPROPSETID_EAX_ListenerProperties, prop, 0, &value, sizeof(value));
    if((ec = alGetError()) != AL_NO_ERROR)
    {
        LOG_AS("[OpenAL]");
        LOGDEV_AUDIO_WARNING("mulEAXf (prop:%i) get failed:\n")
            << prop << alErrorToText(ec);
    }
    setEAXf(prop, de::clamp(min, value * mul, max));
}
#endif  // HAVE_EAX

void DS_SFX_Listener(int prop, float value)
{
    switch(prop)
    {
#ifdef HAVE_EAX
    case SFXLP_UPDATE:
        // If EAX is available, commit deferred property changes.
        if(::eaxAvailable && !::eaxDisabled)
        {
            alEAXSet(&DSPROPSETID_EAX_ListenerProperties, DSPROPERTY_EAXLISTENER_COMMITDEFERREDSETTINGS, 0, nullptr, 0);
            if((ec = alGetError()) != AL_NO_ERROR)
            {
                LOG_AS("[OpenAL]");
                LOGDEV_AUDIO_WARNING("Failed commiting deferred listener EAX properties:\n")
                    << alErrorToText(ec);
            }
        }
        break;
#endif

    case SFXLP_UNITS_PER_METER:
        ::unitsPerMeter = value;
        break;

    case SFXLP_DOPPLER:
        alDopplerFactor(value);
        if((ec = alGetError()) != AL_NO_ERROR)
        {
            LOG_AS("[OpenAL]");
            LOG_AUDIO_ERROR("Failed setting Doppler factor:\n") << alErrorToText(ec);
        }
        break;

    default: break;
    }
}

void DS_SFX_Listenerv(int prop, float *values)
{
    if(!values) return;

    switch(prop)
    {
    case SFXLP_PRIMARY_FORMAT:
        // No need to concern ourselves with this kind of things...
        break;

    case SFXLP_POSITION: {
        auto const position = Vector3f(values).xzy() / ::unitsPerMeter;
        alListener3f(AL_POSITION, position.x, position.y, position.z);
        if((ec = alGetError()) != AL_NO_ERROR)
        {
            LOG_AS("[OpenAL]");
            LOG_AUDIO_ERROR("Failed setting listener position:\n") << alErrorToText(ec);
        }
        break; }

    case SFXLP_VELOCITY: {
        auto const velocity = Vector3f(values).xzy() / ::unitsPerMeter;
        alListener3f(AL_VELOCITY, velocity.x, velocity.y, velocity.z);
        if((ec = alGetError()) != AL_NO_ERROR)
        {
            LOG_AS("[OpenAL]");
            LOG_AUDIO_ERROR("Failed setting listener velocity:\n") << alErrorToText(ec);
        }
        break; }

    case SFXLP_ORIENTATION: {
        Vector3f front, up;
        vectors(::headYaw   = dfloat( values[0] / 180 * DD_PI ),
                ::headPitch = dfloat( values[1] / 180 * DD_PI ),
                front, up);

        dfloat vec[6]; front.decompose(vec); up.decompose(vec + 3);
        alListenerfv(AL_ORIENTATION, vec);
        if((ec = alGetError()) != AL_NO_ERROR)
        {
            LOG_AS("[OpenAL]");
            LOG_AUDIO_ERROR("Failed setting listener orientation:\n") << alErrorToText(ec);
        }
        break; }

#ifdef HAVE_EAX
    case SFXLP_REVERB:
        // If EAX is available, set the listening environmental properties.
        if(::eaxAvailable && !::eaxDisabled)
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

DENG_DECLARE_API(Con);

DENG_API_EXCHANGE(
    DENG_GET_API(DE_API_CONSOLE, Con);
)
