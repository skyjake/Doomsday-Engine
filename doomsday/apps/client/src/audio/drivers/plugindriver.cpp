/** @file plugindriver.cpp  Plugin based audio driver.
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
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#include "audio/drivers/plugindriver.h"

#include "api_audiod.h"         // AUDIOP_* flags
#include "audio/samplecache.h"

#include "world/thinkers.h"

#include "def_main.h"    // SF_* flags, remove me
#include "sys_system.h"  // Sys_Sleep()

#include <de/Error>
#include <de/Library>
#include <de/Log>
#include <de/NativeFile>
#include <de/Observers>
#include <de/concurrency.h>
#include <de/timer.h>        // TICSPERSEC
#include <QList>
#include <QtAlgorithms>

using namespace de;

namespace audio {

PluginDriver::CdPlayer::CdPlayer(PluginDriver &driver)
    : _driver(&driver)
{
    de::zap(gen);
    Play = nullptr;
}

PluginDriver &PluginDriver::CdPlayer::driver() const
{
    DENG2_ASSERT(_driver != nullptr);
    return *_driver;
};

dint PluginDriver::CdPlayer::initialize()
{
    if(_needInit)
    {
        _needInit = false;
        DENG2_ASSERT(gen.Init);
        _initialized = gen.Init();
    }
    return _initialized;
}

void PluginDriver::CdPlayer::deinitialize()
{
    if(!_initialized) return;

    _initialized = false;
    if(gen.Shutdown)
    {
        gen.Shutdown();
    }
    _needInit = true;
}

Channel *PluginDriver::CdPlayer::makeChannel()
{
    return new PluginDriver::CdChannel(driver());
}

LoopResult PluginDriver::CdPlayer::forAllChannels(std::function<LoopResult (Channel const &)> callback) const
{
    return LoopContinue;
}

// ----------------------------------------------------------------------------------

PluginDriver::MusicPlayer::MusicPlayer(PluginDriver &driver)
 : _driver(&driver)
{
    de::zap(gen);
    SongBuffer = nullptr;
    Play       = nullptr;
    PlayFile   = nullptr;
}

PluginDriver &PluginDriver::MusicPlayer::driver() const
{
    DENG2_ASSERT(_driver != nullptr);
    return *_driver;
};

dint PluginDriver::MusicPlayer::initialize()
{
    if(_needInit)
    {
        _needInit = false;
        DENG2_ASSERT(gen.Init);
        _initialized = gen.Init();
    }
    return _initialized;
}

void PluginDriver::MusicPlayer::deinitialize()
{
    if(!_initialized) return;

    _initialized = false;
    if(gen.Shutdown)
    {
        gen.Shutdown();
    }
    _needInit = true;
}

Channel *PluginDriver::MusicPlayer::makeChannel()
{
    return new PluginDriver::MusicChannel(driver());
}

LoopResult PluginDriver::MusicPlayer::forAllChannels(std::function<LoopResult (Channel const &)> callback) const
{
    return LoopContinue;
}

// ----------------------------------------------------------------------------------

DENG2_PIMPL_NOREF(PluginDriver::SoundPlayer)
, DENG2_OBSERVES(SampleCache, SampleRemove)
{
    PluginDriver *driver = nullptr;
    bool needInit        = true;
    bool initialized     = false;
    QList<PluginDriver::SoundChannel *> channels;

    thread_t refreshThread      = nullptr;
    volatile bool refreshPaused = false;
    volatile bool refreshing    = false;

    ~Instance()
    {
        // Should have been deinitialized by now.
        DENG2_ASSERT(!initialized);
    }

    /**
     * This is a high-priority thread that periodically checks if the channels need
     * to be updated with more data. The thread terminates when it notices that the
     * sound player is deinitialized.
     *
     * Each sound uses a 250ms buffer, which means the refresh must be done often
     * enough to keep them filled.
     *
     * @todo Use a real mutex, will you?
     */
    static dint C_DECL RefreshThread(void *instance)
    {
        auto &inst = *static_cast<Instance *>(instance);

        // We'll continue looping until the player is deinitialized.
        while(inst.initialized)
        {
            // The bit is swapped on each refresh (debug info).
            //::refMonitor ^= 1;

            if(!inst.refreshPaused)
            {
                // Do the refresh.
                inst.refreshing = true;
                for(PluginDriver::SoundChannel *channel : inst.channels)
                {
                    if(channel->isPlaying())
                    {
                        channel->update();
                    }
                }
                inst.refreshing = false;

                // Let's take a nap.
                Sys_Sleep(200);
            }
            else
            {
                // Refreshing is not allowed, so take a shorter nap while
                // waiting for allowRefresh.
                Sys_Sleep(150);
            }
        }

        // Time to end this thread.
        return 0;
    }

    void pauseRefresh()
    {
        if(refreshPaused) return;  // No change.

        refreshPaused = true;
        // Make sure that if currently running, we don't continue until it has stopped.
        while(refreshing)
        {
            Sys_Sleep(0);
        }
        // Sys_SuspendThread(refreshThread, true);
    }

    void resumeRefresh()
    {
        if(!refreshPaused) return;  // No change.
        refreshPaused = false;
        // Sys_SuspendThread(refreshThread, false);
    }

    void clearChannels()
    {
        qDeleteAll(channels);
        channels.clear();
    }

    /**
     * The given @a sample will soon no longer exist. All channels currently loaded
     * with it must be reset.
     */
    void sampleCacheAboutToRemove(Sample const &sample)
    {
        pauseRefresh();
        for(PluginDriver::SoundChannel *channel : channels)
        {
            if(!channel->isValid()) continue;

            if(channel->samplePtr() && channel->samplePtr()->soundId == sample.soundId)
            {
                // Stop and unload.
                channel->reset();
            }
        }
        resumeRefresh();
    }
};

