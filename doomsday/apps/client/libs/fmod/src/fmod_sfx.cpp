/**
 * @file fmod_sfx.cpp
 * Sound effects interface. @ingroup dsfmod
 *
 * @authors Copyright © 2011-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html (with exception granted to allow
 * linking against FMOD Ex)
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses
 *
 * <b>Special Exception to GPLv2:</b>
 * Linking the Doomsday Audio Plugin for FMOD Ex (audio_fmod) statically or
 * dynamically with other modules is making a combined work based on
 * audio_fmod. Thus, the terms and conditions of the GNU General Public License
 * cover the whole combination. In addition, <i>as a special exception</i>, the
 * copyright holders of audio_fmod give you permission to combine audio_fmod
 * with free software programs or libraries that are released under the GNU
 * LGPL and with code included in the standard release of "FMOD Ex Programmer's
 * API" under the "FMOD Ex Programmer's API" license (or modified versions of
 * such code, with unchanged license). You may copy and distribute such a
 * system following the terms of the GNU GPL for audio_fmod and the licenses of
 * the other code concerned, provided that you include the source code of that
 * other code when and as the GNU GPL requires distribution of source code.
 * (Note that people who make modified versions of audio_fmod are not obligated
 * to grant this special exception for their modified versions; it is their
 * choice whether to do so. The GNU General Public License gives permission to
 * release a modified version without this exception; this exception also makes
 * it possible to release a modified version which carries forward this
 * exception.) </small>
 */

#include "driver_fmod.h"
#include "dd_share.h"
#include <de/logbuffer.h>
#include <de/lockable.h>
#include <stdlib.h>
#include <cmath>
#include <vector>
#include <map>

typedef std::map<FMOD_SOUND*, sfxbuffer_t*> Streams;
typedef std::vector<char> RawSamplePCM8;

struct BufferInfo
{
    FMOD_CHANNEL* channel;
    FMOD_SOUND* sound;
    FMOD_MODE mode;
    float pan;
    float volume;
    float minDistanceMeters;
    float maxDistanceMeters;
    FMODVector position;
    FMODVector velocity;

    BufferInfo()
        : channel(0), sound(0), mode(0),
          pan(0.f), volume(1.f),
          minDistanceMeters(10), maxDistanceMeters(100) {}

    /**
     * Changes the channel's 3D position mode (head-relative or world coordinates).
     *
     * @param newMode  @c true, if the channel should be head-relative.
     */
    void setRelativeMode(bool newMode) {
        if (newMode) {
            mode &= ~FMOD_3D_WORLDRELATIVE;
            mode |=  FMOD_3D_HEADRELATIVE;
        }
        else {
            mode |=  FMOD_3D_WORLDRELATIVE;
            mode &= ~FMOD_3D_HEADRELATIVE;
        }
        if (channel) FMOD_Channel_SetMode(channel, mode);
    }
};

struct Listener
{
    FMODVector position;
    FMODVector velocity;
    FMODVector front;
    FMODVector up;

    /**
     * Parameters are in radians.
     * Example front vectors:
     *   Yaw 0:(0,0,1), pi/2:(-1,0,0)
     */
    void setOrientation(float yaw, float pitch)
    {
        using std::sin;
        using std::cos;

        front.x = cos(yaw) * cos(pitch);
        front.y = sin(yaw) * cos(pitch);
        front.z = sin(pitch);

        up.x = -cos(yaw) * sin(pitch);
        up.y = -sin(yaw) * sin(pitch);
        up.z = cos(pitch);

        /*DSFMOD_TRACE("Front:" << front.x << "," << front.y << "," << front.z << " Up:"
                     << up.x << "," << up.y << "," << up.z);*/
    }
};

static float unitsPerMeter = 1.f;
static float dopplerScale = 1.f;
static Listener listener;
static de::LockableT<Streams> streams;

