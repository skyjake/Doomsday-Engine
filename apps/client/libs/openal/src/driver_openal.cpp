/**
 * @file driver_openal.cpp
 * OpenAL audio plugin. @ingroup dsopenal
 *
 * @bug Not 64bit clean: In function 'DS_SFX_CreateBuffer': cast to pointer from integer of different size
 * @bug Not 64bit clean: In function 'DS_SFX_DestroyBuffer': cast to pointer from integer of different size
 * @bug Not 64bit clean: In function 'DS_SFX_Play': cast to pointer from integer of different size
 * @bug Not 64bit clean: In function 'DS_SFX_Stop': cast to pointer from integer of different size
 * @bug Not 64bit clean: In function 'DS_SFX_Refresh': cast to pointer from integer of different size
 * @bug Not 64bit clean: In function 'DS_SFX_Set': cast to pointer from integer of different size
 * @bug Not 64bit clean: In function 'DS_SFX_Setv': cast to pointer from integer of different size
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2009 Daniel Swanson <danij@dengine.net>
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
#elif defined (DE_IOS)
#  include <OpenAL/al.h>
#  include <OpenAL/alc.h>
#else
#  include <AL/al.h>
#  include <AL/alc.h>
#endif
#include <stdio.h>
#include <cassert>
#include <iostream>
#include <cstring>
#include <cmath>

#include "api_audiod.h"
#include "api_audiod_sfx.h"
#include "doomsday.h"

#include <de/c_wrapper.h>
#include <de/extension.h>

DE_USING_API(Con);
//DE_DECLARE_API(Con);

#define SRC(buf) ( (ALuint) PTR2INT(buf->ptr3D) )
#define BUF(buf) ( (ALuint) PTR2INT(buf->ptr) )

//enum { VX, VY, VZ };

#ifdef WIN32
ALenum(*EAXGet) (const struct _GUID* propertySetID, ALuint prop, ALuint source, ALvoid* value, ALuint size);
ALenum(*EAXSet) (const struct _GUID* propertySetID, ALuint prop, ALuint source, ALvoid* value, ALuint size);
#endif

//extern "C" {

//int DS_Init(void);
//void DS_Shutdown(void);
//void DS_Event(int type);

//int DS_SFX_Init(void);
//sfxbuffer_t* DS_SFX_CreateBuffer(int flags, int bits, int rate);
//void DS_SFX_DestroyBuffer(sfxbuffer_t* buf);
//void DS_SFX_Load(sfxbuffer_t* buf, struct sfxsample_s* sample);
//void DS_SFX_Reset(sfxbuffer_t* buf);
//void DS_SFX_Play(sfxbuffer_t* buf);
static void DS_SFX_Stop(sfxbuffer_t* buf);
//void DS_SFX_Refresh(sfxbuffer_t* buf);
//void DS_SFX_Set(sfxbuffer_t* buf, int prop, float value);
//void DS_SFX_Setv(sfxbuffer_t* buf, int prop, float* values);
//void DS_SFX_Listener(int prop, float value);
//void DS_SFX_Listenerv(int prop, float* values);
//int DS_SFX_Getv(int prop, void* values);

//} // extern "C"

#ifdef WIN32
// EAX 2.0 GUIDs
struct _GUID DSPROPSETID_EAX20_ListenerProperties = {
    0x306a6a8, 0xb224, 0x11d2, {0x99, 0xe5, 0x0, 0x0, 0xe8, 0xd8, 0xc7, 0x22}
};
struct _GUID DSPROPSETID_EAX20_BufferProperties = {
    0x306a6a7, 0xb224, 0x11d2, {0x99, 0xe5, 0x0, 0x0, 0xe8, 0xd8, 0xc7, 0x22}
};
#endif

static dd_bool initOk = false;
static dd_bool hasEAX = false;
static float unitsPerMeter = 1;
static float headYaw, headPitch; // In radians.
static ALCdevice* device = 0;
static ALCcontext* context = 0;

#ifdef DE_DSOPENAL_DEBUG
#  define DSOPENAL_TRACE(args)  std::cerr << "[dsOpenAL] " << args << std::endl;
#else
#  define DSOPENAL_TRACE(args)
#endif

#define DSOPENAL_ERRCHECK(errorcode) \
    error(errorcode, __FILE__, __LINE__)

static int error(ALenum errorCode, const char* file, int line)
{
    if(errorCode == AL_NO_ERROR) return false;
    std::cerr << "[dsOpenAL] Error at " << file << ", line " << line
              << ": (" << (int)errorCode << ") " << (const char*)alGetString(errorCode);
    return true;
}

static void loadExtensions(void)
{
#ifdef WIN32
    // Check for EAX 2.0.
    hasEAX = alIsExtensionPresent((ALchar*) "EAX2.0");
    if(hasEAX)
    {
        EAXGet = (ALenum (*)(const struct _GUID*, ALuint, ALuint, ALvoid*, ALuint))alGetProcAddress("EAXGet");
        EAXSet = (ALenum (*)(const struct _GUID*, ALuint, ALuint, ALvoid*, ALuint))alGetProcAddress("EAXSet");
        if(!EAXGet || !EAXSet)
            hasEAX = false;
    }
#else
    hasEAX = false;
#endif
}

static int DS_Init(void)
{
    // Already initialized?
    if(initOk) return true;

    // Open the default playback device.
    device = alcOpenDevice(NULL);
    if(!device)
    {
        App_Log(DE2_AUDIO_ERROR, "OpenAL init failed (using default playback device)");
        return false;
    }

    // Create and make current a new context.
    alcMakeContextCurrent(context = alcCreateContext(device, NULL));
    DSOPENAL_ERRCHECK(alGetError());

    // Attempt to load and configure the EAX extensions.
    loadExtensions();

    // Configure the listener and global OpenAL properties/state.
    alListenerf(AL_GAIN, 1);
    alDistanceModel(AL_INVERSE_DISTANCE_CLAMPED);
    headYaw = headPitch = 0;
    unitsPerMeter = 36;

    // Everything is OK.
    DSOPENAL_TRACE("DS_Init: OpenAL initialized%s." << (hasEAX? " (EAX 2.0 available)" : ""));
    initOk = true;
    return true;
}

static void DS_Shutdown(void)
{
    if(!initOk) return;

    alcMakeContextCurrent(NULL);
    alcDestroyContext(context);
    alcCloseDevice(device);

    context = NULL;
    device = NULL;
    initOk = false;
}

static void DS_Event(int /*type*/)
{
    // Not supported.
}

