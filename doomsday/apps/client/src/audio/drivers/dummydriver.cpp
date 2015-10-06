/** @file dummydriver.cpp  Dummy audio driver.
 *
 * @authors Copyright © 2003-2015 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2015 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2006-2007 Jamie Jones <jamie_jones_au@yahoo.com.au>
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
#include "world/thinkers.h"
#include "def_main.h"        // SF_* flags, remove me
#include <de/Log>
#include <de/Observers>
#include <de/timer.h>        // TICSPERSEC
#include <QList>
#include <QtAlgorithms>

using namespace de;

namespace audio {

static sfxbuffer_t *newBuffer(dint flags, dint bits, dint rate)
{
    auto *buf = new sfxbuffer_t;
    de::zapPtr(buf);
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
static duint getBufferLength(sfxbuffer_t &buf)
{
    DENG2_ASSERT(buf.sample);
    return 1000 * buf.sample->numSamples / buf.freq;
}

// ----------------------------------------------------------------------------------

DummyDriver::CdPlayer::CdPlayer()
{}

dint DummyDriver::CdPlayer::initialize()
{
    return _initialized = true;
}

void DummyDriver::CdPlayer::deinitialize()
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

DummyDriver::MusicPlayer::MusicPlayer()
{}

dint DummyDriver::MusicPlayer::initialize()
{
    return _initialized = true;
}

void DummyDriver::MusicPlayer::deinitialize()
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

DENG2_PIMPL_NOREF(DummyDriver::SoundPlayer)
{
    bool initialized = false;
    QList<DummyDriver::Sound *> sounds;

    ~Instance()
    {
        // Should have been deintialized by now.
        DENG2_ASSERT(!initialized);
    }

    void clearSounds()
    {
        qDeleteAll(sounds);
        sounds.clear();
    }
};

DummyDriver::SoundPlayer::SoundPlayer()
    : d(new Instance)
{}

dint DummyDriver::SoundPlayer::initialize()
{
    return d->initialized = true;
}

void DummyDriver::SoundPlayer::deinitialize()
{
    if(!d->initialized) return;

    d->initialized = false;
    d->clearSounds();
}

bool DummyDriver::SoundPlayer::anyRateAccepted() const
{
    // We are not playing any audio so yeah, whatever.
    return d->initialized;
}

void DummyDriver::SoundPlayer::allowRefresh(bool)
{
    // We are not playing any audio so consider it done.
}

void DummyDriver::SoundPlayer::listener(dint, dfloat)
{
    // Not supported.
}

void DummyDriver::SoundPlayer::listenerv(dint, dfloat *)
{
    // Not supported.
}

Sound *DummyDriver::SoundPlayer::makeSound(bool stereoPositioning, dint bytesPer, dint rate)
{
    if(!d->initialized) return nullptr;
    std::unique_ptr<Sound> sound(new DummyDriver::Sound);
    sound->setBuffer(newBuffer(stereoPositioning ? 0 : SFXBF_3D, bytesPer * 8, rate));
    d->sounds << sound.release();
    return d->sounds.last();
}

/**
 * @note Loading must be done prior to setting properties, because the driver might defer
 * creation of the actual data buffer.
 */