const char* sfxPropToString(int prop)
{
    switch (prop)
    {
    case SFXBP_VOLUME:        return "SFXBP_VOLUME";
    case SFXBP_FREQUENCY:     return "SFXBP_FREQUENCY";
    case SFXBP_PAN:           return "SFXBP_PAN";
    case SFXBP_MIN_DISTANCE:  return "SFXBP_MIN_DISTANCE";
    case SFXBP_MAX_DISTANCE:  return "SFXBP_MAX_DISTANCE";
    case SFXBP_POSITION:      return "SFXBP_POSITION";
    case SFXBP_VELOCITY:      return "SFXBP_VELOCITY";
    case SFXBP_RELATIVE_MODE: return "SFXBP_RELATIVE_MODE";
    default:                  return "?";
    }
}

static BufferInfo& bufferInfo(sfxbuffer_t* buf)
{
    assert(buf->ptr != 0);
    return *reinterpret_cast<BufferInfo*>(buf->ptr);
}

static FMOD_RESULT F_CALLBACK channelCallback(FMOD_CHANNELCONTROL *channelcontrol,
                                              FMOD_CHANNELCONTROL_TYPE controltype,
                                              FMOD_CHANNELCONTROL_CALLBACK_TYPE callbacktype,
                                              void *,
                                              void *)
{
    if (controltype != FMOD_CHANNELCONTROL_CHANNEL)
    {
        return FMOD_OK;
    }

    FMOD_CHANNEL *channel = reinterpret_cast<FMOD_CHANNEL *>(channelcontrol);
    sfxbuffer_t *buf = 0;

    switch (callbacktype)
    {
    case FMOD_CHANNELCONTROL_CALLBACK_END:
        // The sound has ended, mark the channel.
        FMOD_Channel_GetUserData(channel, reinterpret_cast<void **>(&buf));
        if (buf)
        {
            LOGDEV_AUDIO_XVERBOSE("[FMOD] channelCallback: sfxbuffer %p stops", buf);
            buf->flags &= ~SFXBF_PLAYING;
            // The channel becomes invalid after the sound stops.
            bufferInfo(buf).channel = 0;
        }
        FMOD_Channel_SetCallback(channel, 0);
        FMOD_Channel_SetUserData(channel, 0);
        break;

    default:
        break;
    }
    return FMOD_OK;
}

int fmod_DS_SFX_Init(void)
{
    return fmodSystem != 0;
}

sfxbuffer_t* fmod_DS_SFX_CreateBuffer(int flags, int bits, int rate)
{
    DSFMOD_TRACE("SFX_CreateBuffer: flags=" << flags << ", bits=" << bits << ", rate=" << rate);

    sfxbuffer_t* buf;

    // Clear the buffer.
    buf = reinterpret_cast<sfxbuffer_t*>(calloc(sizeof(*buf), 1));

    // Initialize with format info.
    buf->bytes = bits / 8;
    buf->rate = rate;
    buf->flags = flags;
    buf->freq = rate; // Modified by calls to Set(SFXBP_FREQUENCY).

    // Allocate extra state information.
    buf->ptr = new BufferInfo;

    LOGDEV_AUDIO_XVERBOSE("[FMOD] SFX_CreateBuffer: Created sfxbuffer %p", buf);

    return buf;
}

void fmod_DS_SFX_DestroyBuffer(sfxbuffer_t* buf)
{
    if (!buf) return;

    LOGDEV_AUDIO_XVERBOSE("[FMOD] SFX_DestroyBuffer: Destroying sfxbuffer %p", buf);

    BufferInfo& info = bufferInfo(buf);
    if (info.sound)
    {
        DE_GUARD(streams);
        FMOD_Sound_Release(info.sound);
        streams.value.erase(info.sound);
    }

    // Free the memory allocated for the buffer.
    delete reinterpret_cast<BufferInfo*>(buf->ptr);
    free(buf);
}

#if 0
static void toSigned8bit(const unsigned char* source, int size, RawSamplePCM8& output)
{
    output.clear();
    output.resize(size);
    for (int i = 0; i < size; ++i)
    {
        output[i] = char(source[i]) - 128;
    }
}
#endif