static int DS_SFX_Init(void)
{
    return true;
}

static sfxbuffer_t* DS_SFX_CreateBuffer(int flags, int bits, int rate)
{
    sfxbuffer_t* buf;
    ALuint bufName, srcName;

    // Create a new buffer and a new source.
    alGenBuffers(1, &bufName);
    if(DSOPENAL_ERRCHECK(alGetError()))
        return NULL;

    alGenSources(1, &srcName);
    if(DSOPENAL_ERRCHECK(alGetError()))
    {
        alDeleteBuffers(1, &bufName);
        return NULL;
    }

    /*
    // Attach the buffer to the source.
    alSourcei(srcName, AL_BUFFER, bufName);
    if(DSOPENAL_ERRCHECK(alGetError()))
    {
        alDeleteSources(1, &srcName);
        alDeleteBuffers(1, &bufName);
        return NULL;
    }
    */

    if(!(flags & SFXBF_3D))
    {
        // 2D sounds are around the listener.
        alSourcei(srcName, AL_SOURCE_RELATIVE, AL_TRUE);
        alSourcef(srcName, AL_ROLLOFF_FACTOR, 0);
    }

    // Create the buffer object.
    buf = static_cast<sfxbuffer_t*>(Z_Calloc(sizeof(*buf), PU_APPSTATIC, 0));

    buf->ptr = INT2PTR(void, bufName);
    buf->ptr3D = INT2PTR(void, srcName);
    buf->bytes = bits / 8;
    buf->rate = rate;
    buf->flags = flags;
    buf->freq = rate; // Modified by calls to Set(SFXBP_FREQUENCY).

    return buf;
}

static void DS_SFX_DestroyBuffer(sfxbuffer_t* buf)
{
    ALuint srcName, bufName;

    if(!buf) return;

    srcName = SRC(buf);
    bufName = BUF(buf);

    alDeleteSources(1, &srcName);
    alDeleteBuffers(1, &bufName);

    Z_Free(buf);
}