DENG2_PIMPL_NOREF(DummyDriver::Sound)
, DENG2_OBSERVES(System, FrameEnds)
{
    dint flags = 0;                  ///< SFXCF_* flags.
    dfloat frequency = 0;            ///< Frequency adjustment: 1.0 is normal.
    dfloat volume = 0;               ///< Sound volume: 1.0 is max.

    mobj_t *emitter = nullptr;       ///< Mobj emitter for the sound, if any (not owned).
    Vector3d origin;                 ///< Emit from here (synced with emitter).

    sfxbuffer_t *buffer = nullptr;   ///< Assigned sound buffer, if any (not owned).
    dint startTime = 0;              ///< When the assigned sound sample was last started.

    ~Instance()
    {
        DENG2_ASSERT(buffer == nullptr);
    }

    void updateOriginIfNeeded()
    {
        // Updating is only necessary if we are tracking an emitter.
        if(!emitter) return;

        origin = Mobj_Origin(*emitter);
        // If this is a mobj, center the Z pos.
        if(Thinker_IsMobjFunc(emitter->thinker.function))
        {
            origin.z += emitter->height / 2;
        }
    }

    /**
     * Flushes property changes to the assigned data buffer.
     *
     * @param force  Usually updates are only necessary during playback. Use
     *               @c true= to override this check and write the properties
     *               regardless.
     */
    void updateBuffer(bool force = false)
    {
        DENG2_ASSERT(buffer);

        // Disabled?
        if(flags & SFXCF_NO_UPDATE) return;

        // Updates are only necessary during playback.
        if(!isPlaying(*buffer) && !force) return;

        // When tracking an emitter we need the latest origin coordinates.
        updateOriginIfNeeded();

        // Frequency is common to both 2D and 3D sounds.
        setFrequency(*buffer, frequency);

        if(buffer->flags & SFXBF_3D)
        {
            // Volume is affected only by maxvol.
            setVolume(*buffer, volume * System::get().soundVolume() / 255.0f);
            if(emitter && emitter == System::get().listener())
            {
                // Emitted by the listener object. Go to relative position mode
                // and set the position to (0,0,0).
                setPositioning(*buffer, true/*head-relative*/);
                setOrigin(*buffer, Vector3d());
            }
            else
            {
                // Use the channel's map space origin.
                setPositioning(*buffer, false/*absolute*/);
                setOrigin(*buffer, origin);
            }

            // If the sound is emitted by the listener, speed is zero.
            if(emitter && emitter != System::get().listener() &&
               Thinker_IsMobjFunc(emitter->thinker.function))
            {
                setVelocity(*buffer, Vector3d(emitter->mom)* TICSPERSEC);
            }
            else
            {
                // Not moving.
                setVelocity(*buffer, Vector3d());
            }
        }
        else
        {
            dfloat dist = 0;
            dfloat finalPan = 0;

            // This is a 2D buffer.
            if((flags & SFXCF_NO_ORIGIN) ||
               (emitter && emitter == System::get().listener()))
            {
                dist = 1;
                finalPan = 0;
            }
            else
            {
                // Calculate roll-off attenuation. [.125/(.125+x), x=0..1]
                Ranged const attenRange = System::get().soundVolumeAttenuationRange();

                dist = System::get().distanceToListener(origin);

                if(dist < attenRange.start || (flags & SFXCF_NO_ATTENUATION))
                {
                    // No distance attenuation.
                    dist = 1;
                }
                else if(dist > attenRange.end)
                {
                    // Can't be heard.
                    dist = 0;
                }
                else
                {
                    ddouble const normdist = (dist - attenRange.start) / attenRange.size();

                    // Apply the linear factor so that at max distance there
                    // really is silence.
                    dist = .125f / (.125f + normdist) * (1 - normdist);
                }

                // And pan, too. Calculate angle from listener to emitter.
                if(mobj_t *listener = System::get().listener())
                {
                    dfloat angle = (M_PointXYToAngle2(listener->origin[0], listener->origin[1],
                                                      origin.x, origin.y)
                                        - listener->angle)
                                 / (dfloat) ANGLE_MAX * 360;

                    // We want a signed angle.
                    if(angle > 180)
                        angle -= 360;

                    // Front half.
                    if(angle <= 90 && angle >= -90)
                    {
                        finalPan = -angle / 90;
                    }
                    else
                    {
                        // Back half.
                        finalPan = (angle + (angle > 0 ? -180 : 180)) / 90;
                        // Dampen sounds coming from behind.
                        dist *= (1 + (finalPan > 0 ? finalPan : -finalPan)) / 2;
                    }
                }
                else
                {
                    // No listener mobj? Can't pan, then.
                    finalPan = 0;
                }
            }

            setVolume(*buffer, volume * dist * System::get().soundVolume() / 255.0f);
            setPan(*buffer, finalPan);
        }
    }

    void systemFrameEnds(System &)
    {
        updateBuffer();
    }

    void stop(sfxbuffer_t &buf)
    {
        // Clear the flag that tells the Sfx module about playing buffers.
        buf.flags &= ~SFXBF_PLAYING;

        // If the sound is started again, it needs to be reloaded.
        buf.flags |= SFXBF_RELOAD;
    }

    void reset(sfxbuffer_t &buf)
    {
        stop(buf);
        buf.sample = nullptr;
        buf.flags &= ~SFXBF_RELOAD;
    }

    void load(sfxbuffer_t &buf, sfxsample_t &sample)
    {
        // Now the buffer is ready for playing.
        buf.sample  = &sample;
        buf.written = sample.size;
        buf.flags  &= ~SFXBF_RELOAD;
    }

    void play(sfxbuffer_t &buf)
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

    bool isPlaying(sfxbuffer_t &buf) const
    {
        return (buf.flags & SFXBF_PLAYING) != 0;
    }

    void refresh(sfxbuffer_t &buf)
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

    void setFrequency(sfxbuffer_t &buf, dfloat newFrequency)
    {
        buf.freq = buf.rate * newFrequency;
    }

    // Unsupported sound buffer property manipulation:

    void setOrigin(sfxbuffer_t &, Vector3d const &) {}
    void setPan(sfxbuffer_t &, dfloat) {}
    void setPositioning(sfxbuffer_t &, bool) {}
    void setVelocity(sfxbuffer_t &, Vector3d const &) {}
    void setVolume(sfxbuffer_t &, dfloat) {}
    void setVolumeAttenuationRange(sfxbuffer_t &, Ranged const &) {}
};