static FMOD_RESULT F_CALLBACK pcmReadCallback(FMOD_SOUND* soundPtr, void* data, unsigned int datalen)
{
    FMOD_SOUND *sound = reinterpret_cast<FMOD_SOUND *>(soundPtr);

    sfxbuffer_t *buf = nullptr;
    {
        DE_GUARD(streams);
        Streams::iterator found = streams.value.find(sound);
        if (found == streams.value.end())
        {
            return FMOD_ERR_NOTREADY;
        }
        buf = found->second;
        DE_ASSERT(buf != NULL);
        DE_ASSERT(buf->flags & SFXBF_STREAM);
    }

    // Call the stream callback.
    sfxstreamfunc_t func = reinterpret_cast<sfxstreamfunc_t>(buf->sample->data);
    if (func(buf, data, datalen))
    {
        return FMOD_OK;
    }
    else
    {
        // The stream function failed to produce data.
        return FMOD_ERR_NOTREADY;
    }
}

/**
 * Prepare the buffer for playing a sample by filling the buffer with as
 * much sample data as fits. The pointer to sample is saved, so the caller
 * mustn't free it while the sample is loaded.
 */
void fmod_DS_SFX_Load(sfxbuffer_t* buf, struct sfxsample_s* sample)
{
    if (!fmodSystem || !buf || !sample) return;

    bool streaming = (buf->flags & SFXBF_STREAM) != 0;

    // Tell the engine we have used up the entire sample already.
    buf->sample = sample;
    buf->written = sample->size;
    buf->flags &= ~SFXBF_RELOAD;

    BufferInfo& info = bufferInfo(buf);

    FMOD_CREATESOUNDEXINFO params;
    zeroStruct(params);
    params.length = sample->size;
    params.defaultfrequency = sample->rate;
    params.numchannels = 1; // Doomsday only uses mono samples currently.
    params.format = (sample->bytesPer == 1? FMOD_SOUND_FORMAT_PCM8 : FMOD_SOUND_FORMAT_PCM16);

    LOGDEV_AUDIO_XVERBOSE("[FMOD] SFX_Load: sfxbuffer %p sample (size:%i, freq:%i, bps:%i)",
                          buf << sample->size << sample->rate << sample->bytesPer);

    // If it has a sample, release it later.
    if (info.sound)
    {
        DE_GUARD(streams);
        LOGDEV_AUDIO_XVERBOSE("[FMOD] SFX_Load: Releasing buffer's old Sound %p", info.sound);
        FMOD_Sound_Release(info.sound);
        streams.value.erase(info.sound);
    }

    //RawSamplePCM8 signConverted;
    const char* sampleData = reinterpret_cast<const char*>(sample->data);
    if (!streaming)
    {
        /*if (sample->bytesPer == 1)
        {
            // Doomsday gives us unsigned 8-bit audio samples.
            toSigned8bit(reinterpret_cast<const unsigned char*>(sample->data), sample->size, signConverted);
            sampleData = &signConverted[0];
        }*/
        info.mode = FMOD_OPENMEMORY |
                    FMOD_OPENRAW |
                    (buf->flags & SFXBF_3D? FMOD_3D : FMOD_2D) |
                    (buf->flags & SFXBF_REPEAT? FMOD_LOOP_NORMAL : 0);
    }
    else // Set up for streaming.
    {
        info.mode = FMOD_OPENUSER |
                    FMOD_CREATESTREAM |
                    FMOD_LOOP_NORMAL;

        params.numchannels = 2; /// @todo  Make this configurable.
        params.length = sample->numSamples;
        params.decodebuffersize = sample->rate / 4;
        params.pcmreadcallback = pcmReadCallback;
        sampleData = 0; // will be streamed
    }
    if (buf->flags & SFXBF_3D)
    {
        info.mode |= FMOD_3D_WORLDRELATIVE;
    }

    // Pass the sample to FMOD.
    FMOD_RESULT result;
    result = FMOD_System_CreateSound(fmodSystem, sampleData, info.mode, &params, &info.sound);
    DSFMOD_ERRCHECK(result);
    LOGDEV_AUDIO_XVERBOSE("[FMOD] SFX_Load: created Sound %p%s",
                          info.sound << (streaming? " as streaming" : ""));

    if (streaming)
    {
        DE_GUARD(streams);
        // Keep a record of the playing stream for the PCM read callback.
        streams.value[info.sound] = buf;
        LOGDEV_AUDIO_XVERBOSE("[FMOD] SFX_Load: noting %p belongs to streaming buffer %p",
                              info.sound << buf);
    }

    // Not started yet.
    info.channel = 0;

#ifdef _DEBUG
    // Check memory.
    int currentAlloced = 0;
    int maxAlloced = 0;
    FMOD_Memory_GetStats(&currentAlloced, &maxAlloced, false);
    DSFMOD_TRACE("SFX_Load: FMOD memory alloced:" << currentAlloced << ", max:" << maxAlloced);
#endif

    // Now the buffer is ready for playing.
}

