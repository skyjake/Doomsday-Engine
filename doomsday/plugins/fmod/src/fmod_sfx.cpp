/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2011 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "driver_fmod.h"
#include "dd_share.h"
#include <stdlib.h>

struct BufferInfo
{
    FMOD::Channel* channel;
    FMOD::Sound* sound;
    float pan;
    float volume;

    BufferInfo() : channel(0), sound(0), pan(0.f), volume(1.f) {}
};

static const char* sfxPropToString(int prop)
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

#if 0
/**
 * @return The length of the buffer in milliseconds.
 */
static unsigned int bufferLength(sfxbuffer_t* buf)
{
    if(!buf || !buf->sample) return 0;
    return 1000 * buf->sample->numSamples / buf->freq;
}
#endif

sfxbuffer_t* DS_SFX_CreateBuffer(int flags, int bits, int rate)
{
    DSFMOD_TRACE("DS_SFX_CreateBuffer: flags=" << flags << ", bits=" << bits << ", rate=" << rate);

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

    DSFMOD_TRACE("DS_SFX_CreateBuffer: Created sfxbuffer " << buf);

    return buf;
}

void DS_SFX_DestroyBuffer(sfxbuffer_t* buf)
{
    if(!buf) return;

    DSFMOD_TRACE("DS_SFX_DestroyBuffer: Destroying sfxbuffer " << buf);

    // Free the memory allocated for the buffer.
    delete reinterpret_cast<BufferInfo*>(buf->ptr);
    free(buf);
}

/**
 * Prepare the buffer for playing a sample by filling the buffer with as
 * much sample data as fits. The pointer to sample is saved, so the caller
 * mustn't free it while the sample is loaded.
 */
void DS_SFX_Load(sfxbuffer_t* buf, struct sfxsample_s* sample)
{
    if(!fmodSystem || !buf || !sample)
        return;

    // Tell the engine we have used up the entire sample already.
    buf->sample = sample;
    buf->written = sample->size;
    buf->flags &= ~SFXBF_RELOAD;

    BufferInfo& info = bufferInfo(buf);

    FMOD_CREATESOUNDEXINFO params;
    memset(&params, 0, sizeof(params));
    params.cbsize = sizeof(params);
    params.length = sample->size;
    params.defaultfrequency = sample->rate;
    params.numchannels = 1; // Doomsday only uses mono samples currently.
    params.format = (sample->bytesPer == 1? FMOD_SOUND_FORMAT_PCM8 : FMOD_SOUND_FORMAT_PCM16);

    DSFMOD_TRACE("DS_SFX_Load: sfxbuffer " << buf
                 << " sample (size:" << sample->size
                 << ", freq:" << sample->rate
                 << ", bps:" << sample->bytesPer << ")");

    // Pass the sample to FMOD.
    FMOD_RESULT result;
    result = fmodSystem->createSound(reinterpret_cast<const char*>(sample->data),
                                     (buf->flags & SFXBF_3D? FMOD_3D : 0) |
                                     FMOD_OPENMEMORY | FMOD_OPENUSER, &params,
                                     &info.sound);
    DSFMOD_ERRCHECK(result);
    DSFMOD_TRACE("DS_SFX_Load: created Sound " << info.sound);

    // Not started yet.
    info.channel = 0;

    // Now the buffer is ready for playing.
}

/**
 * Stops the buffer and makes it forget about its sample.
 */
void DS_SFX_Reset(sfxbuffer_t* buf)
{
    if(!buf)
        return;

    DSFMOD_TRACE("DS_SFX_Reset: sfxbuffer " << buf);

    DS_SFX_Stop(buf);
    buf->sample = 0;
    buf->flags &= ~SFXBF_RELOAD;

    BufferInfo& info = bufferInfo(buf);
    if(info.sound)
    {
        DSFMOD_TRACE("DS_SFX_Reset: releasing Sound " << info.sound);
        info.sound->release();
    }
    if(info.channel)
    {
        info.channel->setCallback(0);
        info.channel->setUserData(0);
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
    info.channel->setFrequency(buf->freq);
    info.channel->setVolume(info.volume);
    info.channel->setUserData(buf);
    info.channel->setCallback(channelCallback);

    DSFMOD_TRACE("DS_SFX_Play: sfxbuffer " << buf <<
                 ", pan:" << info.pan <<
                 ", freq:" << buf->freq <<
                 ", vol:" << info.volume);

    // Start playing it.
    info.channel->setPaused(false);

    // The buffer is now playing.
    buf->flags |= SFXBF_PLAYING;
}

void DS_SFX_Stop(sfxbuffer_t* buf)
{
    if(!buf) return;

    DSFMOD_TRACE("DS_SFX_Stop: sfxbuffer " << buf);

    BufferInfo& info = bufferInfo(buf);
    if(info.channel)
    {
        info.channel->setUserData(0);
        info.channel->setCallback(0);
        info.channel->stop();
        info.channel = 0;
    }

    // Clear the flag that tells the Sfx module about playing buffers.
    buf->flags &= ~SFXBF_PLAYING;

    // If the sound is started again, it needs to be reloaded.
    //buf->flags |= SFXBF_RELOAD;
}

/**
 * Buffer streamer. Called by the Sfx refresh thread.
 * FMOD handles this for us.
 */
void DS_SFX_Refresh(sfxbuffer_t*)
{}

/**
 * @param prop          SFXBP_VOLUME (if negative, interpreted as attenuation)
 *                      SFXBP_FREQUENCY
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
        info.volume = value;
        if(info.channel) info.channel->setVolume(info.volume);
        break;

    case SFXBP_FREQUENCY: {
        int newFreq = buf->rate * value;
        if(buf->freq == newFreq) return; // No change.
        buf->freq = newFreq;
        if(info.channel) info.channel->setFrequency(buf->freq);
        break; }

    case SFXBP_PAN:
        if(FEQUAL(info.pan, value)) return; // No change.
        info.pan = value;
        if(info.channel) info.channel->setPan(info.pan);
        break;

    default:
        break;
    }

    DSFMOD_TRACE("DS_SFX_Set: sfxbuffer " << buf << ", "
                 << sfxPropToString(prop) << " = " << value);
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
    // Nothing to do.
}

/**
 * @param property      SFXLP_UNITS_PER_METER
 *                      SFXLP_DOPPLER
 *                      SFXLP_UPDATE
 */
void DS_SFX_Listener(int prop, float value)
{
    // Nothing to do.
}

static void setListenerEnvironment(float* rev)
{
    // Nothing to do.
}

void DS_SFX_Listenerv(int prop, float* values)
{
    // Nothing to do.
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
    case SFXP_DISABLE_CHANNEL_REFRESH: {
        /// For SFXP_DISABLE_CHANNEL_REFRESH, the return value is a single 32-bit int.
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