PluginDriver::SoundPlayer::SoundPlayer(PluginDriver &driver)
    : d(new Instance)
{
    d->driver = &driver;
    de::zap(gen);
}

PluginDriver &PluginDriver::SoundPlayer::driver() const
{
    DENG2_ASSERT(d->driver != nullptr);
    return *d->driver;
};

dint PluginDriver::SoundPlayer::initialize()
{
    if(d->needInit)
    {
        d->needInit = false;
        DENG2_ASSERT(gen.Init);
        d->initialized = gen.Init();

        if(d->initialized)
        {
            // This is based on the scientific calculations that if the DOOM marine
            // is 56 units tall, 60 is about two meters.
            //// @todo Derive from the viewheight.
            gen.Listener(SFXLP_UNITS_PER_METER, 30);
            gen.Listener(SFXLP_DOPPLER, 1.5f);

            dfloat rev[4] = { 0, 0, 0, 0 };
            gen.Listenerv(SFXLP_REVERB, rev);
            gen.Listener(SFXLP_UPDATE, 0);

            audio::System::get().sampleCache().audienceForSampleRemove() += d;
        }
    }
    return d->initialized;
}

void PluginDriver::SoundPlayer::deinitialize()
{
    if(!d->initialized) return;

    // Cancel sample cache removal notification - we intend to clear sounds.
    audio::System::get().sampleCache().audienceForSampleRemove() -= d;

    // Stop any channels still playing (note: does not affect refresh).
    for(PluginDriver::SoundChannel *channel : d->channels)
    {
        channel->stop();
    }

    d->initialized = false;  // Signal the refresh thread to stop.
    d->pauseRefresh();       // Stop further refreshing if in progress.

    if(d->refreshThread)
    {
        // Wait for the refresh thread to stop.
        Sys_WaitThread(d->refreshThread, 2000, nullptr);
        d->refreshThread = nullptr;
    }

    /*if(gen.Shutdown)
    {
        gen.Shutdown();
    }*/

    d->clearChannels();
    d->needInit = true;
}

bool PluginDriver::SoundPlayer::anyRateAccepted() const
{
    dint anyRateAccepted = 0;
    if(gen.Getv)
    {
        gen.Getv(SFXIP_ANY_SAMPLE_RATE_ACCEPTED, &anyRateAccepted);
    }
    return CPP_BOOL( anyRateAccepted );
}

bool PluginDriver::SoundPlayer::needsRefresh() const
{
    if(!d->initialized) return false;
    dint disableRefresh = false;
    if(gen.Getv)
    {
        gen.Getv(SFXIP_DISABLE_CHANNEL_REFRESH, &disableRefresh);
    }
    return !disableRefresh;
}

void PluginDriver::SoundPlayer::allowRefresh(bool allow)
{
    if(!d->initialized) return;
    if(!needsRefresh()) return;

    if(allow)
    {
        d->resumeRefresh();
    }
    else
    {
        d->pauseRefresh();
    }
}

void PluginDriver::SoundPlayer::listener(dint prop, dfloat value)
{
    if(!d->initialized) return;
    DENG2_ASSERT(gen.Listener);
    gen.Listener(prop, value);
}

void PluginDriver::SoundPlayer::listenerv(dint prop, dfloat *values)
{
    if(!d->initialized) return;
    DENG2_ASSERT(gen.Listenerv);
    gen.Listenerv(prop, values);
}

Channel *PluginDriver::SoundPlayer::makeChannel()
{
    if(!d->initialized) return nullptr;
    std::unique_ptr<PluginDriver::SoundChannel> channel(new PluginDriver::SoundChannel(driver()));
    d->channels << channel.release();
    if(d->channels.count() == 1)
    {
        // Start the channel refresh thread. It will stop on its own when it notices that
        // the player is deinitialized.
        d->refreshing    = false;
        d->refreshPaused = false;

        // Start the refresh thread.
        d->refreshThread = Sys_StartThread(Instance::RefreshThread, d.get());
        if(!d->refreshThread)
        {
            throw Error("PluginDriver::SoundPlayer::makeSoundChannel", "Failed starting the refresh thread");
        }
    }
    return d->channels.last();
}

LoopResult PluginDriver::SoundPlayer::forAllChannels(std::function<LoopResult (Channel const &)> callback) const
{
    for(Channel const *ch : d->channels)
    {
        if(auto result = callback(*ch))
            return result;
    }
    return LoopContinue;
}

// --------------------------------------------------------------------------------------

PluginDriver::CdChannel::CdChannel(PluginDriver &driver)
    : audio::CdChannel()
    , _driver(&driver)
{}

Channel &PluginDriver::CdChannel::setVolume(dfloat newVolume)
{
    DENG2_ASSERT(_driver->iCd().gen.Set);
    _driver->iCd().gen.Set(MUSIP_VOLUME, newVolume);
    return *this;
}

bool PluginDriver::CdChannel::isPaused() const
{
    if(isPlaying())
    {
        dint result = 0;
        DENG2_ASSERT(_driver->iCd().gen.Get);
        if(_driver->iCd().gen.Get(MUSIP_PAUSED, &result))
            return CPP_BOOL( result );
    }
    return false;
}

void PluginDriver::CdChannel::pause()
{
    if(!isPlaying()) return;
    DENG2_ASSERT(_driver->iCd().gen.Pause);
    _driver->iCd().gen.Pause(true);
}