/**
 * Stops the buffer and makes it forget about its sample.
 */
void fmod_DS_SFX_Reset(sfxbuffer_t* buf)
{
    if (!buf)
        return;

    LOGDEV_AUDIO_XVERBOSE("[FMOD] SFX_Reset: sfxbuffer %p", buf);

    fmod_DS_SFX_Stop(buf);
    buf->sample = 0;
    buf->flags &= ~SFXBF_RELOAD;

    BufferInfo& info = bufferInfo(buf);
    if (info.sound)
    {
        DE_GUARD(streams);
        LOGDEV_AUDIO_XVERBOSE("[FMOD] SFX_Reset: releasing Sound %p", info.sound);
        FMOD_Sound_Release(info.sound);
        streams.value.erase(info.sound);
    }
    if (info.channel)
    {
        FMOD_Channel_SetCallback(info.channel, 0);
        FMOD_Channel_SetUserData(info.channel, 0);
        FMOD_Channel_SetMute(info.channel, true);
    }
    info = BufferInfo();
}

void fmod_DS_SFX_Play(sfxbuffer_t* buf)
{
    // Playing is quite impossible without a sample.
    if (!buf || !buf->sample)
        return;

    BufferInfo& info = bufferInfo(buf);
    assert(info.sound != 0);

    FMOD_RESULT result;
    result = FMOD_System_PlaySound(fmodSystem,
                                   info.sound,
                                   nullptr,
                                   true,
                                   &info.channel);
    DSFMOD_ERRCHECK(result);

    if (!info.channel) return;

    // Set the properties of the sound.
    FMOD_Channel_SetPan(info.channel, info.pan);
    FMOD_Channel_SetFrequency(info.channel, float(buf->freq));
    FMOD_Channel_SetVolume(info.channel, info.volume);
    FMOD_Channel_SetUserData(info.channel, buf);
    FMOD_Channel_SetCallback(info.channel, channelCallback);
    if (buf->flags & SFXBF_3D)
    {
        // 3D properties.
        FMOD_Channel_Set3DMinMaxDistance(info.channel,
                                         info.minDistanceMeters,
                                         info.maxDistanceMeters);
        FMOD_Channel_Set3DAttributes(info.channel, &info.position, &info.velocity, 0);
        FMOD_Channel_SetMode(info.channel, info.mode);
    }

    LOGDEV_AUDIO_XVERBOSE("[FMOD] SFX_Play: sfxbuffer %p, pan:%f, freq:%i, vol:%f, loop:%b",
            buf << info.pan << buf->freq << info.volume << ((buf->flags & SFXBF_REPEAT) != 0));

    // Start playing it.
    FMOD_Channel_SetPaused(info.channel, false);

    // The buffer is now playing.
    buf->flags |= SFXBF_PLAYING;
}