static void DS_SFX_Load(sfxbuffer_t* buf, struct sfxsample_s* sample)
{
    if(!buf || !sample) return;

    // Does the buffer already have a sample loaded?
    if(buf->sample)
    {
        // Is the same one?
        if(buf->sample->id == sample->id)
            return; // No need to reload.
    }

    // Make sure its not bound right now.
    alSourcei(SRC(buf), AL_BUFFER, 0);

    alBufferData(BUF(buf),
                 sample->bytesPer == 1 ? AL_FORMAT_MONO8 : AL_FORMAT_MONO16,
                 sample->data, sample->size, sample->rate);

    if(DSOPENAL_ERRCHECK(alGetError()))
    {
        /// @todo What to do?
    }
    buf->sample = sample;
}

/**
 * Stops the buffer and makes it forget about its sample.
 */
static void DS_SFX_Reset(sfxbuffer_t* buf)
{
    if(!buf) return;

    DS_SFX_Stop(buf);
    alSourcei(SRC(buf), AL_BUFFER, 0);
    buf->sample = NULL;
}

static void DS_SFX_Play(sfxbuffer_t* buf)
{
    ALuint source;

    // Playing is quite impossible without a sample.
    if(!buf || !buf->sample) return;

    source = SRC(buf);
    alSourcei(source, AL_BUFFER, BUF(buf));
    alSourcei(source, AL_LOOPING, (buf->flags & SFXBF_REPEAT) != 0);
    alSourcePlay(source);
    DSOPENAL_ERRCHECK(alGetError());

    // The buffer is now playing.
    buf->flags |= SFXBF_PLAYING;
}

static void DS_SFX_Stop(sfxbuffer_t* buf)
{
    if(!buf || !buf->sample) return;

    alSourceRewind(SRC(buf));
    buf->flags &= ~SFXBF_PLAYING;
}

static void DS_SFX_Refresh(sfxbuffer_t* buf)
{
    ALint state;

    if(!buf || !buf->sample) return;

    alGetSourcei(SRC(buf), AL_SOURCE_STATE, &state);
    if(state == AL_STOPPED)
    {
        buf->flags &= ~SFXBF_PLAYING;
    }
}

/**
 * @param yaw           Yaw in radians.
 * @param pitch         Pitch in radians.
 * @param front         Ptr to front vector, can be @c NULL.
 * @param up            Ptr to up vector, can be @c NULL.
 */
static void vectors(float yaw, float pitch, float* front, float* up)
{
    if(!front && !up)
        return; // Nothing to do.

    if(front)
    {
        front[VX] = (float) (cos(yaw) * cos(pitch));
        front[VZ] = (float) (sin(yaw) * cos(pitch));
        front[VY] = (float) sin(pitch);
    }

    if(up)
    {
        up[VX] = (float) (-cos(yaw) * sin(pitch));
        up[VZ] = (float) (-sin(yaw) * sin(pitch));
        up[VY] = (float) cos(pitch);
    }
}

/**
 * Pan is linear, from -1 to 1. 0 is in the middle.
 */
static void setPan(ALuint source, float pan)
{
    float pos[3];

    vectors((float) (headYaw - pan * DD_PI / 2), headPitch, pos, 0);
    alSourcefv(source, AL_POSITION, pos);
}

static void DS_SFX_Set(sfxbuffer_t* buf, int prop, float value)
{
    ALuint source;

    if(!buf) return;

    source = SRC(buf);

    switch(prop)
    {
    case SFXBP_VOLUME:
        alSourcef(source, AL_GAIN, value);
        break;

    case SFXBP_FREQUENCY: {
        unsigned int dw = (int) (buf->rate * value);
        if(dw != buf->freq) // Don't set redundantly.
        {
            buf->freq = dw;
            alSourcef(source, AL_PITCH, value);
        }
        break; }

    case SFXBP_PAN:
        setPan(source, value);
        break;

    case SFXBP_MIN_DISTANCE:
        alSourcef(source, AL_REFERENCE_DISTANCE, value / unitsPerMeter);
        break;

    case SFXBP_MAX_DISTANCE:
        alSourcef(source, AL_MAX_DISTANCE, value / unitsPerMeter);
        break;

    case SFXBP_RELATIVE_MODE:
        alSourcei(source, AL_SOURCE_RELATIVE, value ? AL_TRUE : AL_FALSE);
        break;

    default: break;
    }
}