void PluginDriver::CdChannel::resume()
{
    if(!isPlaying()) return;
    DENG2_ASSERT(_driver->iCd().gen.Pause);
    _driver->iCd().gen.Pause(false);
}

void PluginDriver::CdChannel::stop()
{
    DENG2_ASSERT(_driver->iCd().gen.Stop);
    _driver->iCd().gen.Stop();
}

Channel::PlayingMode PluginDriver::CdChannel::mode() const
{
    DENG2_ASSERT(_driver->iCd().gen.Get);
    if(!_driver->iCd().gen.Get(MUSIP_PLAYING, nullptr/*unused*/))
        return NotPlaying;
    return _mode;
}

void PluginDriver::CdChannel::play(PlayingMode mode)
{
    if(isPlaying()) return;
    if(mode == NotPlaying) return;

    if(_track >= 0)
    {
        DENG2_ASSERT(_driver->iCd().Play);
        if(_driver->iCd().Play(_track, dint(mode == Looping)))
        {
            _mode = mode;
            return;
        }
        throw Error("PluginDriver::CdChannel::play", "Failed playing track #" + String::number(_track));
    }
    throw Error("PluginDriver::CdChannel::play", "No track bound");
}

void PluginDriver::CdChannel::bindTrack(dint track)
{
    _track = track;
}

// --------------------------------------------------------------------------------------

PluginDriver::MusicChannel::MusicChannel(PluginDriver &driver)
    : audio::MusicChannel()
    , _driver(&driver)
{}

Channel &PluginDriver::MusicChannel::setVolume(dfloat newVolume)
{
    DENG2_ASSERT(_driver->iMusic().gen.Set);
    _driver->iMusic().gen.Set(MUSIP_VOLUME, newVolume);
    return *this;
}

bool PluginDriver::MusicChannel::isPaused() const
{
    if(isPlaying())
    {
        dint result = 0;
        DENG2_ASSERT(_driver->iMusic().gen.Get);
        if(_driver->iMusic().gen.Get(MUSIP_PAUSED, &result))
            return CPP_BOOL( result );
    }
    return false;
}

void PluginDriver::MusicChannel::pause()
{
    if(!isPlaying()) return;
    DENG2_ASSERT(_driver->iMusic().gen.Pause);
    _driver->iMusic().gen.Pause(true);
}

void PluginDriver::MusicChannel::resume()
{
    if(!isPlaying()) return;
    DENG2_ASSERT(_driver->iMusic().gen.Pause);
    _driver->iMusic().gen.Pause(false);
}

void PluginDriver::MusicChannel::stop()
{
    if(!isPlaying()) return;
    DENG2_ASSERT(_driver->iMusic().gen.Stop);
    _driver->iMusic().gen.Stop();
}

Channel::PlayingMode PluginDriver::MusicChannel::mode() const
{
    DENG2_ASSERT(_driver->iMusic().gen.Get);
    if(!_driver->iMusic().gen.Get(MUSIP_PLAYING, nullptr/*unused*/))
        return NotPlaying;

    return _mode;
}

void PluginDriver::MusicChannel::play(PlayingMode mode)
{
    if(isPlaying()) return;
    if(mode == NotPlaying) return;

    if(!_sourcePath.isEmpty())
    {
        DENG2_ASSERT(_driver->iMusic().PlayFile);
        if(_driver->iMusic().PlayFile(_sourcePath.toUtf8().constData(), dint( mode == Looping )))
        {
            _mode = mode;
            return;
        }
        throw Error("PluginDriver::MusicChannel::play", "Failed playing source \"" + _sourcePath + "\"");
    }
    else
    {
        DENG2_ASSERT(_driver->iMusic().Play);
        if(_driver->iMusic().Play(dint( mode == Looping )))
        {
            _mode = mode;
            return;
        }
        throw Error("PluginDriver::MusicChannel::play", "Failed playing buffered data");
    }
}

bool PluginDriver::MusicChannel::canPlayBuffer() const
{
    return _driver->iMusic().Play != nullptr && _driver->iMusic().SongBuffer != nullptr;
}

void *PluginDriver::MusicChannel::songBuffer(duint length)
{
    stop();
    _sourcePath.clear();

    if(!_driver->iMusic().SongBuffer) return nullptr;
    return _driver->iMusic().SongBuffer(length);
}

bool PluginDriver::MusicChannel::canPlayFile() const
{
    return _driver->iMusic().PlayFile != nullptr;
}

void PluginDriver::MusicChannel::bindFile(String const &path)
{
    stop();
    _sourcePath = path;
}

// --------------------------------------------------------------------------------------

/**
 * @note Loading must be done prior to setting properties, because the driver might defer
 * creation of the actual data buffer.
 */
