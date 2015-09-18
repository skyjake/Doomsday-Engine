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

#include "audio/sound.h"
#include <de/Log>
#include <de/memoryzone.h>
#include <de/timer.h>

using namespace de;

namespace audio {

/**
 * @param buf  Sound buffer.
 * @return The length of the buffer in milliseconds.
 */
static duint getBufferLength(sfxbuffer_t &buf)
{
    DENG2_ASSERT(buf.sample);
    return 1000 * buf.sample->numSamples / buf.freq;
}

// ----------------------------------------------------------------------------------

DummyDriver::CdPlayer::CdPlayer(DummyDriver &driver) : ICdPlayer(driver)
{}

String DummyDriver::CdPlayer::name() const
{
    return "cd";
}

dint DummyDriver::CdPlayer::init()
{
    return _initialized = true;
}

void DummyDriver::CdPlayer::shutdown()
{
    _initialized = false;
}

void DummyDriver::CdPlayer::update()
{}

void DummyDriver::CdPlayer::setVolume(dfloat)
{}

bool DummyDriver::CdPlayer::isPlaying() const
{
    return false;
}

void DummyDriver::CdPlayer::pause(dint)
{}

void DummyDriver::CdPlayer::stop()
{}

dint DummyDriver::CdPlayer::play(dint, dint)
{
    return true;
}

// ----------------------------------------------------------------------------------

DummyDriver::MusicPlayer::MusicPlayer(DummyDriver &driver) : IMusicPlayer(driver)
{}

String DummyDriver::MusicPlayer::name() const
{
    return "music";
}

dint DummyDriver::MusicPlayer::init()
{
    return _initialized = true;
}

void DummyDriver::MusicPlayer::shutdown()
{
    _initialized = false;
}

void DummyDriver::MusicPlayer::update()
{}

void DummyDriver::MusicPlayer::setVolume(dfloat)
{}

bool DummyDriver::MusicPlayer::isPlaying() const
{
    return false;
}

void DummyDriver::MusicPlayer::pause(dint)
{}

void DummyDriver::MusicPlayer::stop()
{}

bool DummyDriver::MusicPlayer::canPlayBuffer() const
{
    return false;  /// @todo Should support this...
}

void *DummyDriver::MusicPlayer::songBuffer(duint)
{
    return nullptr;
}

dint DummyDriver::MusicPlayer::play(dint)
{
    return true;
}

bool DummyDriver::MusicPlayer::canPlayFile() const
{
    return true;
}

dint DummyDriver::MusicPlayer::playFile(char const *, dint)
{
    return true;
}

// ----------------------------------------------------------------------------------

DummyDriver::SoundPlayer::SoundPlayer(DummyDriver &driver) : ISoundPlayer(driver)
{}

String DummyDriver::SoundPlayer::name() const
{
    return "sfx";
}

dint DummyDriver::SoundPlayer::init()
{
    return _initialized = true;
}

void DummyDriver::SoundPlayer::destroy(sfxbuffer_t &buf)
{
    // Free the memory allocated for the buffer.
    Z_Free(&buf);
}

sfxbuffer_t *DummyDriver::SoundPlayer::create(dint flags, dint bits, dint rate)
{
    /// @todo fixme: We have ownership - ensure the buffer is destroyed when the
    /// SoundPlayer is. -ds
    auto *buf = (sfxbuffer_t *) Z_Calloc(sizeof(sfxbuffer_t), PU_APPSTATIC, 0);

    buf->bytes = bits / 8;
    buf->rate  = rate;
    buf->flags = flags;
    buf->freq  = rate;  // Modified by calls to Set(SFXBP_FREQUENCY).

    return buf;
}

Sound *DummyDriver::SoundPlayer::makeSound(bool stereoPositioning, dint bitsPer, dint rate)
{
    std::unique_ptr<Sound> sound(new Sound(*this));
    sound->setBuffer(create(stereoPositioning ? 0 : SFXBF_3D, bitsPer, rate));
    return sound.release();
}

bool DummyDriver::SoundPlayer::anyRateAccepted() const
{
    // We are not playing any audio so yeah, whatever.
    return true;
}

void DummyDriver::SoundPlayer::stop(sfxbuffer_t &buf)
{
    // Clear the flag that tells the Sfx module about playing buffers.
    buf.flags &= ~SFXBF_PLAYING;

    // If the sound is started again, it needs to be reloaded.
    buf.flags |= SFXBF_RELOAD;
}

void DummyDriver::SoundPlayer::reset(sfxbuffer_t &buf)
{
    stop(buf);
    buf.sample = nullptr;
    buf.flags &= ~SFXBF_RELOAD;
}

void DummyDriver::SoundPlayer::load(sfxbuffer_t &buf, sfxsample_t &sample)
{
    // Now the buffer is ready for playing.
    buf.sample  = &sample;
    buf.written = sample.size;
    buf.flags  &= ~SFXBF_RELOAD;
}

void DummyDriver::SoundPlayer::play(sfxbuffer_t &buf)
{
    // Playing is quite impossible without a sample.
    if(!buf.sample) return;

    // Do we need to reload?
    if(buf.flags & SFXBF_RELOAD)
    {
        load(buf, *buf.sample);
    }

    // The sound starts playing now?
    if(!isPlaying(buf))
    {
        // Calculate the end time (milliseconds).
        buf.endTime = Timer_RealMilliseconds() + getBufferLength(buf);
    }

    // The buffer is now playing.
    buf.flags |= SFXBF_PLAYING;
}

bool DummyDriver::SoundPlayer::isPlaying(sfxbuffer_t &buf) const
{
    return (buf.flags & SFXBF_PLAYING) != 0;
}

void DummyDriver::SoundPlayer::refresh(sfxbuffer_t &buf)
{
    // Can only be done if there is a sample and the buffer is playing.
    if(!buf.sample || !isPlaying(buf))
        return;

    // Have we passed the predicted end of sample?
    if(!(buf.flags & SFXBF_REPEAT) && Timer_RealMilliseconds() >= buf.endTime)
    {
        // Time for the sound to stop.
        stop(buf);
    }
}

bool DummyDriver::SoundPlayer::needsRefresh() const
{
    // We are not playing any audio so no.
    return false;
}

void DummyDriver::SoundPlayer::setFrequency(sfxbuffer_t &buf, dfloat newFrequency)
{
    buf.freq = buf.rate * newFrequency;
}

void DummyDriver::SoundPlayer::setOrigin(sfxbuffer_t &, Vector3d const &)
{
    // Not supported.
}

void DummyDriver::SoundPlayer::setPan(sfxbuffer_t &, dfloat)
{
    // Not supported.
}

void DummyDriver::SoundPlayer::setPositioning(sfxbuffer_t &, bool)
{
    // Not supported.
}

void DummyDriver::SoundPlayer::setVelocity(sfxbuffer_t &, Vector3d const &)
{
    // Not supported.
}

void DummyDriver::SoundPlayer::setVolume(sfxbuffer_t &, dfloat)
{
    // Not supported.
}

void DummyDriver::SoundPlayer::setVolumeAttenuationRange(sfxbuffer_t &, Ranged const &)
{
    // Not supported.
}

void DummyDriver::SoundPlayer::listener(dint, dfloat)
{
    // Not supported.
}

void DummyDriver::SoundPlayer::listenerv(dint, dfloat *)
{
    // Not supported.
}

// ----------------------------------------------------------------------------------

DENG2_PIMPL(DummyDriver)
{
    bool initialized = false;

    CdPlayer iCd;
    MusicPlayer iMusic;
    SoundPlayer iSfx;

    Instance(Public *i)
        : Base(i)
        , iCd   (self)
        , iMusic(self)
        , iSfx  (self)
    {}

    ~Instance()
    {
        // Should have been deinitialized by now.
        DENG2_ASSERT(!initialized);
    }
};

DummyDriver::DummyDriver() : d(new Instance(this))
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

ICdPlayer /*const*/ &DummyDriver::iCd() const
{
    return d->iCd;
}

IMusicPlayer /*const*/ &DummyDriver::iMusic() const
{
    return d->iMusic;
}

ISoundPlayer /*const*/ &DummyDriver::iSfx() const
{
    return d->iSfx;
}

}  // namespace audio