static void DS_SFX_Setv(sfxbuffer_t* buf, int prop, float* values)
{
    ALuint source;

    if(!buf || !values) return;

    source = SRC(buf);

    switch(prop)
    {
    case SFXBP_POSITION:
        alSource3f(source, AL_POSITION, values[VX] / unitsPerMeter,
                   values[VZ] / unitsPerMeter, values[VY] / unitsPerMeter);
        break;

    case SFXBP_VELOCITY:
        alSource3f(source, AL_VELOCITY, values[VX] / unitsPerMeter,
                   values[VZ] / unitsPerMeter, values[VY] / unitsPerMeter);
        break;

    default: break;
    }
}

static void DS_SFX_Listener(int prop, float value)
{
    switch(prop)
    {
    case SFXLP_UNITS_PER_METER:
        unitsPerMeter = value;
        break;

    case SFXLP_DOPPLER:
        alDopplerFactor(value);
        break;

    default: break;
    }
}

static void DS_SFX_Listenerv(int prop, float* values)
{
    float ori[6];

    if(!values) return;

    switch(prop)
    {
    case SFXLP_PRIMARY_FORMAT:
        // No need to concern ourselves with this kind of things...
        break;

    case SFXLP_POSITION:
        alListener3f(AL_POSITION, values[VX] / unitsPerMeter,
                     values[VZ] / unitsPerMeter, values[VY] / unitsPerMeter);
        break;

    case SFXLP_VELOCITY:
        alListener3f(AL_VELOCITY, values[VX] / unitsPerMeter,
                     values[VZ] / unitsPerMeter, values[VY] / unitsPerMeter);
        break;

    case SFXLP_ORIENTATION:
        vectors(headYaw = (float) (values[VX] / 180 * DD_PI),
                headPitch = (float) (values[VY] / 180 * DD_PI),
                ori, ori + 3);
        alListenerfv(AL_ORIENTATION, ori);
        break;

    case SFXLP_REVERB: // Not supported.
        break;

    default:
        DS_SFX_Listener(prop, 0);
        break;
    }
}

static int DS_SFX_Getv(int /*prop*/, void* /*values*/)
{
    // Stub.
    return 0;
}

/**
 * Declares the type of the plugin so the engine knows how to treat it. Called
 * automatically when the plugin is loaded.
 */
static const char *deng_LibraryType(void)
{
    return "deng-plugin/audio";
}

//DE_API_EXCHANGE(
//    DE_GET_API(DE_API_CONSOLE, Con);
//)

DE_ENTRYPOINT void *extension_openal_symbol(const char *name)
{
    DE_SYMBOL_PTR(name, deng_LibraryType)
    DE_SYMBOL_PTR(name, DS_Init)
    DE_SYMBOL_PTR(name, DS_Shutdown)
    DE_SYMBOL_PTR(name, DS_Event)
    DE_SYMBOL_PTR(name, DS_SFX_Init)
    DE_SYMBOL_PTR(name, DS_SFX_CreateBuffer)
    DE_SYMBOL_PTR(name, DS_SFX_DestroyBuffer)
    DE_SYMBOL_PTR(name, DS_SFX_Load)
    DE_SYMBOL_PTR(name, DS_SFX_Reset)
    DE_SYMBOL_PTR(name, DS_SFX_Play)
    DE_SYMBOL_PTR(name, DS_SFX_Stop)
    DE_SYMBOL_PTR(name, DS_SFX_Refresh)
    DE_SYMBOL_PTR(name, DS_SFX_Set)
    DE_SYMBOL_PTR(name, DS_SFX_Setv)
    DE_SYMBOL_PTR(name, DS_SFX_Listener)
    DE_SYMBOL_PTR(name, DS_SFX_Listenerv)
    DE_SYMBOL_PTR(name, DS_SFX_Getv)
    de::warning("\"%s\" not found in audio_openal", name);
    return nullptr;
}
