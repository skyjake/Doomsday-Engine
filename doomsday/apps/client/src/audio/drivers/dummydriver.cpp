/** @file dummydriver.cpp  Dummy audio driver.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2007-2015 Daniel Swanson <danij@dengine.net>
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
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#include "audio/drivers/dummydriver.h"

#include "api_audiod_sfx.h"
#include <de/Log>
#include <de/memoryzone.h>
#include <de/timer.h>

using namespace de;

namespace audio {

static bool sfxInitialized;

static int DS_Dummy_SFX_Init()
{
    return sfxInitialized = true;
}

static void DS_Dummy_SFX_DestroyBuffer(sfxbuffer_t *buf)
{
    // Free the memory allocated for the buffer.
    Z_Free(buf);
}

static sfxbuffer_t *DS_Dummy_SFX_CreateBuffer(int flags, int bits, int rate)
{
    auto *buf = (sfxbuffer_t *) Z_Calloc(sizeof(sfxbuffer_t), PU_APPSTATIC, 0);

    buf->bytes = bits / 8;
    buf->rate  = rate;
    buf->flags = flags;
    buf->freq  = rate;  // Modified by calls to Set(SFXBP_FREQUENCY).

    return buf;
}

/**
 * @param buf  Sound buffer.
 * @return The length of the buffer in milliseconds.
 */
static uint DS_DummyBufferLength(sfxbuffer_t *buf)
{
    DENG2_ASSERT(buf && buf->sample);
    return 1000 * buf->sample->numSamples / buf->freq;
}

static void DS_Dummy_SFX_Stop(sfxbuffer_t *buf)
{
    DENG2_ASSERT(buf);

    // Clear the flag that tells the Sfx module about playing buffers.
    buf->flags &= ~SFXBF_PLAYING;

    // If the sound is started again, it needs to be reloaded.
    buf->flags |= SFXBF_RELOAD;
}

static void DS_Dummy_SFX_Reset(sfxbuffer_t *buf)
{
    DENG2_ASSERT(buf);

    DS_Dummy_SFX_Stop(buf);
    buf->sample = nullptr;
    buf->flags &= ~SFXBF_RELOAD;
}

static void DS_Dummy_SFX_Load(sfxbuffer_t *buf, struct sfxsample_s *sample)
{
    DENG2_ASSERT(buf && sample);

    // Now the buffer is ready for playing.
    buf->sample  = sample;
    buf->written = sample->size;
    buf->flags  &= ~SFXBF_RELOAD;
}

static void DS_Dummy_SFX_Play(sfxbuffer_t *buf)
{
    DENG2_ASSERT(buf);

    // Playing is quite impossible without a sample.
    if(!buf->sample) return;

    // Do we need to reload?
    if(buf->flags & SFXBF_RELOAD)
    {
        DS_Dummy_SFX_Load(buf, buf->sample);
    }

    // The sound starts playing now?
    if(!(buf->flags & SFXBF_PLAYING))
    {
        // Calculate the end time (milliseconds).
        buf->endTime = Timer_RealMilliseconds() + DS_DummyBufferLength(buf);
    }

    // The buffer is now playing.
    buf->flags |= SFXBF_PLAYING;
}

static void DS_Dummy_SFX_Refresh(sfxbuffer_t *buf)
{
    DENG2_ASSERT(buf);

    // Can only be done if there is a sample and the buffer is playing.
    if(!buf->sample || !(buf->flags & SFXBF_PLAYING))
        return;

    // Have we passed the predicted end of sample?
    if(!(buf->flags & SFXBF_REPEAT) && Timer_RealMilliseconds() >= buf->endTime)
    {
        // Time for the sound to stop.
        DS_Dummy_SFX_Stop(buf);
    }
}

static void DS_Dummy_SFX_Set(sfxbuffer_t *buf, int prop, float value)
{
    DENG2_ASSERT(buf);

    switch(prop)
    {
    case SFXBP_FREQUENCY:
        buf->freq = buf->rate * value;
        break;

    default: break;
    }
}