DummyDriver::Sound::Sound() : d(new Instance)
{}

DummyDriver::Sound::~Sound()
{
    releaseBuffer();
}

bool DummyDriver::Sound::hasBuffer() const
{
    return d->buffer != nullptr;
}

sfxbuffer_t const &DummyDriver::Sound::buffer() const
{
    if(d->buffer) return *d->buffer;
    /// @throw MissingBufferError  No sound buffer is currently assigned.
    throw MissingBufferError("audio::DummyDriver::Sound::buffer", "No data buffer is assigned");
}

void DummyDriver::Sound::setBuffer(sfxbuffer_t *newBuffer)
{
    if(d->buffer == newBuffer) return;

    stop();

    if(d->buffer)
    {
        // Cancel frame notifications - we'll soon have no buffer to update.
        System::get().audienceForFrameEnds() -= d;

        delete d->buffer;
        d->buffer = nullptr;
    }

    d->buffer = newBuffer;

    if(d->buffer)
    {
        // We want notification when the frame ends in order to flush deferred property writes.
        System::get().audienceForFrameEnds() += d;
    }
}

void DummyDriver::Sound::format(bool stereoPositioning, dint bytesPer, dint rate)
{
    // Do we need to (re)create the sound data buffer?
    if(   !d->buffer
       || (d->buffer->rate != rate || d->buffer->bytes != bytesPer))
    {
        setBuffer(newBuffer(stereoPositioning ? 0 : SFXBF_3D, bytesPer, rate));
    }
}

dint DummyDriver::Sound::flags() const
{
    return d->flags;
}

/// @todo Use QFlags -ds
void DummyDriver::Sound::setFlags(dint newFlags)
{
    d->flags = newFlags;
}

mobj_t *DummyDriver::Sound::emitter() const
{
    return d->emitter;
}

void DummyDriver::Sound::setEmitter(mobj_t *newEmitter)
{
    d->emitter = newEmitter;
}

void DummyDriver::Sound::setOrigin(Vector3d const &newOrigin)
{
    d->origin = newOrigin;
}

Vector3d DummyDriver::Sound::origin() const
{
    return d->origin;
}

