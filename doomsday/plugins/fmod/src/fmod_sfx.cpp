/**
 * @file fmod_sfx.cpp
 * Sound effects interface. @ingroup dsfmod
 *
 * @authors Copyright © 2011-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "driver_fmod.h"
#include "dd_share.h"
#include <stdlib.h>
#include <cmath>
#include <vector>
#include <map>

typedef std::map<FMOD::Sound*, sfxbuffer_t*> Streams;
typedef std::vector<char> RawSamplePCM8;

struct BufferInfo
{
    FMOD::Channel* channel;
    FMOD::Sound* sound;
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
        if(newMode) {
            mode &= ~FMOD_3D_WORLDRELATIVE;
            mode |=  FMOD_3D_HEADRELATIVE;
        }
        else {
            mode |=  FMOD_3D_WORLDRELATIVE;
            mode &= ~FMOD_3D_HEADRELATIVE;
        }
        if(channel) channel->setMode(mode);
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
static Streams streams;

const char* sfxPropToString(int prop)
{
    switch(prop)
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

static FMOD_RESULT F_CALLBACK channelCallback(FMOD_CHANNEL* chanPtr,
                                              FMOD_CHANNEL_CALLBACKTYPE type,
                                              void* /*commanddata1*/,
                                              void* /*commanddata2*/)
{
    FMOD::Channel *channel = reinterpret_cast<FMOD::Channel*>(chanPtr);
    sfxbuffer_t *buf = 0;

    switch(type)
    {
    case FMOD_CHANNEL_CALLBACKTYPE_END:
        // The sound has ended, mark the channel.
        channel->getUserData(reinterpret_cast<void**>(&buf));
        if(buf)
        {
            DSFMOD_TRACE("channelCallback: sfxbuffer " << buf << " stops.");
            buf->flags &= ~SFXBF_PLAYING;
            // The channel becomes invalid after the sound stops.
            bufferInfo(buf).channel = 0;
        }
        channel->setCallback(0);
        channel->setUserData(0);
        break;

    default:
        break;
    }
    return FMOD_OK;
}

int DS_SFX_Init(void)
{
    return fmodSystem != 0;
}

sfxbuffer_t* DS_SFX_CreateBuffer(int flags, int bits, int rate)
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

    DSFMOD_TRACE("SFX_CreateBuffer: Created sfxbuffer " << buf);

    return buf;
}

void DS_SFX_DestroyBuffer(sfxbuffer_t* buf)
{
    if(!buf) return;

    DSFMOD_TRACE("SFX_DestroyBuffer: Destroying sfxbuffer " << buf);

    BufferInfo& info = bufferInfo(buf);
    if(info.sound)
    {
        info.sound->release();
        streams.erase(info.sound);
    }

    // Free the memory allocated for the buffer.
    delete reinterpret_cast<BufferInfo*>(buf->ptr);
    free(buf);
}

static void toSigned8bit(const unsigned char* source, int size, RawSamplePCM8& output)
{
    output.clear();
    output.resize(size);
    for(int i = 0; i < size; ++i)
    {
        output[i] = char(source[i]) - 128;
    }
}