void fmod_DS_SFX_Stop(sfxbuffer_t* buf)
{
    if (!buf) return;

    LOGDEV_AUDIO_XVERBOSE("[FMOD] SFX_Stop: sfxbuffer %p", buf);

    BufferInfo& info = bufferInfo(buf);
    {
        DE_GUARD(streams);
        Streams::iterator found = streams.value.find(info.sound);
        if (found != streams.value.end() && info.channel)
        {
            FMOD_Channel_SetPaused(info.channel, true);
        }
    }
    if (info.channel)
    {
        FMOD_Channel_SetUserData(info.channel, 0);
        FMOD_Channel_SetCallback(info.channel, 0);
        FMOD_Channel_SetMute(info.channel, true);
        info.channel = 0;
    }

    // Clear the flag that tells the Sfx module about playing buffers.
    buf->flags &= ~SFXBF_PLAYING;
}

/**
 * Buffer streamer. Called by the Sfx refresh thread.
 * FMOD handles this for us.
 */
void fmod_DS_SFX_Refresh(sfxbuffer_t*)
{}

/**
 * @param prop          SFXBP_VOLUME (0..1)
 *                      SFXBP_FREQUENCY (Hz)
 *                      SFXBP_PAN (-1..1)
 *                      SFXBP_MIN_DISTANCE
 *                      SFXBP_MAX_DISTANCE
 *                      SFXBP_RELATIVE_MODE
 */
void fmod_DS_SFX_Set(sfxbuffer_t* buf, int prop, float value)
{
    if (!buf)
        return;

    BufferInfo& info = bufferInfo(buf);

    switch (prop)
    {
    case SFXBP_VOLUME:
        if (FEQUAL(info.volume, value)) return; // No change.
        assert(value >= 0);
        info.volume = value;
        if (info.channel) FMOD_Channel_SetVolume(info.channel, info.volume);
        break;

    case SFXBP_FREQUENCY: {
        unsigned int newFreq = (unsigned int) (buf->rate * value);
        if (buf->freq == newFreq) return; // No change.
        buf->freq = newFreq;
        if (info.channel) FMOD_Channel_SetFrequency(info.channel, float(buf->freq));
        break; }

    case SFXBP_PAN:
        if (FEQUAL(info.pan, value)) return; // No change.
        info.pan = value;
        if (info.channel) FMOD_Channel_SetPan(info.channel, info.pan);
        break;

    case SFXBP_MIN_DISTANCE:
        info.minDistanceMeters = value;
        if (info.channel) FMOD_Channel_Set3DMinMaxDistance(info.channel,
                                                           info.minDistanceMeters,
                                                           info.maxDistanceMeters);
        break;

    case SFXBP_MAX_DISTANCE:
        info.maxDistanceMeters = value;
        if (info.channel) FMOD_Channel_Set3DMinMaxDistance(info.channel,
                                                           info.minDistanceMeters,
                                                           info.maxDistanceMeters);
        break;

    case SFXBP_RELATIVE_MODE:
        info.setRelativeMode(value > 0);
        break;

    default:
        break;
    }

    //DSFMOD_TRACE("SFX_Set: sfxbuffer " << buf << ", " << sfxPropToString(prop) << " = " << value);
}

/**
 * Coordinates specified in world coordinate system:
 * +X to the right, +Y up and +Z away (Y and Z swapped, i.e.).
 *
 * @param property      SFXBP_POSITION
 *                      SFXBP_VELOCITY
 */
void fmod_DS_SFX_Setv(sfxbuffer_t* buf, int prop, float* values)
{
    if (!fmodSystem || !buf) return;

    BufferInfo& info = bufferInfo(buf);

    switch (prop)
    {
    case SFXBP_POSITION:
        info.position.set(values);
        if (info.channel)
            FMOD_Channel_Set3DAttributes(info.channel, &info.position,
                                         &info.velocity, 0);
        break;

    case SFXBP_VELOCITY:
        info.velocity.set(values);
        if (info.channel)
            FMOD_Channel_Set3DAttributes(info.channel, &info.position,
                                         &info.velocity, 0);
        break;

    default:
        break;
    }
}