void DummyDriver::Sound::load(sfxsample_t &sample)
{
    if(!d->buffer)
    {
        /// @throw MissingBufferError  Loading is impossible without a buffer...
        throw MissingBufferError("audio::DummyDriver::Sound::load", "Attempted with no data buffer assigned");
    }

    // Don't reload if a sample with the same sound ID is already loaded.
    if(!d->buffer->sample || d->buffer->sample->soundId != sample.soundId)
    {
        d->load(*d->buffer, sample);
    }
}

void DummyDriver::Sound::stop()
{
    if(!d->buffer) return;
    d->stop(*d->buffer);
}

void DummyDriver::Sound::reset()
{
    if(!d->buffer) return;
    d->reset(*d->buffer);
}

void DummyDriver::Sound::play()
{
    if(!d->buffer)
    {
        /// @throw MissingBufferError  Playing is obviously impossible without data to play...
        throw MissingBufferError("audio::DummyDriver::Sound::play", "Attempted with no data buffer assigned");
    }

    // Flush deferred property value changes to the assigned data buffer.
    d->updateBuffer(true/*force*/);

    // 3D sounds need a few extra properties set up.
    if(d->buffer->flags & SFXBF_3D)
    {
        // Configure the attentuation distances.
        // This is only done once, when the sound is first played (i.e., here).
        if(d->flags & SFXCF_NO_ATTENUATION)
        {
            d->setVolumeAttenuationRange(*d->buffer, Ranged(10000, 20000));
        }
        else
        {
            d->setVolumeAttenuationRange(*d->buffer, System::get().soundVolumeAttenuationRange());
        }
    }

    d->play(*d->buffer);
    d->startTime = Timer_Ticks();  // Note the current time.
}

void DummyDriver::Sound::setPlayingMode(dint sfFlags)
{
    if(d->buffer)
    {
        d->buffer->flags &= ~(SFXBF_REPEAT | SFXBF_DONT_STOP);
        if(sfFlags & SF_REPEAT)    d->buffer->flags |= SFXBF_REPEAT;
        if(sfFlags & SF_DONT_STOP) d->buffer->flags |= SFXBF_DONT_STOP;
    }
}

dint DummyDriver::Sound::startTime() const
{
    return d->startTime;
}

audio::Sound &DummyDriver::Sound::setFrequency(dfloat newFrequency)
{
    d->frequency = newFrequency;
    return *this;
}

audio::Sound &DummyDriver::Sound::setVolume(dfloat newVolume)
{
    d->volume = newVolume;
    return *this;
}

bool DummyDriver::Sound::isPlaying() const
{
    return d->buffer && d->isPlaying(*d->buffer);
}

dfloat DummyDriver::Sound::frequency() const
{
    return d->frequency;
}

dfloat DummyDriver::Sound::volume() const
{
    return d->volume;
}

void DummyDriver::Sound::refresh()
{
    d->refresh(*d->buffer);
}

// ----------------------------------------------------------------------------------

DENG2_PIMPL(DummyDriver)
{
    bool initialized = false;

    CdPlayer cd;
    MusicPlayer music;
    SoundPlayer sound;

    Instance(Public *i)
        : Base(i)
        , cd   ()
        , music()
        , sound()
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

dint DummyDriver::playerCount() const
{
    return d->initialized ? 3 : 0;
}

String DummyDriver::playerIdentityKey(IPlayer const &player) const
{
    if(&player == &d->cd)    return "cd";
    if(&player == &d->music) return "music";
    if(&player == &d->sound) return "sfx";
    return "";  // Unknown.
}

LoopResult DummyDriver::forAllPlayers(std::function<LoopResult (IPlayer &)> callback) const
{
    if(d->initialized)
    {
        if(auto result = callback(d->cd))    return result;
        if(auto result = callback(d->music)) return result;
        if(auto result = callback(d->sound)) return result;
    }
    return LoopContinue;  // Continue iteration.
}

}  // namespace audio
