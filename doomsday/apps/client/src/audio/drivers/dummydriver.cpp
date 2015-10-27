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

#include "audio/channel.h"
#include "world/thinkers.h"
#include "def_main.h"        // SF_* flags, remove me
#include <de/Log>
#include <de/Observers>
#include <de/timer.h>        // TICSPERSEC
#include <QList>
#include <QtAlgorithms>

using namespace de;

namespace audio {

DummyDriver::CdChannel::CdChannel() : audio::CdChannel()
{}

Channel &DummyDriver::CdChannel::setVolume(dfloat)
{
    return *this;
}

bool DummyDriver::CdChannel::isPaused() const
{
    return _paused;
}

void DummyDriver::CdChannel::pause()
{
    _paused = true;
}

void DummyDriver::CdChannel::resume()
{
    _paused = false;
}

void DummyDriver::CdChannel::stop()
{}

Channel::PlayingMode DummyDriver::CdChannel::mode() const
{
    return _mode;
}

void DummyDriver::CdChannel::play(PlayingMode mode)
{
    if(isPlaying()) return;
    if(mode == NotPlaying) return;

    if(_track < 0) Error("DummyDriver::CdChannel::play", "No track is bound");
    _mode = mode;
}

void DummyDriver::CdChannel::bindTrack(dint track)
{
    _track = de::max(-1, track);
}

// --------------------------------------------------------------------------------------

DummyDriver::MusicChannel::MusicChannel() : audio::MusicChannel()
{}

Channel &DummyDriver::MusicChannel::setVolume(dfloat)
{
    return *this;
}

bool DummyDriver::MusicChannel::isPaused() const
{
    return _paused;
}

void DummyDriver::MusicChannel::pause()
{
    _paused = true;
}

void DummyDriver::MusicChannel::resume()
{
    _paused = false;
}

void DummyDriver::MusicChannel::stop()
{}

Channel::PlayingMode DummyDriver::MusicChannel::mode() const
{
    return _mode;
}

void DummyDriver::MusicChannel::play(PlayingMode mode)
{
    if(isPlaying()) return;
    if(mode == NotPlaying) return;

    if(_sourcePath.isEmpty()) Error("DummyDriver::MusicChannel::play", "No track is bound");
    _mode = mode;
}

bool DummyDriver::MusicChannel::canPlayBuffer() const
{
    return false;  /// @todo Should support this...
}

void *DummyDriver::MusicChannel::songBuffer(duint)
{
    return nullptr;
}

bool DummyDriver::MusicChannel::canPlayFile() const
{
    return true;
}

void DummyDriver::MusicChannel::bindFile(String const &sourcePath)
{
    _sourcePath = sourcePath;
}

// --------------------------------------------------------------------------------------

/**
 * @note Loading must be done prior to setting properties, because the driver might defer
 * creation of the actual data buffer.
 */
DENG2_PIMPL_NOREF(DummyDriver::SoundChannel)
, DENG2_OBSERVES(System, FrameEnds)
{
    dint flags = 0;                   ///< SFXCF_* flags.
    dfloat frequency = 0;             ///< Frequency adjustment: 1.0 is normal.
    dfloat volume = 0;                ///< Sound volume: 1.0 is max.

    SoundEmitter *emitter = nullptr;  ///< SoundEmitter for the sound, if any (not owned).
    Vector3d origin;                  ///< Emit from here (synced with emitter).
    dint startTime = 0;               ///< When the assigned sound sample was last started.

    sfxbuffer_t buffer;
    bool valid = false;               ///< Set to @c true when in the valid state.

    Instance()
    {
        de::zap(buffer);

        // We want notification when the frame ends in order to flush deferred property writes.
        System::get().audienceForFrameEnds() += this;
    }

    ~Instance()
    {
        // Ensure to stop playback, if we haven't already.
        stop(buffer);

        // Cancel frame notifications.
        System::get().audienceForFrameEnds() -= this;
    }

    void updateOriginIfNeeded()
    {
        // Updating is only necessary if we are tracking an emitter.
        if(!emitter) return;

        origin = Vector3d(emitter->origin);
        // If this is a ap object, center the Z pos.
        if(Thinker_IsMobjFunc(emitter->thinker.function))
        {
            origin.z += ((mobj_t *)emitter)->height / 2;
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
        // Disabled?
        if(flags & SFXCF_NO_UPDATE) return;

        // Updates are only necessary during playback.
        if(!isPlaying(buffer) && !force) return;

        // When tracking an emitter we need the latest origin coordinates.
        updateOriginIfNeeded();

        // Frequency is common to both 2D and 3D sounds.
        setFrequency(buffer, frequency);

        if(buffer.flags & SFXBF_3D)
        {
            // Volume is affected only by maxvol.
            setVolume(buffer, volume * System::get().soundVolume() / 255.0f);
            if(emitter && emitter == (ddmobj_base_t const *)System::get().worldStage().listener().trackedMapObject())
            {
                // Emitted by the listener object. Go to relative position mode
                // and set the position to (0,0,0).
                setPositioning(buffer, true/*head-relative*/);
                setOrigin(buffer, Vector3d());
            }
            else
            {
                // Use the channel's map space origin.
                setPositioning(buffer, false/*absolute*/);
                setOrigin(buffer, origin);
            }

            // If the sound is emitted by the listener, speed is zero.
            if(emitter && emitter != (ddmobj_base_t const *)System::get().worldStage().listener().trackedMapObject()
               && Thinker_IsMobjFunc(emitter->thinker.function))
            {
                setVelocity(buffer, Vector3d(((mobj_t *)emitter)->mom)* TICSPERSEC);
            }
            else
            {
                // Not moving.
                setVelocity(buffer, Vector3d());
            }
        }
        else
        {
            dfloat dist = 0;
            dfloat finalPan = 0;

            // This is a 2D buffer.
            if((flags & SFXCF_NO_ORIGIN) ||
               (emitter && emitter == (ddmobj_base_t const *)System::get().worldStage().listener().trackedMapObject()))
            {
                dist = 1;
                finalPan = 0;
            }
            else
            {
                // Calculate roll-off attenuation. [.125/(.125+x), x=0..1]
                Ranged const attenRange = System::get().worldStage().listener().volumeAttenuationRange();

                dist = System::get().worldStage().listener().distanceFrom(origin);

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
                if(mobj_t const *listener = System::get().worldStage().listener().trackedMapObject())
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

            setVolume(buffer, volume * dist * System::get().soundVolume() / 255.0f);
            setPan(buffer, finalPan);
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

    void load(sfxbuffer_t &buf, sfxsample_t *sample)
    {
        DENG2_ASSERT(sample != nullptr);

        // Now the buffer is ready for playing.
        buf.sample  = sample;
        buf.written = sample->size;
        buf.flags  &= ~SFXBF_RELOAD;
    }

    void play(sfxbuffer_t &buf)
    {
        // Playing is quite impossible without a sample.
        if(!buf.sample) return;

        // Do we need to reload?
        if(buf.flags & SFXBF_RELOAD)
        {
            load(buf, buf.sample);
        }

        // Calculate the end time (milliseconds).
        buf.endTime = Timer_RealMilliseconds() + buf.milliseconds();

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

DummyDriver::SoundChannel::SoundChannel()
    : audio::SoundChannel()
    , d(new Instance)
{
    format(StereoPositioning, 1, 11025);
}

Channel::PlayingMode DummyDriver::SoundChannel::mode() const
{
    if(!d->isPlaying(d->buffer))          return NotPlaying;
    if(d->buffer.flags & SFXBF_REPEAT)    return Looping;
    if(d->buffer.flags & SFXBF_DONT_STOP) return OnceDontDelete;
    return Once;
}

void DummyDriver::SoundChannel::play(PlayingMode mode)
{
    if(isPlaying()) return;

    if(mode == NotPlaying) return;

    d->buffer.flags &= ~(SFXBF_REPEAT | SFXBF_DONT_STOP);
    switch(mode)
    {
    case Looping:        d->buffer.flags |= SFXBF_REPEAT;    break;
    case OnceDontDelete: d->buffer.flags |= SFXBF_DONT_STOP; break;
    default: break;
    }

    // Flush deferred property value changes to the assigned data buffer.
    d->updateBuffer(true/*force*/);

    // 3D sounds need a few extra properties set up.
    if(d->buffer.flags & SFXBF_3D)
    {
        // Configure the attentuation distances.
        // This is only done once, when the sound is first played (i.e., here).
        if(d->flags & SFXCF_NO_ATTENUATION)
        {
            d->setVolumeAttenuationRange(d->buffer, Ranged(10000, 20000));
        }
        else
        {
            d->setVolumeAttenuationRange(d->buffer, System::get().worldStage().listener().volumeAttenuationRange());
        }
    }

    d->play(d->buffer);
    d->startTime = Timer_Ticks();  // Note the current time.
}

void DummyDriver::SoundChannel::stop()
{
    d->stop(d->buffer);
}

bool DummyDriver::SoundChannel::isPaused() const
{
    return false;  // Never...
}

void DummyDriver::SoundChannel::pause()
{
    // Never paused...
}

void DummyDriver::SoundChannel::resume()
{
    // Never paused...
}

SoundEmitter *DummyDriver::SoundChannel::emitter() const
{
    return d->emitter;
}

dfloat DummyDriver::SoundChannel::frequency() const
{
    return d->frequency;
}

Vector3d DummyDriver::SoundChannel::origin() const
{
    return d->origin;
}

Positioning DummyDriver::SoundChannel::positioning() const
{
    return (d->buffer.flags & SFXBF_3D) ? AbsolutePositioning : StereoPositioning;
}

dfloat DummyDriver::SoundChannel::volume() const
{
    return d->volume;
}

audio::SoundChannel &DummyDriver::SoundChannel::setEmitter(SoundEmitter *newEmitter)
{
    d->emitter = newEmitter;
    return *this;
}

audio::SoundChannel &DummyDriver::SoundChannel::setFrequency(dfloat newFrequency)
{
    d->frequency = newFrequency;
    return *this;
}

audio::SoundChannel &DummyDriver::SoundChannel::setOrigin(Vector3d const &newOrigin)
{
    d->origin = newOrigin;
    return *this;
}

Channel &DummyDriver::SoundChannel::setVolume(dfloat newVolume)
{
    d->volume = newVolume;
    return *this;
}

dint DummyDriver::SoundChannel::flags() const
{
    return d->flags;
}

void DummyDriver::SoundChannel::setFlags(dint newFlags)
{
    d->flags = newFlags;
}

void DummyDriver::SoundChannel::update()
{
    d->refresh(d->buffer);
}

void DummyDriver::SoundChannel::reset()
{
    d->reset(d->buffer);
}

bool DummyDriver::SoundChannel::format(Positioning positioning, dint bytesPer, dint rate)
{
    // Do we need to (re)configure the sample data buffer?
    if(this->positioning() != positioning
       || d->buffer.rate   != rate
       || d->buffer.bytes  != bytesPer)
    {
        stop();
        DENG2_ASSERT(!isPlaying());

        de::zap(d->buffer);
        d->buffer.bytes = bytesPer;
        d->buffer.rate  = rate;
        d->buffer.flags = positioning == AbsolutePositioning ? SFXBF_3D : 0;
        d->buffer.freq  = rate;  // Modified by calls to Set(SFXBP_FREQUENCY).
        d->valid = true;
    }
    return isValid();
}

bool DummyDriver::SoundChannel::isValid() const
{
    return d->valid;
}

void DummyDriver::SoundChannel::load(sfxsample_t const &sample)
{
    // Don't reload if a sample with the same sound ID is already loaded.
    if(!d->buffer.sample || d->buffer.sample->soundId != sample.soundId)
    {
        d->load(d->buffer, &const_cast<sfxsample_t &>(sample));
    }
}

dint DummyDriver::SoundChannel::bytes() const
{
    return d->buffer.bytes;
}

dint DummyDriver::SoundChannel::rate() const
{
    return d->buffer.rate;
}

sfxsample_t const *DummyDriver::SoundChannel::samplePtr() const
{
    return d->buffer.sample;
}

dint DummyDriver::SoundChannel::startTime() const
{
    return d->startTime;
}

dint DummyDriver::SoundChannel::endTime() const
{
    return d->buffer.endTime;
}

void DummyDriver::SoundChannel::updateEnvironment()
{
    // Not supported.
}

// --------------------------------------------------------------------------------------

DENG2_PIMPL(DummyDriver)
{
    bool initialized = false;

    struct ChannelSet : public QList<Channel *>
    {
        ~ChannelSet() { DENG2_ASSERT(isEmpty()); }
    } channels[PlaybackInterfaceTypeCount];

    struct CdPlayer : public IPlayer
    {
        bool initialized = false;

        de::dint initialize()
        {
            return initialized = true;
        }

        void deinitialize()
        {
            initialized = false;
        }
    } cd;

    struct MusicPlayer : public IPlayer
    {
        bool initialized = false;

        dint initialize()
        {
            return initialized = true;
        }

        void deinitialize()
        {
            initialized = false;
        }
    } music;

    struct SoundPlayer : public ISoundPlayer
    {
        bool initialized = false;

        ~SoundPlayer() { DENG2_ASSERT(!initialized); }

        bool anyRateAccepted() const
        {
            // We are not playing any audio so yeah, whatever.
            return initialized;
        }

        dint initialize()
        {
            return initialized = true;
        }

        void deinitialize()
        {
            if(!initialized) return;

            initialized = false;
        }

        void allowRefresh(bool allow)
        {
            // We are not playing any audio so consider it done.
        }

        void listener(dint prop, dfloat value)
        {
            // Not supported.
        }
        
        void listenerv(dint prop, dfloat *values)
        {
            // Not supported.
        }
    } sound;

    Instance(Public *i) : Base(i) {}

    ~Instance() { DENG2_ASSERT(!initialized); }

    void clearChannels()
    {
        for(ChannelSet &set : channels)
        {
            qDeleteAll(set);
            set.clear();
        }
    }
};

DummyDriver::DummyDriver() : d(new Instance(this))
{}

DummyDriver::~DummyDriver()
{
    deinitialize();  // If necessary.
}

void DummyDriver::initialize()
{
    LOG_AS("DummyDriver");

    // Already been here?
    if(d->initialized) return;

    d->initialized = true;
}

void DummyDriver::deinitialize()
{
    LOG_AS("DummyDriver");

    // Already been here?
    if(!d->initialized) return;

    d->cd.deinitialize();
    d->music.deinitialize();
    d->sound.deinitialize();
    d->clearChannels();

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

QList<Record> DummyDriver::listInterfaces() const
{
    QList<Record> list;
    {
        Record rec;
        rec.addNumber("type",        AUDIO_ICD);
        rec.addText  ("identityKey", DotPath(identityKey()) / "cd");
        list << rec;  // A copy is made.
    }
    {
        Record rec;
        rec.addNumber("type",        AUDIO_IMUSIC);
        rec.addText  ("identityKey", DotPath(identityKey()) / "music");
        list << rec;
    }
    {
        Record rec;
        rec.addNumber("type",        AUDIO_ISFX);
        rec.addText  ("identityKey", DotPath(identityKey()) / "sfx");
        list << rec;
    }
    return list;
}

IPlayer &DummyDriver::findPlayer(String interfaceIdentityKey) const
{
    if(IPlayer *found = tryFindPlayer(interfaceIdentityKey)) return *found;
    /// @throw UnknownInterfaceError  Unknown interface referenced.
    throw UnknownInterfaceError("DummyDriver::findPlayer", "Unknown playback interface \"" + interfaceIdentityKey + "\"");
}

IPlayer *DummyDriver::tryFindPlayer(String interfaceIdentityKey) const
{
    interfaceIdentityKey = interfaceIdentityKey.toLower();

    if(interfaceIdentityKey == "cd")    return &d->cd;
    if(interfaceIdentityKey == "music") return &d->music;
    if(interfaceIdentityKey == "sfx")   return &d->sound;

    return nullptr;  // Not found.
}

Channel *DummyDriver::makeChannel(PlaybackInterfaceType type)
{
    if(!d->initialized)
        return nullptr;

    switch(type)
    {
    case AUDIO_ICD:
        // Initialize this interface now if we haven't already.
        if(d->cd.initialize())
        {
            std::unique_ptr<Channel> channel(new CdChannel);
            d->channels[type] << channel.get();
            return channel.release();
        }
        break;

    case AUDIO_IMUSIC:
        // Initialize this interface now if we haven't already.
        if(d->music.initialize())
        {
            std::unique_ptr<Channel> channel(new MusicChannel);
            d->channels[type] << channel.get();
            return channel.release();
        }
        break;

    case AUDIO_ISFX:
        // Initialize this interface now if we haven't already.
        if(d->sound.initialize())
        {
            std::unique_ptr<Channel> channel(new SoundChannel);
            d->channels[type] << channel.get();
            return channel.release();
        }
        break;

    default: break;
    }

    return nullptr;
}

LoopResult DummyDriver::forAllChannels(PlaybackInterfaceType type,
    std::function<LoopResult (Channel const &)> callback) const
{
    for(Channel *ch : d->channels[type])
    {
        if(auto result = callback(*ch))
            return result;
    }
    return LoopContinue;
}

}  // namespace audio