/**
 * @param property      SFXLP_UNITS_PER_METER
 *                      SFXLP_DOPPLER
 *                      SFXLP_UPDATE
 */
void fmod_DS_SFX_Listener(int prop, float value)
{
    switch (prop)
    {
    case SFXLP_UNITS_PER_METER:
        unitsPerMeter = value;
        FMOD_System_Set3DSettings(fmodSystem, dopplerScale, unitsPerMeter, 1.0f);
        DSFMOD_TRACE("SFX_Listener: Units per meter = " << unitsPerMeter);
        break;

    case SFXLP_DOPPLER:
        dopplerScale = value;
        FMOD_System_Set3DSettings(fmodSystem, dopplerScale, unitsPerMeter, 1.0f);
        DSFMOD_TRACE("SFX_Listener: Doppler factor = " << value);
        break;

    case SFXLP_UPDATE:
        // Update the properties set with Listenerv.
        FMOD_System_Set3DListenerAttributes(fmodSystem, 0,
                                            &listener.position, &listener.velocity,
                                            &listener.front, &listener.up);
        break;

    default:
        break;
    }
}

/**
 * Convert linear volume 0..1 to a logarithmic range.
 */
static float linearToLog(float linear)
{
    return 10.f * std::log10(linear);
}

/**
 * Convert dB value to a linear 0..1 value.
 */
static float logToLinear(float db)
{
    return std::pow(10.f, db/10.f);
}

//static float scaleLogarithmic(float db, float scale, const de::Rangef &range)
//{
//    return range.clamp(linearToLog(scale * logToLinear(db)));
//}

/**
 * Update the ambient reverb properties.
 *
 * @param reverb  Array of NUM_REVERB_DATA parameters (see SRD_*).
 */
static void updateListenerEnvironmentSettings(float *reverb)
{
    if (!fmodSystem || !reverb) return;

    DSFMOD_TRACE("updateListenerEnvironmentSettings: " <<
                 reverb[0] << " " << reverb[1] << " " <<
                 reverb[2] << " " << reverb[3]);

    // No reverb?
    if (reverb[SFXLP_REVERB_VOLUME] == 0 && reverb[SFXLP_REVERB_SPACE]   == 0 &&
        reverb[SFXLP_REVERB_DECAY]  == 0 && reverb[SFXLP_REVERB_DAMPING] == 0)
    {
        FMOD_REVERB_PROPERTIES noReverb = FMOD_PRESET_OFF;
        FMOD_System_SetReverbProperties(fmodSystem, 0, &noReverb);
        return;
    }

    static const FMOD_REVERB_PROPERTIES presetPlain       = FMOD_PRESET_PLAIN;
    static const FMOD_REVERB_PROPERTIES presetConcertHall = FMOD_PRESET_CONCERTHALL;
    static const FMOD_REVERB_PROPERTIES presetAuditorium  = FMOD_PRESET_AUDITORIUM;
    static const FMOD_REVERB_PROPERTIES presetCave        = FMOD_PRESET_CAVE;
    static const FMOD_REVERB_PROPERTIES presetGeneric     = FMOD_PRESET_GENERIC;
    static const FMOD_REVERB_PROPERTIES presetRoom        = FMOD_PRESET_ROOM;

    float space = reverb[SFXLP_REVERB_SPACE];
    if (reverb[SFXLP_REVERB_DECAY] > .5)
    {
        // This much decay needs at least the Generic environment.
        if (space < .2)
            space = .2f;
    }

    // Choose a preset based on the size of the space.
    FMOD_REVERB_PROPERTIES props;
    if (space >= 1)
        props = presetPlain;
    else if (space >= .8)
        props = presetConcertHall;
    else if (space >= .6)
        props = presetAuditorium;
    else if (space >= .4)
        props = presetCave;
    else if (space >= .2)
        props = presetGeneric;
    else
        props = presetRoom;

    // Overall reverb volume adjustment.
    //props.WetLevel = scaleLogarithmic(props.WetLevel, reverb[SFXLP_REVERB_VOLUME], de::Rangef(-80.f, 0.f));
    props.WetLevel = de::Rangef(-80.f, 0.f).clamp(linearToLog((logToLinear(props.WetLevel) + reverb[SFXLP_REVERB_VOLUME])/6.f));
    //setEAXdw(DSPROPERTY_EAXLISTENER_ROOM, volLinearToLog(rev[SFXLP_REVERB_VOLUME]));

    // Reverb decay.
    const float decayFactor = 1.f + (reverb[SFXLP_REVERB_DECAY] - .5f) * 1.5f;
    props.DecayTime = std::min(std::max(100.f, props.DecayTime * decayFactor), 20000.f);
    //mulEAXf(DSPROPERTY_EAXLISTENER_DECAYTIME, val, EAXLISTENER_MINDECAYTIME, EAXLISTENER_MAXDECAYTIME);

    // Damping.
    //props.HighCut = de::Rangef(20, 20000).clamp(20000 * std::pow(1.f - reverb[SFXLP_REVERB_DAMPING], 2.f));
    props.HighCut = de::Rangef(20, 20000).clamp(props.HighCut * std::pow(1.f - reverb[SFXLP_REVERB_DAMPING], 2.f));
    //const float damping = std::max(.1f, 1.1f * (1.2f - reverb[SFXLP_REVERB_DAMPING]));
    //props.RoomHF = linearToLog(std::pow(10.f, props.RoomHF / 2000.f) * damping);
    //mulEAXdw(DSPROPERTY_EAXLISTENER_ROOMHF, val);

    std::ostringstream os;
    os << "WetLevel: " << props.WetLevel << " dB "
       << "input: " << reverb[SFXLP_REVERB_VOLUME] << " DecayTime: " << props.DecayTime << " ms "
       << "HighCut: " << props.HighCut << " Hz";
    de::debug(os.str().c_str());

    // A slightly increased roll-off. (Not in FMOD?)
    //props.RoomRolloffFactor = 1.3f;

    FMOD_System_SetReverbProperties(fmodSystem, 0, &props);
}