static void DS_Dummy_SFX_Setv(sfxbuffer_t * /*buf*/, int /*prop*/, float * /*values*/)
{
    // Nothing to do.
}

static int DS_Dummy_SFX_Getv(int prop, void *values)
{
    switch(prop)
    {
    case SFXIP_DISABLE_CHANNEL_REFRESH: {
        /// The return value is a single 32-bit int.
        auto *wantDisable = (int *) values;
        if(wantDisable)
        {
            // We are not playing any audio.
            *wantDisable = true;
        }
        return true; }

    default: return false;
    }
}

static void DS_Dummy_SFX_Listener(int /*prop*/, float /*value*/)
{
    // Nothing to do.
}

static void DS_Dummy_SFX_Listenerv(int /*prop*/, float * /*values*/)
{
    // Nothing to do.
}

DENG2_PIMPL_NOREF(DummyDriver)
{
    bool initialized = false;

    audiointerface_cd_t iCd;
    audiointerface_music_t iMusic;
    audiointerface_sfx_t iSfx;

    Instance()
    {
        de::zap(iCd);
        de::zap(iMusic);

        de::zap(iSfx);
        iSfx.gen.Init      = DS_Dummy_SFX_Init;
        iSfx.gen.Create    = DS_Dummy_SFX_CreateBuffer;
        iSfx.gen.Destroy   = DS_Dummy_SFX_DestroyBuffer;
        iSfx.gen.Load      = DS_Dummy_SFX_Load;
        iSfx.gen.Reset     = DS_Dummy_SFX_Reset;
        iSfx.gen.Play      = DS_Dummy_SFX_Play;
        iSfx.gen.Stop      = DS_Dummy_SFX_Stop;
        iSfx.gen.Refresh   = DS_Dummy_SFX_Refresh;
        iSfx.gen.Set       = DS_Dummy_SFX_Set;
        iSfx.gen.Setv      = DS_Dummy_SFX_Setv;
        iSfx.gen.Listener  = DS_Dummy_SFX_Listener;
        iSfx.gen.Listenerv = DS_Dummy_SFX_Listenerv;
        iSfx.gen.Getv      = DS_Dummy_SFX_Getv;
    }

    ~Instance()
    {
        // Should have been deinitialized by now.
        DENG2_ASSERT(!initialized);
    }
};

DummyDriver::DummyDriver() : d(new Instance)
{}

DummyDriver::~DummyDriver()
{
    LOG_AS("~audio::DummyDriver");
    deinitialize();  // If necessary.
}

void DummyDriver::initialize()
{
    LOG_AS("audio::DummyDriver");

    // Already been here?
    if(d->initialized) return;

    d->initialized = true;
}

void DummyDriver::deinitialize()
{
    LOG_AS("audio::DummyDriver");

    // Already been here?
    if(!d->initialized) return;

    d->initialized = false;
}

audio::System::IDriver::Status DummyDriver::status() const
{
    if(d->initialized) return Initialized;
    return Loaded;
}

String DummyDriver::description() const
{
    return "blah";
}

String DummyDriver::identityKey() const
{
    return "dummy";
}

String DummyDriver::title() const
{
    return "Dummy Driver";
}

bool DummyDriver::hasCd() const
{
    return false;
}

bool DummyDriver::hasMusic() const
{
    return false;
}

bool DummyDriver::hasSfx() const
{
    return d->initialized;
}

audiointerface_cd_t /*const*/ &DummyDriver::iCd() const
{
    return d->iCd;
}

audiointerface_music_t /*const*/ &DummyDriver::iMusic() const
{
    return d->iMusic;
}

audiointerface_sfx_t /*const*/ &DummyDriver::iSfx() const
{
    return d->iSfx;
}

String DummyDriver::interfaceName(void *playbackInterface) const
{
    if((void *)&d->iCd == playbackInterface)
    {
        return "Dummy/CD";
    }
    if((void *)&d->iMusic == playbackInterface)
    {
        return "Dummy/Music";
    }
    if((void *)&d->iSfx == playbackInterface)
    {
        return "Dummy/SFX";
    }

    return "";  // Not recognized.
}

}  // namespace audio