DENG2_PIMPL_NOREF(PluginDriver::SoundChannel)
, DENG2_OBSERVES(Listener, EnvironmentChange)
, DENG2_OBSERVES(Listener, Deletion)
, DENG2_OBSERVES(System,   FrameEnds)
{
    /// No data buffer is assigned. @ingroup errors
    DENG2_ERROR(MissingBufferError);

    PluginDriver &driver;             ///< Owning driver (not owned).

    dint flags = 0;                   ///< SFXCF_* flags.

    dfloat frequency = 0;             ///< Frequency adjustment: 1.0 is normal.
    dfloat volume = 0;                ///< Sound volume: 1.0 is max.
    SoundEmitter *emitter = nullptr;  ///< SoundEmitter for the sound, if any (not owned).
    Vector3d origin;                  ///< Emit from here (synced with emitter).

    Positioning positioning = StereoPositioning;
    dint bytes = 0;
    dint rate = 0;

    sfxbuffer_t *buffer = nullptr;    ///< Assigned sound buffer, if any (not owned).
    dint startTime = 0;               ///< When the assigned sound sample was last started.

    Listener *listener = nullptr;
    // Only necessary when using 3D positioning.
    /// @todo optimize: stop observing when this changes. -ds
    bool needEnvironmentUpdate = false;

    Instance(PluginDriver &owner) : driver(owner)
    {}

    ~Instance()
    {
        // Stop observing the configured listener (if we haven't already).
        observeListener(nullptr);

        setBuffer(nullptr);
        DENG2_ASSERT(buffer == nullptr);
    }

    sfxbuffer_t &getBuffer() const
    {
        if(buffer) return *buffer;
        /// @throw MissingBufferError  No sound buffer is currently assigned.
        throw MissingBufferError("audio::PluginDriver::SoundChannel::Instance", "No sample buffer is assigned");
    }

    /**
     * Replace the sample data buffer with @a newBuffer.
     */
    void setBuffer(sfxbuffer_t *newBuffer)
    {
        if(buffer == newBuffer) return;

        stop();

        if(buffer)
        {
            // Cancel frame notifications - we'll soon have no buffer to update.
            System::get().audienceForFrameEnds() -= this;

            driver.iSound().gen.Destroy(buffer);
        }

        buffer = newBuffer;

        if(buffer)
        {
            // We want notification when the frame ends in order to flush deferred property writes.
            System::get().audienceForFrameEnds() += this;
        }
    }

    void updateOriginIfNeeded()
    {
        // Updating is only necessary if we are tracking an emitter.
        if(!emitter) return;

        origin = Vector3d(emitter->origin);
        // If this is a map object, center the Z pos.
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
        if(!buffer) return;

        // Disabled?
        if(flags & SFXCF_NO_UPDATE) return;

        // Updates are only necessary during playback.
        if(!isPlaying(*buffer) && !force) return;

        // When tracking an emitter we need the latest origin coordinates.
        updateOriginIfNeeded();

        // Frequency is common to both 2D and 3D sounds.
        driver.iSound().gen.Set(buffer, SFXBP_FREQUENCY, frequency);

        if(buffer->flags & SFXBF_3D)
        {
            // Volume is affected only by maxvol.
            driver.iSound().gen.Set(buffer, SFXBP_VOLUME, volume * System::get().soundVolume() / 255.0f);

            if(emitter && emitter == (ddmobj_base_t const *)System::get().worldStage().listener().trackedMapObject())
            {
                // Emitted by the listener object. Go to relative position mode
                // and set the position to (0,0,0).
                float vec[] = { 0, 0, 0 };
                driver.iSound().gen.Set (buffer, SFXBP_RELATIVE_MODE, 1/*headRelative*/);
                driver.iSound().gen.Setv(buffer, SFXBP_POSITION, vec);
            }
            else
            {
                // Use the channel's map space origin.
                driver.iSound().gen.Set (buffer, SFXBP_RELATIVE_MODE, 0/*absolute*/);
                dfloat vec[3]; origin.toVector3f().decompose(vec);
                driver.iSound().gen.Setv(buffer, SFXBP_POSITION, vec);
            }

            // If the sound is emitted by the listener, speed is zero.
            if(emitter && emitter != (ddmobj_base_t const *)System::get().worldStage().listener().trackedMapObject() &&
               Thinker_IsMobjFunc(emitter->thinker.function))
            {
                dfloat vec[3]; (Vector3d(((mobj_t *)emitter)->mom) * TICSPERSEC).toVector3f().decompose(vec);
                driver.iSound().gen.Setv(buffer, SFXBP_VELOCITY, vec);
            }
            else
            {
                // Not moving.
                dfloat vec[] = { 0, 0, 0 };
                driver.iSound().gen.Setv(buffer, SFXBP_VELOCITY, vec);
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
                if(mobj_t const *tracking = System::get().worldStage().listener().trackedMapObject())
                {
                    dfloat angle = (M_PointXYToAngle2(tracking->origin[0], tracking->origin[1],
                                                      origin.x, origin.y)
                                        - tracking->angle)
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

            driver.iSound().gen.Set(buffer, SFXBP_VOLUME, volume * dist * System::get().soundVolume() / 255.0f);
            driver.iSound().gen.Set(buffer, SFXBP_PAN, finalPan);
        }
    }

    void systemFrameEnds(System &)
    {
        updateBuffer();
    }

    bool isPlaying(sfxbuffer_t &buf) const
    {
        return (buf.flags & SFXBF_PLAYING) != 0;
    }

    void stop()
    {
        if(!buffer) return;

        DENG2_ASSERT(driver.iSound().gen.Stop);
        driver.iSound().gen.Stop(buffer);
    }

    void observeListener(Listener *newListener)
    {
        // No change?
        if(listener == newListener) return;

        if(listener)
        {
           listener->audienceForEnvironmentChange() -= this;
           listener->audienceForDeletion()          -= this;
        }

        listener = newListener;
        needEnvironmentUpdate = true;

        if(listener)
        {
            listener->audienceForDeletion()          += this;
            listener->audienceForEnvironmentChange() += this;
        }
    }

    void listenerEnvironmentChanged(Listener &changed)
    {
        DENG2_ASSERT(&changed == listener);
        DENG2_UNUSED(changed);
        // Defer until the end of the frame.
        needEnvironmentUpdate = true;
    }

    void listenerBeingDeleted(Listener const &deleting)
    {
        DENG2_ASSERT(&deleting == listener);
        DENG2_UNUSED(deleting);
        // Defer until the end of the frame.
        needEnvironmentUpdate = true;
        listener = nullptr;
    }
};

PluginDriver::SoundChannel::SoundChannel(PluginDriver &owner)
    : audio::SoundChannel()
    , d(new Instance(owner))
{
    format(StereoPositioning, 1, 11025);
}

Channel::PlayingMode PluginDriver::SoundChannel::mode() const
{
    if(!d->buffer || !d->isPlaying(*d->buffer)) return NotPlaying;
    if(d->buffer->flags & SFXBF_REPEAT)         return Looping;
    if(d->buffer->flags & SFXBF_DONT_STOP)      return OnceDontDelete;
    return Once;
}

void PluginDriver::SoundChannel::play(PlayingMode mode)
{
    if(isPlaying()) return;

    if(mode == NotPlaying) return;

    d->getBuffer().flags &= ~(SFXBF_REPEAT | SFXBF_DONT_STOP);
    switch(mode)
    {
    case Looping:        d->getBuffer().flags |= SFXBF_REPEAT;    break;
    case OnceDontDelete: d->getBuffer().flags |= SFXBF_DONT_STOP; break;
    default: break;
    }

    // When playing on a sound stage with a Listener, we may need to update the channel
    // dynamically during playback.
    d->observeListener(&System::get().worldStage().listener());

    // Flush deferred property value changes to the assigned data buffer.
    d->updateBuffer(true/*force*/);

    // 3D sounds need a few extra properties set up.
    if(d->getBuffer().flags & SFXBF_3D)
    {
        DENG2_ASSERT(d->driver.iSound().gen.Set);

        // Configure the attentuation distances.
        // This is only done once, when the sound is first played (i.e., here).
        if(d->flags & SFXCF_NO_ATTENUATION)
        {
            d->driver.iSound().gen.Set(&d->getBuffer(), SFXBP_MIN_DISTANCE, 10000);
            d->driver.iSound().gen.Set(&d->getBuffer(), SFXBP_MAX_DISTANCE, 20000);
        }
        else
        {
            Ranged const &range = System::get().worldStage().listener().volumeAttenuationRange();
            d->driver.iSound().gen.Set(&d->getBuffer(), SFXBP_MIN_DISTANCE, dfloat( range.start ));
            d->driver.iSound().gen.Set(&d->getBuffer(), SFXBP_MAX_DISTANCE, dfloat( range.end ));
        }
    }

    DENG2_ASSERT(d->driver.iSound().gen.Play);
    d->driver.iSound().gen.Play(&d->getBuffer());

    d->startTime = Timer_Ticks();  // Note the current time.
}

void PluginDriver::SoundChannel::stop()
{
    d->stop();
}

bool PluginDriver::SoundChannel::isPaused() const
{
    return false;  // Never...
}

void PluginDriver::SoundChannel::pause()
{
    // Never paused...
}

void PluginDriver::SoundChannel::resume()
{
    // Never paused...
}

SoundEmitter *PluginDriver::SoundChannel::emitter() const
{
    return d->emitter;
}

dfloat PluginDriver::SoundChannel::frequency() const
{
    return d->frequency;
}

Vector3d PluginDriver::SoundChannel::origin() const
{
    return d->origin;
}

Positioning PluginDriver::SoundChannel::positioning() const
{
    return d->positioning;
}

dfloat PluginDriver::SoundChannel::volume() const
{
    return d->volume;
}

audio::SoundChannel &PluginDriver::SoundChannel::setEmitter(SoundEmitter *newEmitter)
{
    d->emitter = newEmitter;
    return *this;
}

audio::SoundChannel &PluginDriver::SoundChannel::setFrequency(dfloat newFrequency)
{
    d->frequency = newFrequency;
    return *this;
}

audio::SoundChannel &PluginDriver::SoundChannel::setOrigin(Vector3d const &newOrigin)
{
    d->origin = newOrigin;
    return *this;
}

Channel &PluginDriver::SoundChannel::setVolume(dfloat newVolume)
{
    d->volume = newVolume;
    return *this;
}

dint PluginDriver::SoundChannel::flags() const
{
    return d->flags;
}

void PluginDriver::SoundChannel::setFlags(dint newFlags)
{
    d->flags = newFlags;
}

void PluginDriver::SoundChannel::update()
{
    DENG2_ASSERT(d->driver.iSound().gen.Refresh);
    d->driver.iSound().gen.Refresh(d->buffer);
}

void PluginDriver::SoundChannel::reset()
{
    if(!d->buffer) return;

    DENG2_ASSERT(d->driver.iSound().gen.Reset);
    d->driver.iSound().gen.Reset(d->buffer);
}

bool PluginDriver::SoundChannel::format(Positioning positioning, dint bytesPer, dint rate)
{
    // Do we need to (re)create the sound data buffer?
    if(!d->buffer
       || d->positioning != positioning
       || d->bytes       != bytesPer
       || d->rate        != rate)
    {
        stop();
        DENG2_ASSERT(!isPlaying());

        /// @todo Don't duplicate state! -ds
        d->positioning = positioning;
        d->bytes       = bytesPer;
        d->rate        = rate;

        d->setBuffer(d->driver.iSound().gen.Create(d->positioning == AbsolutePositioning ? SFXBF_3D : 0,
                                                   d->bytes * 8, d->rate));
    }
    return isValid();
}

bool PluginDriver::SoundChannel::isValid() const
{
    return d->buffer != nullptr;
}

void PluginDriver::SoundChannel::load(sfxsample_t const &sample)
{
    // Don't reload if a sample with the same sound ID is already loaded.
    if(!d->getBuffer().sample || d->getBuffer().sample->soundId != sample.soundId)
    {
        DENG2_ASSERT(d->driver.iSound().gen.Load);
        d->driver.iSound().gen.Load(&d->getBuffer(), &const_cast<sfxsample_t &>(sample));
    }
}

dint PluginDriver::SoundChannel::bytes() const
{
    return d->bytes;
}

dint PluginDriver::SoundChannel::rate() const
{
    return d->rate;
}

dint PluginDriver::SoundChannel::startTime() const
{
    return d->startTime;
}

dint PluginDriver::SoundChannel::endTime() const
{
    return isValid() && d->buffer ? d->buffer->endTime : 0;
}

sfxsample_t const *PluginDriver::SoundChannel::samplePtr() const
{
    return d->buffer ? d->buffer->sample : nullptr;
}

void PluginDriver::SoundChannel::updateEnvironment()
{
    // No volume means no sound.
    if(!System::get().soundVolume()) return;

    LOG_AS("PluginDriver::SoundChannel");
    DENG2_ASSERT(d->driver.iSound().gen.Listener);
    DENG2_ASSERT(d->driver.iSound().gen.Listenerv);

    Listener &listener = System::get().worldStage().listener();
    if(listener.trackedMapObject())
    {
        {
            // Position.
            dfloat vec[4];
            Vector4f(listener.position().toVector3f(), 0).decompose(vec);
            d->driver.iSound().gen.Listenerv(SFXLP_POSITION, vec);
        }
        {
            // Orientation.
            dfloat vec[2];
            listener.orientation().toVector2f().decompose(vec);
            d->driver.iSound().gen.Listenerv(SFXLP_ORIENTATION, vec);
        }
        {
            // Velocity.
            dfloat vec[4];
            Vector4f(listener.velocity().toVector3f() * TICSPERSEC, 0).decompose(vec);
            d->driver.iSound().gen.Listenerv(SFXLP_VELOCITY, vec);
        }
    }

    if(d->needEnvironmentUpdate)
    {
        d->needEnvironmentUpdate = false;

        // Environment properties.
        Environment const environment = listener.environment();
        dfloat vec[NUM_REVERB_DATA];
        vec[SRD_VOLUME ] = environment.volume;
        vec[SRD_SPACE  ] = environment.space;
        vec[SRD_DECAY  ] = environment.decay;
        vec[SRD_DAMPING] = environment.damping;
        d->driver.iSound().gen.Listenerv(SFXLP_REVERB, vec);
    }

    // Update all listener properties.
    d->driver.iSound().gen.Listener(SFXLP_UPDATE, 0);
}

// ----------------------------------------------------------------------------------

DENG2_PIMPL(PluginDriver)
, DENG2_OBSERVES(audio::System, FrameBegins)
, DENG2_OBSERVES(audio::System, FrameEnds)
, DENG2_OBSERVES(audio::System, MidiFontChange)
{
    bool initialized   = false;
    ::Library *library = nullptr;  ///< Library instance (owned).

    /// @todo Extract this into a (base) Plugin class. -ds
    struct IPlugin
    {
        int (*Init) (void);
        void (*Shutdown) (void);
        void (*Event) (int type);
        int (*Get) (int prop, void *ptr);
        int (*Set) (int prop, void const *ptr);

        IPlugin() { de::zapPtr(this); }
    } iBase;

    CdPlayer    cd;
    MusicPlayer music;
    SoundPlayer sound;

    Instance(Public *i)
        : Base(i)
        , cd   (self)
        , music(self)
        , sound(self)
    {}

    ~Instance()
    {
        // Should have been deinitialized by now.
        DENG2_ASSERT(!initialized);

        // Unload the library.
        Library_Delete(library);
    }

    /**
     * Lookup the value of driver property @a prop.
     */
    String getPropertyAsString(dint prop) const
    {
        DENG2_ASSERT(iBase.Get);
        ddstring_t str; Str_InitStd(&str);
        if(iBase.Get(prop, &str))
        {
            auto string = String(Str_Text(&str));
            Str_Free(&str);
            return string;
        }
        /// @throw ReadPropertyError  Driver returned not successful.
        throw ReadPropertyError("audio::PluginDriver::Instance::getPropertyAsString", "Error reading property:" + String::number(prop));
    }

    /**
     * Lookup the value of @a player property @a prop.
     */
    String getPlayerPropertyAsString(IPlayer const &player, dint prop) const
    {
        char buf[256];  /// @todo This could easily overflow...
        if(&player == &cd)
        {
            DENG2_ASSERT(self.iCd().gen.Get);
            if(self.iCd().gen.Get(prop, buf)) return buf;
            return "";
        }
        if(&player == &music)
        {
            DENG2_ASSERT(self.iMusic().gen.Get);
            if(self.iMusic().gen.Get(prop, buf)) return buf;
            return "";
        }
        if(&player == &sound)
        {
            DENG2_ASSERT(self.iSound().gen.Getv);
            if(self.iSound().gen.Getv(prop, buf)) return buf;
            return "";
        }
        throw ReadPropertyError("audio::PluginDriver::Instance::getPlayerPropertyAsString", "Error reading player property:" + String::number(prop));
    }

    void systemFrameBegins(audio::System &)
    {
        DENG2_ASSERT(initialized);
        if(cd.gen.Init)    cd.gen.Update();
        if(music.gen.Init) music.gen.Update();
        //if(sound.gen.Init) sound.gen.Update();
    }

    void systemFrameEnds(audio::System &)
    {
        DENG2_ASSERT(initialized);
        iBase.Event(SFXEV_REFRESH);
    }

    void systemMidiFontChanged(String const &newMidiFontPath)
    {
        DENG2_ASSERT(initialized);
        DENG2_ASSERT(iBase.Set);
        iBase.Set(AUDIOP_SOUNDFONT_FILENAME, newMidiFontPath.toLatin1().constData());
    }
};

PluginDriver::PluginDriver() : d(new Instance(this))
{}

PluginDriver::~PluginDriver()
{
    deinitialize();  // If necessary.
}

PluginDriver *PluginDriver::interpretFile(LibraryFile *libFile)  // static
{
    if(recognize(*libFile))
    {
        try
        {
            std::unique_ptr<PluginDriver> driver(new PluginDriver);
            PluginDriver &inst = *driver;

            inst.d->library = Library_New(libFile->path().toUtf8().constData());
            if(!inst.d->library)
            {
                /// @todo fixme: Should not be failing here! -ds
                return nullptr;
            }

            de::Library &lib = libFile->library();

            lib.setSymbolPtr( inst.d->iBase.Init,     "DS_Init");
            lib.setSymbolPtr( inst.d->iBase.Shutdown, "DS_Shutdown");
            lib.setSymbolPtr( inst.d->iBase.Event,    "DS_Event");
            lib.setSymbolPtr( inst.d->iBase.Get,      "DS_Get");
            lib.setSymbolPtr( inst.d->iBase.Set,      "DS_Set", de::Library::OptionalSymbol);

            if(lib.hasSymbol("DS_SFX_Init"))
            {
                lib.setSymbolPtr( inst.d->sound.gen.Init,      "DS_SFX_Init");
                //lib.setSymbolPtr( inst.d->sound.gen.Shutdown,  "DM_SFX_Shutdown", de::Library::OptionalSymbol);
                lib.setSymbolPtr( inst.d->sound.gen.Create,    "DS_SFX_CreateBuffer");
                lib.setSymbolPtr( inst.d->sound.gen.Destroy,   "DS_SFX_DestroyBuffer");
                lib.setSymbolPtr( inst.d->sound.gen.Load,      "DS_SFX_Load");
                lib.setSymbolPtr( inst.d->sound.gen.Reset,     "DS_SFX_Reset");
                lib.setSymbolPtr( inst.d->sound.gen.Play,      "DS_SFX_Play");
                lib.setSymbolPtr( inst.d->sound.gen.Stop,      "DS_SFX_Stop");
                lib.setSymbolPtr( inst.d->sound.gen.Refresh,   "DS_SFX_Refresh");
                lib.setSymbolPtr( inst.d->sound.gen.Set,       "DS_SFX_Set");
                lib.setSymbolPtr( inst.d->sound.gen.Setv,      "DS_SFX_Setv");
                lib.setSymbolPtr( inst.d->sound.gen.Listener,  "DS_SFX_Listener");
                lib.setSymbolPtr( inst.d->sound.gen.Listenerv, "DS_SFX_Listenerv");
                lib.setSymbolPtr( inst.d->sound.gen.Getv,      "DS_SFX_Getv");
            }

            if(lib.hasSymbol("DM_Music_Init"))
            {
                lib.setSymbolPtr( inst.d->music.gen.Init,      "DM_Music_Init");
                lib.setSymbolPtr( inst.d->music.gen.Shutdown,  "DM_Music_Shutdown", de::Library::OptionalSymbol);
                lib.setSymbolPtr( inst.d->music.gen.Update,    "DM_Music_Update");
                lib.setSymbolPtr( inst.d->music.gen.Get,       "DM_Music_Get");
                lib.setSymbolPtr( inst.d->music.gen.Set,       "DM_Music_Set");
                lib.setSymbolPtr( inst.d->music.gen.Pause,     "DM_Music_Pause");
                lib.setSymbolPtr( inst.d->music.gen.Stop,      "DM_Music_Stop");
                lib.setSymbolPtr( inst.d->music.SongBuffer,    "DM_Music_SongBuffer", de::Library::OptionalSymbol);
                lib.setSymbolPtr( inst.d->music.Play,          "DM_Music_Play",       de::Library::OptionalSymbol);
                lib.setSymbolPtr( inst.d->music.PlayFile,      "DM_Music_PlayFile",   de::Library::OptionalSymbol);
            }

            if(lib.hasSymbol("DM_CDAudio_Init"))
            {
                lib.setSymbolPtr( inst.d->cd.gen.Init,         "DM_CDAudio_Init");
                lib.setSymbolPtr( inst.d->cd.gen.Shutdown,     "DM_CDAudio_Shutdown", de::Library::OptionalSymbol);
                lib.setSymbolPtr( inst.d->cd.gen.Update,       "DM_CDAudio_Update");
                lib.setSymbolPtr( inst.d->cd.gen.Set,          "DM_CDAudio_Set");
                lib.setSymbolPtr( inst.d->cd.gen.Get,          "DM_CDAudio_Get");
                lib.setSymbolPtr( inst.d->cd.gen.Pause,        "DM_CDAudio_Pause");
                lib.setSymbolPtr( inst.d->cd.gen.Stop,         "DM_CDAudio_Stop");
                lib.setSymbolPtr( inst.d->cd.Play,             "DM_CDAudio_Play");
            }

            LOG_RES_VERBOSE("Interpreted ") << NativePath(libFile->path()).pretty()
                                            << " as a plugin audio driver";
            return driver.release();
        }
        catch(de::Library::SymbolMissingError const &er)
        {
            LOG_AS("PluginDriver::interpretFile");
            LOG_AUDIO_ERROR("") << er.asText();
        }
    }
    return nullptr;
}

bool PluginDriver::recognize(LibraryFile &library)  // static
{
    // By convention, driver plugin names use a standard prefix.
    if(!library.name().beginsWith("audio_")) return false;

    // Driver plugins are native files.
    if(!library.source()->is<NativeFile>()) return false;

    // This appears to be usuable with PluginDriver.
    /// @todo Open the library and ensure it's type matches.
    return true;
}

String PluginDriver::identityKey() const
{
    return d->getPropertyAsString(AUDIOP_IDENTITYKEY).toLower();
}

String PluginDriver::title() const
{
    return d->getPropertyAsString(AUDIOP_TITLE);
}

audio::System::IDriver::Status PluginDriver::status() const
{
    if(d->initialized) return Initialized;
    DENG2_ASSERT(d->iBase.Init != nullptr);
    return Loaded;
}

void PluginDriver::initialize()
{
    LOG_AS("PluginDriver");

    // Already been here?
    if(d->initialized) return;

    DENG2_ASSERT(d->iBase.Init != nullptr);
    d->initialized = d->iBase.Init();
    if(!d->initialized) return;

    // We want notification at various times:
    audioSystem().audienceForFrameBegins() += d;
    audioSystem().audienceForFrameEnds()   += d;
    if(d->iBase.Set)
    {
        audioSystem().audienceForMidiFontChange() += d;
    }
}

void PluginDriver::deinitialize()
{
    LOG_AS("PluginDriver");

    // Already been here?
    if(!d->initialized) return;

    if(d->iBase.Shutdown)
    {
        d->iBase.Shutdown();
    }

    // Stop receiving notifications:
    if(d->iBase.Set)
    {
        audioSystem().audienceForMidiFontChange() -= d;
    }
    audioSystem().audienceForFrameEnds()   -= d;
    audioSystem().audienceForFrameBegins() -= d;

    d->initialized = false;
}

::Library *PluginDriver::library() const
{
    return d->library;
}

QList<Record> PluginDriver::listInterfaces() const
{
    QList<Record> list;
    String const driverIdKey = identityKey().split(';').first();

    if(d->cd.gen.Init != nullptr)
    {
        String const idKey = d->getPlayerPropertyAsString(d->cd, MUSIP_IDENTITYKEY);
        if(!idKey.isEmpty())
        {
            Record rec;
            rec.addNumber("type",        AUDIO_ICD);
            rec.addText  ("identityKey", DotPath(driverIdKey) / idKey);
            list << rec;  // A copy is made.
        }
        else DENG2_ASSERT(!"[MUSIP_IDENTITYKEY not defined]");
    }
    if(d->music.gen.Init != nullptr)
    {
        String const idKey = d->getPlayerPropertyAsString(d->music, MUSIP_IDENTITYKEY);
        if(!idKey.isEmpty())
        {
            Record rec;
            rec.addNumber("type",        AUDIO_IMUSIC);
            rec.addText  ("identityKey", DotPath(driverIdKey) / idKey);
            list << rec;
        }
        else DENG2_ASSERT(!"[MUSIP_IDENTITYKEY not defined]");
    }
    if(d->sound.gen.Init != nullptr)
    {
        String const idKey = d->getPlayerPropertyAsString(d->sound, SFXIP_IDENTITYKEY);
        if(!idKey.isEmpty())
        {
            Record rec;
            rec.addNumber("type",        AUDIO_ISFX);
            rec.addText  ("identityKey", DotPath(driverIdKey) / idKey);
            list << rec;
        }
        else DENG2_ASSERT(!"[SFXIP_IDENTITYKEY not defined]");
    }
    return list;
}

IPlayer &PluginDriver::findPlayer(String interfaceIdentityKey) const
{
    if(IPlayer *found = tryFindPlayer(interfaceIdentityKey)) return *found;
    /// @throw UnknownInterfaceError  Unknown interface referenced.
    throw UnknownInterfaceError("PluginDriver::findPlayer", "Unknown playback interface \"" + interfaceIdentityKey + "\"");
}

IPlayer *PluginDriver::tryFindPlayer(String interfaceIdentityKey) const
{
    interfaceIdentityKey = interfaceIdentityKey.toLower();

    if(d->cd.gen.Init != nullptr)
    {
        if(d->getPlayerPropertyAsString(d->cd   , MUSIP_IDENTITYKEY) == interfaceIdentityKey)
            return &d->cd;
    }
    if(d->music.gen.Init != nullptr)
    {
        if(d->getPlayerPropertyAsString(d->music, MUSIP_IDENTITYKEY) == interfaceIdentityKey)
            return &d->music;
    }
    if(d->sound.gen.Init != nullptr)
    {
        if(d->getPlayerPropertyAsString(d->sound, SFXIP_IDENTITYKEY) == interfaceIdentityKey)
            return &d->sound;
    }

    return nullptr;  // Not found.
}

LoopResult PluginDriver::forAllChannels(PlaybackInterfaceType type,
    std::function<LoopResult (Channel const &)> callback) const
{
    switch(type)
    {
    case AUDIO_ICD:
        return d->cd.forAllChannels([&callback] (Channel const &ch)
        {
            return callback(ch);
        });

    case AUDIO_IMUSIC:
        return d->music.forAllChannels([&callback] (Channel const &ch)
        {
            return callback(ch);
        });

    case AUDIO_ISFX:
        return d->sound.forAllChannels([&callback] (Channel const &ch)
        {
            return callback(ch);
        });

    default: break;
    }
    return LoopContinue;
}

PluginDriver::CdPlayer &PluginDriver::iCd() const
{
    return d->cd;
}

PluginDriver::MusicPlayer &PluginDriver::iMusic() const
{
    return d->music;
}

PluginDriver::SoundPlayer &PluginDriver::iSound() const
{
    return d->sound;
}

}  // namespace audio