/**
 * @param prop  SFXLP_ORIENTATION  (yaw, pitch) in degrees.
 */
void fmod_DS_SFX_Listenerv(int prop, float* values)
{
    switch (prop)
    {
    case SFXLP_POSITION:
        listener.position.set(values);
        //DSFMOD_TRACE("Pos:" << values[0] << "," << values[1] << "," << values[2]);
        break;

    case SFXLP_ORIENTATION:
        // Convert the angles to front and up vectors.
        listener.setOrientation(float(values[0]/180*M_PI), float(values[1]/180*M_PI));
        break;

    case SFXLP_VELOCITY:
        listener.velocity.set(values);
        break;

    case SFXLP_REVERB:
        updateListenerEnvironmentSettings(values);
        break;

    case SFXLP_PRIMARY_FORMAT:
        DSFMOD_TRACE("SFX_Listenerv: Ignoring SFXLP_PRIMARY_FORMAT.");
        return;

    default:
        return;
    }
}

/**
 * Gets a driver property.
 *
 * @param prop    Property (SFXP_*).
 * @param values  Pointer to return value(s).
 */
int fmod_DS_SFX_Getv(int prop, void *values)
{
    switch (prop)
    {
    case SFXIP_DISABLE_CHANNEL_REFRESH: {
        /// The return value is a single 32-bit int.
        int *wantDisable = reinterpret_cast<int *>(values);
        if (wantDisable)
        {
            // Channel refresh is handled by FMOD, so we don't need to do anything.
            *wantDisable = true;
        }
        break; }

    case SFXIP_ANY_SAMPLE_RATE_ACCEPTED: {
        int *anySampleRate = reinterpret_cast<int *>(values);
        if (anySampleRate)
        {
            // FMOD can resample on the fly as needed.
            *anySampleRate = true;
        }
        break; }

    default:
        return false;
    }
    return true;
}