static FMOD_RESULT F_CALLBACK pcmReadCallback(FMOD_SOUND* soundPtr, void* data, unsigned int datalen)
{
    FMOD::Sound* sound = reinterpret_cast<FMOD::Sound*>(soundPtr);

    Streams::iterator found = streams.find(sound);
    if(found == streams.end())
    {
        return FMOD_ERR_NOTREADY;
    }

    sfxbuffer_t* buf = found->second;
    DENG_ASSERT(buf != NULL);
    DENG_ASSERT(buf->flags & SFXBF_STREAM);

    // Call the stream callback.
    sfxstreamfunc_t func = reinterpret_cast<sfxstreamfunc_t>(buf->sample->data);
    if(func(buf, data, datalen))
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
void DS_SFX_Load(sfxbuffer_t* buf, struct sfxsample_s* sample)
{
    if(!fmodSystem || !buf || !sample) return;

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

    DSFMOD_TRACE("SFX_Load: sfxbuffer " << buf
                 << " sample (size:" << sample->size
                 << ", freq:" << sample->rate
                 << ", bps:" << sample->bytesPer << ")");

    // If it has a sample, release it later.
    if(info.sound)
    {
        DSFMOD_TRACE("SFX_Load: Releasing buffer's old Sound " << info.sound);
        info.sound->release();
        streams.erase(info.sound);
    }

    RawSamplePCM8 signConverted;
    const char* sampleData = reinterpret_cast<const char*>(sample->data);
    if(!streaming)
    {
        if(sample->bytesPer == 1)
        {
            // Doomsday gives us unsigned 8-bit audio samples.
            toSigned8bit(reinterpret_cast<const unsigned char*>(sample->data), sample->size, signConverted);
            sampleData = &signConverted[0];
        }
        info.mode = FMOD_OPENMEMORY |
                    FMOD_OPENRAW |
                    FMOD_HARDWARE |
                    (buf->flags & SFXBF_3D? FMOD_3D : FMOD_2D) |
                    (buf->flags & SFXBF_REPEAT? FMOD_LOOP_NORMAL : 0);
    }
    else // Set up for streaming.
    {
        info.mode = FMOD_OPENUSER |
                    FMOD_CREATESTREAM |
                    FMOD_HARDWARE |
                    FMOD_LOOP_NORMAL;

        params.numchannels = 2; /// @todo  Make this configurable.
        params.length = sample->numSamples;
        params.decodebuffersize = sample->rate / 4;
        params.pcmreadcallback = pcmReadCallback;
        sampleData = 0; // will be streamed
    }
    if(buf->flags & SFXBF_3D)
    {
        info.mode |= FMOD_3D_WORLDRELATIVE;
    }

    // Pass the sample to FMOD.
    FMOD_RESULT result;
    result = fmodSystem->createSound(sampleData, info.mode, &params, &info.sound);
    DSFMOD_ERRCHECK(result);
    DSFMOD_TRACE("SFX_Load: created Sound " << info.sound << (streaming? " as streaming" : ""));

    if(streaming)
    {
        // Keep a record of the playing stream for the PCM read callback.
        streams[info.sound] = buf;
        DSFMOD_TRACE("SFX_Load: noting " << info.sound << " belongs to streaming buffer " << buf);
    }

    // Not started yet.
    info.channel = 0;

#ifdef _DEBUG
    // Check memory.
    int currentAlloced = 0;
    int maxAlloced = 0;
    FMOD::Memory_GetStats(&currentAlloced, &maxAlloced, false);
    DSFMOD_TRACE("SFX_Load: FMOD memory alloced:" << currentAlloced << ", max:" << maxAlloced);
#endif

    // Now the buffer is ready for playing.
}

/**
 * Stops the buffer and makes it forget about its sample.
 */
void DS_SFX_Reset(sfxbuffer_t* buf)
{
    if(!buf)
        return;

    DSFMOD_TRACE("SFX_Reset: sfxbuffer " << buf);

    DS_SFX_Stop(buf);
    buf->sample = 0;
    buf->flags &= ~SFXBF_RELOAD;

    BufferInfo& info = bufferInfo(buf);
    if(info.sound)
    {
        DSFMOD_TRACE("SFX_Reset: releasing Sound " << info.sound);
        info.sound->release();
        streams.erase(info.sound);
    }
    if(info.channel)
    {
        info.channel->setCallback(0);
        info.channel->setUserData(0);
        info.channel->setMute(true);
    }
    info = BufferInfo();
}

void DS_SFX_Play(sfxbuffer_t* buf)
{
    // Playing is quite impossible without a sample.
    if(!buf || !buf->sample)
        return;

    BufferInfo& info = bufferInfo(buf);
    assert(info.sound != 0);

    FMOD_RESULT result;
    result = fmodSystem->playSound(FMOD_CHANNEL_FREE, info.sound, true, &info.channel);
    DSFMOD_ERRCHECK(result);

    if(!info.channel) return;

    // Set the properties of the sound.
    info.channel->setPan(info.pan);
    info.channel->setFrequency(float(buf->freq));
    info.channel->setVolume(info.volume);
    info.channel->setUserData(buf);
    info.channel->setCallback(channelCallback);
    if(buf->flags & SFXBF_3D)
    {
        // 3D properties.
        info.channel->set3DMinMaxDistance(info.minDistanceMeters,
                                          info.maxDistanceMeters);
        info.channel->set3DAttributes(&info.position, &info.velocity);
        info.channel->setMode(info.mode);
    }

    DSFMOD_TRACE("SFX_Play: sfxbuffer " << buf <<
                 ", pan:" << info.pan <<
                 ", freq:" << buf->freq <<
                 ", vol:" << info.volume <<
                 ", loop:" << ((buf->flags & SFXBF_REPEAT) != 0));

    // Start playing it.
    info.channel->setPaused(false);

    // The buffer is now playing.
    buf->flags |= SFXBF_PLAYING;
}

void DS_SFX_Stop(sfxbuffer_t* buf)
{
    if(!buf) return;

    DSFMOD_TRACE("SFX_Stop: sfxbuffer " << buf);

    BufferInfo& info = bufferInfo(buf);
    if(info.channel)
    {
        info.channel->setUserData(0);
        info.channel->setCallback(0);
        info.channel->setMute(true);
        info.channel = 0;
    }

    // Clear the flag that tells the Sfx module about playing buffers.
    buf->flags &= ~SFXBF_PLAYING;
}

/**
 * Buffer streamer. Called by the Sfx refresh thread.
 * FMOD handles this for us.
 */
void DS_SFX_Refresh(sfxbuffer_t*)
{}

/**
 * @param prop          SFXBP_VOLUME (0..1)
 *                      SFXBP_FREQUENCY (Hz)
 *                      SFXBP_PAN (-1..1)
 *                      SFXBP_MIN_DISTANCE
 *                      SFXBP_MAX_DISTANCE
 *                      SFXBP_RELATIVE_MODE
 */
void DS_SFX_Set(sfxbuffer_t* buf, int prop, float value)
{
    if(!buf)
        return;

    BufferInfo& info = bufferInfo(buf);

    switch(prop)
    {
    case SFXBP_VOLUME:
        if(FEQUAL(info.volume, value)) return; // No change.
        assert(value >= 0);
        info.volume = value;
        if(info.channel) info.channel->setVolume(info.volume);
        break;

    case SFXBP_FREQUENCY: {
        unsigned int newFreq = (unsigned int) (buf->rate * value);
        if(buf->freq == newFreq) return; // No change.
        buf->freq = newFreq;
        if(info.channel) info.channel->setFrequency(float(buf->freq));
        break; }

    case SFXBP_PAN:
        if(FEQUAL(info.pan, value)) return; // No change.
        info.pan = value;
        if(info.channel) info.channel->setPan(info.pan);
        break;

    case SFXBP_MIN_DISTANCE:
        info.minDistanceMeters = value;
        if(info.channel) info.channel->set3DMinMaxDistance(info.minDistanceMeters,
                                                           info.maxDistanceMeters);
        break;

    case SFXBP_MAX_DISTANCE:
        info.maxDistanceMeters = value;
        if(info.channel) info.channel->set3DMinMaxDistance(info.minDistanceMeters,
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
void DS_SFX_Setv(sfxbuffer_t* buf, int prop, float* values)
{
    if(!fmodSystem || !buf) return;

    BufferInfo& info = bufferInfo(buf);

    switch(prop)
    {
    case SFXBP_POSITION:
        info.position.set(values);
        if(info.channel) info.channel->set3DAttributes(&info.position, &info.velocity);
        break;

    case SFXBP_VELOCITY:
        info.velocity.set(values);
        if(info.channel) info.channel->set3DAttributes(&info.position, &info.velocity);
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
void DS_SFX_Listener(int prop, float value)
{
    switch(prop)
    {
    case SFXLP_UNITS_PER_METER:
        unitsPerMeter = value;
        fmodSystem->set3DSettings(dopplerScale, unitsPerMeter, 1.0f);
        DSFMOD_TRACE("SFX_Listener: Units per meter = " << unitsPerMeter);
        break;

    case SFXLP_DOPPLER:
        dopplerScale = value;
        fmodSystem->set3DSettings(dopplerScale, unitsPerMeter, 1.0f);
        DSFMOD_TRACE("SFX_Listener: Doppler factor = " << value);
        break;

    case SFXLP_UPDATE:
        // Update the properties set with Listenerv.
        fmodSystem->set3DListenerAttributes(0, &listener.position, &listener.velocity,
                                            &listener.front, &listener.up);
        break;

    default:
        break;
    }
}

/**
 * Convert linear volume 0..1 to logarithmic -10000..0.
 */
static int linearToLog(float vol)
{
    if(vol <= 0) return -10000;
    if(vol >= 1) return 0;

    // Straighten the volume curve.
    return std::min(std::max(-10000, (int) (100 * 20 * std::log10(vol))), 0);
}

/**
 * Update the ambient reverb properties.
 *
 * @param reverb  Array of NUM_REVERB_DATA parameters (see SRD_*).
 */
static void updateListenerEnvironmentSettings(float* reverb)
{
    if(!fmodSystem || !reverb) return;

    DSFMOD_TRACE("updateListenerEnvironmentSettings: " <<
                 reverb[0] << " " << reverb[1] << " " <<
                 reverb[2] << " " << reverb[3]);

    // No reverb?
    if(reverb[SRD_VOLUME] == 0 && reverb[SRD_SPACE]   == 0 &&
       reverb[SRD_DECAY]  == 0 && reverb[SRD_DAMPING] == 0)
    {
        FMOD_REVERB_PROPERTIES noReverb = FMOD_PRESET_OFF;
        fmodSystem->setReverbAmbientProperties(&noReverb);
        return;
    }

    const static FMOD_REVERB_PROPERTIES presetPlain       = FMOD_PRESET_PLAIN;
    const static FMOD_REVERB_PROPERTIES presetConcertHall = FMOD_PRESET_CONCERTHALL;
    const static FMOD_REVERB_PROPERTIES presetAuditorium  = FMOD_PRESET_AUDITORIUM;
    const static FMOD_REVERB_PROPERTIES presetCave        = FMOD_PRESET_CAVE;
    const static FMOD_REVERB_PROPERTIES presetGeneric     = FMOD_PRESET_GENERIC;
    const static FMOD_REVERB_PROPERTIES presetRoom        = FMOD_PRESET_ROOM;

    float space = reverb[SRD_SPACE];
    if(reverb[SRD_DECAY] > .5)
    {
        // This much decay needs at least the Generic environment.
        if(space < .2)
            space = .2f;
    }

    // Choose a preset based on the size of the space.
    FMOD_REVERB_PROPERTIES props;
    if(space >= 1)
        props = presetPlain;
    else if(space >= .8)
        props = presetConcertHall;
    else if(space >= .6)
        props = presetAuditorium;
    else if(space >= .4)
        props = presetCave;
    else if(space >= .2)
        props = presetGeneric;
    else
        props = presetRoom;

    // Overall reverb volume adjustment.
    props.Room = linearToLog(reverb[SRD_VOLUME]);
    //setEAXdw(DSPROPERTY_EAXLISTENER_ROOM, volLinearToLog(rev[SRD_VOLUME]));

    // Reverb decay.
    float decay = (reverb[SRD_DECAY] - .5f) * 1.5f + 1;
    props.DecayTime = std::min(std::max(0.1f, props.DecayTime * decay), 20.f);
    //mulEAXf(DSPROPERTY_EAXLISTENER_DECAYTIME, val, EAXLISTENER_MINDECAYTIME, EAXLISTENER_MAXDECAYTIME);

    // Damping.
    float damping = std::max(.1f, 1.1f * (1.2f - reverb[SRD_DAMPING]));
    props.RoomHF = linearToLog(std::pow(10.f, props.RoomHF / 2000.f) * damping);
    //mulEAXdw(DSPROPERTY_EAXLISTENER_ROOMHF, val);

    // A slightly increased roll-off. (Not in FMOD?)
    //props.RoomRolloffFactor = 1.3f;

    fmodSystem->setReverbAmbientProperties(&props);
}

/**
 * @param prop  SFXLP_ORIENTATION  (yaw, pitch) in degrees.
 */
void DS_SFX_Listenerv(int prop, float* values)
{
    switch(prop)
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
int DS_SFX_Getv(int prop, void* values)
{
    switch(prop)
    {
    case SFXIP_DISABLE_CHANNEL_REFRESH: {
        /// The return value is a single 32-bit int.
        int* wantDisable = reinterpret_cast<int*>(values);
        if(wantDisable)
        {
            // Channel refresh is handled by FMOD, so we don't need to do anything.
            *wantDisable = true;
        }
        break; }

    default:
        return false;
    }
    return true;
}
