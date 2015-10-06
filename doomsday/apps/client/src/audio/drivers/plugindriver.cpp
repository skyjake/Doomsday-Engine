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
#include "def_main.h"           // SF_* flags, remove me
#include "sys_system.h"         // Sys_Sleep()
#include <de/Library>
#include <de/Log>
#include <de/Observers>
#include <de/NativeFile>
#include <de/concurrency.h>
#include <de/timer.h>           // TICSPERSEC
#include <QList>
#include <QtAlgorithms>

using namespace de;

namespace audio {

PluginDriver::CdPlayer::CdPlayer(PluginDriver &driver)
    : _driver(&driver)
{}

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
        DENG2_ASSERT(driver().as<PluginDriver>().iCd().gen.Init);
        _initialized = driver().as<PluginDriver>().iCd().gen.Init();
    }
    return _initialized;
}

void PluginDriver::CdPlayer::deinitialize()
{
    if(!_initialized) return;

    _initialized = false;
    if(driver().as<PluginDriver>().iCd().gen.Shutdown)
    {
        driver().as<PluginDriver>().iCd().gen.Shutdown();
    }
    _needInit = true;
}

void PluginDriver::CdPlayer::update()
{
    if(!_initialized) return;
    DENG2_ASSERT(driver().as<PluginDriver>().iCd().gen.Update);
    driver().as<PluginDriver>().iCd().gen.Update();
}

void PluginDriver::CdPlayer::setVolume(dfloat newVolume)
{
    if(!_initialized) return;
    DENG2_ASSERT(driver().as<PluginDriver>().iCd().gen.Set);
    driver().as<PluginDriver>().iCd().gen.Set(MUSIP_VOLUME, newVolume);
}

bool PluginDriver::CdPlayer::isPlaying() const
{
    if(!_initialized) return false;
    DENG2_ASSERT(driver().as<PluginDriver>().iCd().gen.Get);
    return driver().as<PluginDriver>().iCd().gen.Get(MUSIP_PLAYING, nullptr);
}

void PluginDriver::CdPlayer::pause(dint pause)
{
    if(!_initialized) return;
    DENG2_ASSERT(driver().as<PluginDriver>().iCd().gen.Pause);
    driver().as<PluginDriver>().iCd().gen.Pause(pause);
}

void PluginDriver::CdPlayer::stop()
{
    if(!_initialized) return;
    DENG2_ASSERT(driver().as<PluginDriver>().iCd().gen.Stop);
    driver().as<PluginDriver>().iCd().gen.Stop();
}

dint PluginDriver::CdPlayer::play(dint track, dint looped)
{
    if(!_initialized) return false;
    DENG2_ASSERT(driver().as<PluginDriver>().iCd().Play);
    return driver().as<PluginDriver>().iCd().Play(track, looped);
}

// ----------------------------------------------------------------------------------

PluginDriver::MusicPlayer::MusicPlayer(PluginDriver &driver)
 : _driver(&driver)
{}

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
        DENG2_ASSERT(driver().as<PluginDriver>().iMusic().gen.Init);
        _initialized = driver().as<PluginDriver>().iMusic().gen.Init();
    }
    return _initialized;
}

void PluginDriver::MusicPlayer::deinitialize()
{
    if(!_initialized) return;

    _initialized = false;
    if(driver().as<PluginDriver>().iMusic().gen.Shutdown)
    {
        driver().as<PluginDriver>().iMusic().gen.Shutdown();
    }
    _needInit = true;
}

void PluginDriver::MusicPlayer::update()
{
    if(!_initialized) return;
    DENG2_ASSERT(driver().as<PluginDriver>().iMusic().gen.Update);
    driver().as<PluginDriver>().iMusic().gen.Update();
}

void PluginDriver::MusicPlayer::setVolume(dfloat newVolume)
{
    if(!_initialized) return;
    DENG2_ASSERT(driver().as<PluginDriver>().iMusic().gen.Set);
    driver().as<PluginDriver>().iMusic().gen.Set(MUSIP_VOLUME, newVolume);
}

bool PluginDriver::MusicPlayer::isPlaying() const
{
    if(!_initialized) return false;
    DENG2_ASSERT(driver().as<PluginDriver>().iMusic().gen.Get);
    return driver().as<PluginDriver>().iMusic().gen.Get(MUSIP_PLAYING, nullptr);
}

void PluginDriver::MusicPlayer::pause(dint pause)
{
    if(!_initialized) return;
    DENG2_ASSERT(driver().as<PluginDriver>().iMusic().gen.Pause);
    driver().as<PluginDriver>().iMusic().gen.Pause(pause);
}

void PluginDriver::MusicPlayer::stop()
{
    if(!_initialized) return;
    DENG2_ASSERT(driver().as<PluginDriver>().iMusic().gen.Stop);
    driver().as<PluginDriver>().iMusic().gen.Stop();
}

bool PluginDriver::MusicPlayer::canPlayBuffer() const
{
    if(!_initialized) return false;
    return driver().as<PluginDriver>().iMusic().Play != nullptr && driver().as<PluginDriver>().iMusic().SongBuffer != nullptr;
}

void *PluginDriver::MusicPlayer::songBuffer(duint length)
{
    if(!_initialized) return nullptr;
    if(!driver().as<PluginDriver>().iMusic().SongBuffer) return nullptr;
    return driver().as<PluginDriver>().iMusic().SongBuffer(length);
}

dint PluginDriver::MusicPlayer::play(dint looped)
{
    if(!_initialized) return false;
    if(!driver().as<PluginDriver>().iMusic().Play) return false;
    return driver().as<PluginDriver>().iMusic().Play(looped);
}

bool PluginDriver::MusicPlayer::canPlayFile() const
{
    if(!_initialized) return false;
    return driver().as<PluginDriver>().iMusic().PlayFile != nullptr;
}

dint PluginDriver::MusicPlayer::playFile(char const *filename, dint looped)
{
    if(!_initialized) return false;
    if(!driver().as<PluginDriver>().iMusic().PlayFile) return false;
    return driver().as<PluginDriver>().iMusic().PlayFile(filename, looped);
}

// ----------------------------------------------------------------------------------

DENG2_PIMPL_NOREF(PluginDriver::SoundPlayer)
, DENG2_OBSERVES(SampleCache, SampleRemove)
{
    PluginDriver *driver = nullptr;
    bool needInit        = true;
    bool initialized     = false;
    QList<PluginDriver::Sound *> sounds;

    thread_t refreshThread      = nullptr;
    volatile bool refreshPaused = false;
    volatile bool refreshing    = false;

    ~Instance()
    {
        // Should have been deinitialized by now.
        DENG2_ASSERT(!initialized);
    }

    /**
     * This is a high-priority thread that periodically checks if the sounds need
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
                for(PluginDriver::Sound *sound : inst.sounds)
                {
                    if(sound->isPlaying())
                    {
                        sound->refresh();
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

    void clearSounds()
    {
        qDeleteAll(sounds);
        sounds.clear();
    }

    /**
     * The given @a sample will soon no longer exist. All channels currently loaded
     * with it must be reset.
     */
    void sampleCacheAboutToRemove(Sample const &sample)
    {
        pauseRefresh();
        for(PluginDriver::Sound *sound : sounds)
        {
            if(!sound->hasBuffer()) continue;

            sfxbuffer_t const &sbuf = sound->buffer();
            if(sbuf.sample && sbuf.sample->soundId == sample.soundId)
            {
                // Stop and unload.
                sound->reset();
            }
        }
        resumeRefresh();
    }
};

PluginDriver::SoundPlayer::SoundPlayer(PluginDriver &driver)
    : d(new Instance)
{
    d->driver = &driver;
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
        DENG2_ASSERT(driver().as<PluginDriver>().iSound().gen.Init);
        d->initialized = driver().as<PluginDriver>().iSound().gen.Init();

        if(d->initialized)
        {
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

    // Stop any sounds still playing (note: does not affect refresh).
    for(PluginDriver::Sound *sound : d->sounds)
    {
        sound->stop();
    }

    d->initialized = false;  // Signal the refresh thread to stop.
    d->pauseRefresh();       // Stop further refreshing if in progress.

    if(d->refreshThread)
    {
        // Wait for the refresh thread to stop.
        Sys_WaitThread(d->refreshThread, 2000, nullptr);
        d->refreshThread = nullptr;
    }

    /*if(driver().as<PluginDriver>().iSound().gen.Shutdown)
    {
        driver().as<PluginDriver>().iSound().gen.Shutdown();
    }*/

    d->clearSounds();
    d->needInit = true;
}

bool PluginDriver::SoundPlayer::anyRateAccepted() const
{
    dint anyRateAccepted = 0;
    if(driver().as<PluginDriver>().iSound().gen.Getv)
    {
        driver().as<PluginDriver>().iSound().gen.Getv(SFXIP_ANY_SAMPLE_RATE_ACCEPTED, &anyRateAccepted);
    }
    return CPP_BOOL( anyRateAccepted );
}

bool PluginDriver::SoundPlayer::needsRefresh() const
{
    if(!d->initialized) return false;
    dint disableRefresh = false;
    if(driver().as<PluginDriver>().iSound().gen.Getv)
    {
        driver().as<PluginDriver>().iSound().gen.Getv(SFXIP_DISABLE_CHANNEL_REFRESH, &disableRefresh);
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
    DENG2_ASSERT(driver().as<PluginDriver>().iSound().gen.Listener);
    driver().as<PluginDriver>().iSound().gen.Listener(prop, value);
}

void PluginDriver::SoundPlayer::listenerv(dint prop, dfloat *values)
{
    if(!d->initialized) return;
    DENG2_ASSERT(driver().as<PluginDriver>().iSound().gen.Listenerv);
    driver().as<PluginDriver>().iSound().gen.Listenerv(prop, values);
}

Sound *PluginDriver::SoundPlayer::makeSound(bool stereoPositioning, dint bytesPer, dint rate)
{
    if(!d->initialized) return nullptr;
    std::unique_ptr<Sound> sound(new PluginDriver::Sound(*this));
    sound->setBuffer(driver().as<PluginDriver>().iSound().gen.Create(stereoPositioning ? 0 : SFXBF_3D, bytesPer * 8, rate));
    d->sounds << sound.release();
    if(d->sounds.count() == 1)
    {
        // Start the sound refresh thread. It will stop on its own when it notices that
        // the player is deinitialized.
        d->refreshing    = false;
        d->refreshPaused = false;

        // Start the refresh thread.
        d->refreshThread = Sys_StartThread(Instance::RefreshThread, d.get());
        if(!d->refreshThread)
        {
            throw Error("PluginDriver::SoundPlayer::makeSound", "Failed starting the refresh thread");
        }
    }
    return d->sounds.last();
}

/**
 * @note Loading must be done prior to setting properties, because the driver might defer
 * creation of the actual data buffer.
 */
DENG2_PIMPL_NOREF(PluginDriver::Sound)
, DENG2_OBSERVES(System, FrameEnds)
{
    dint flags = 0;                  ///< SFXCF_* flags.
    dfloat frequency = 0;            ///< Frequency adjustment: 1.0 is normal.
    dfloat volume = 0;               ///< Sound volume: 1.0 is max.

    mobj_t *emitter = nullptr;       ///< Mobj emitter for the sound, if any (not owned).
    Vector3d origin;                 ///< Emit from here (synced with emitter).

    sfxbuffer_t *buffer = nullptr;   ///< Assigned sound buffer, if any (not owned).
    dint startTime = 0;              ///< When the assigned sound sample was last started.

    SoundPlayer *player = nullptr;  ///< Owning player (not owned).

    ~Instance()
    {
        DENG2_ASSERT(buffer == nullptr);
    }

    inline PluginDriver &getDriver()
    {
        DENG2_ASSERT(player != nullptr);
        return player->driver();
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
        auto &driver = getDriver();

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

            if(emitter && emitter == System::get().listener())
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
            if(emitter && emitter != System::get().listener() &&
               Thinker_IsMobjFunc(emitter->thinker.function))
            {
                dfloat vec[3]; (Vector3d(emitter->mom) * TICSPERSEC).toVector3f().decompose(vec);
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
};

PluginDriver::Sound::Sound(PluginDriver::SoundPlayer &player) : d(new Instance)
{
    d->player = &player;
}

PluginDriver::Sound::~Sound()
{
    releaseBuffer();
}

bool PluginDriver::Sound::hasBuffer() const
{
    return d->buffer != nullptr;
}

sfxbuffer_t const &PluginDriver::Sound::buffer() const
{
    if(d->buffer) return *d->buffer;
    /// @throw MissingBufferError  No sound buffer is currently assigned.
    throw MissingBufferError("audio::PluginDriver::Sound::buffer", "No data buffer is assigned");
}

void PluginDriver::Sound::setBuffer(sfxbuffer_t *newBuffer)
{
    if(d->buffer == newBuffer) return;

    stop();

    if(d->buffer)
    {
        // Cancel frame notifications - we'll soon have no buffer to update.
        System::get().audienceForFrameEnds() -= d;

        d->getDriver().iSound().gen.Destroy(d->buffer);
        d->buffer = nullptr;
    }

    d->buffer = newBuffer;

    if(d->buffer)
    {
        // We want notification when the frame ends in order to flush deferred property writes.
        System::get().audienceForFrameEnds() += d;
    }
}

void PluginDriver::Sound::format(bool stereoPositioning, dint bytesPer, dint rate)
{
    // Do we need to (re)create the sound data buffer?
    if(   !d->buffer
       || (d->buffer->rate != rate || d->buffer->bytes != bytesPer))
    {
        setBuffer(d->getDriver().iSound().gen.Create(stereoPositioning ? 0 : SFXBF_3D, bytesPer, rate));
    }
}

dint PluginDriver::Sound::flags() const
{
    return d->flags;
}

/// @todo Use QFlags -ds
void PluginDriver::Sound::setFlags(dint newFlags)
{
    d->flags = newFlags;
}

mobj_t *PluginDriver::Sound::emitter() const
{
    return d->emitter;
}

void PluginDriver::Sound::setEmitter(mobj_t *newEmitter)
{
    d->emitter = newEmitter;
}

void PluginDriver::Sound::setOrigin(Vector3d const &newOrigin)
{
    d->origin = newOrigin;
}

Vector3d PluginDriver::Sound::origin() const
{
    return d->origin;
}

void PluginDriver::Sound::load(sfxsample_t &sample)
{
    if(!d->buffer)
    {
        /// @throw MissingBufferError  Loading is impossible without a buffer...
        throw MissingBufferError("audio::PluginDriver::Sound::load", "Attempted with no data buffer assigned");
    }

    // Don't reload if a sample with the same sound ID is already loaded.
    if(!d->buffer->sample || d->buffer->sample->soundId != sample.soundId)
    {
        DENG2_ASSERT(d->getDriver().iSound().gen.Load);
        d->getDriver().iSound().gen.Load(d->buffer, &sample);
    }
}

void PluginDriver::Sound::stop()
{
    if(!d->buffer) return;

    DENG2_ASSERT(d->getDriver().iSound().gen.Stop);
    d->getDriver().iSound().gen.Stop(d->buffer);
}

void PluginDriver::Sound::reset()
{
    if(!d->buffer) return;

    DENG2_ASSERT(d->getDriver().iSound().gen.Reset);
    d->getDriver().iSound().gen.Reset(d->buffer);
}

void PluginDriver::Sound::play()
{
    if(!d->buffer)
    {
        /// @throw MissingBufferError  Playing is obviously impossible without data to play...
        throw MissingBufferError("audio::PluginDriver::Sound::play", "Attempted with no data buffer assigned");
    }

    // Flush deferred property value changes to the assigned data buffer.
    d->updateBuffer(true/*force*/);

    // 3D sounds need a few extra properties set up.
    if(d->buffer->flags & SFXBF_3D)
    {
        DENG2_ASSERT(d->getDriver().iSound().gen.Set);

        // Configure the attentuation distances.
        // This is only done once, when the sound is first played (i.e., here).
        if(d->flags & SFXCF_NO_ATTENUATION)
        {
            d->getDriver().iSound().gen.Set(d->buffer, SFXBP_MIN_DISTANCE, 10000);
            d->getDriver().iSound().gen.Set(d->buffer, SFXBP_MAX_DISTANCE, 20000);
        }
        else
        {
            Ranged const &range = System::get().soundVolumeAttenuationRange();
            d->getDriver().iSound().gen.Set(d->buffer, SFXBP_MIN_DISTANCE, dfloat( range.start ));
            d->getDriver().iSound().gen.Set(d->buffer, SFXBP_MAX_DISTANCE, dfloat( range.end ));
        }
    }

    DENG2_ASSERT(d->getDriver().iSound().gen.Play);
    d->getDriver().iSound().gen.Play(d->buffer);

    d->startTime = Timer_Ticks();  // Note the current time.
}

void PluginDriver::Sound::setPlayingMode(dint sfFlags)
{
    if(d->buffer)
    {
        d->buffer->flags &= ~(SFXBF_REPEAT | SFXBF_DONT_STOP);
        if(sfFlags & SF_REPEAT)    d->buffer->flags |= SFXBF_REPEAT;
        if(sfFlags & SF_DONT_STOP) d->buffer->flags |= SFXBF_DONT_STOP;
    }
}

dint PluginDriver::Sound::startTime() const
{
    return d->startTime;
}

audio::Sound &PluginDriver::Sound::setFrequency(dfloat newFrequency)
{
    d->frequency = newFrequency;
    return *this;
}

audio::Sound &PluginDriver::Sound::setVolume(dfloat newVolume)
{
    d->volume = newVolume;
    return *this;
}

bool PluginDriver::Sound::isPlaying() const
{
    return d->buffer && d->isPlaying(*d->buffer);
}

dfloat PluginDriver::Sound::frequency() const
{
    return d->frequency;
}

dfloat PluginDriver::Sound::volume() const
{
    return d->volume;
}

void PluginDriver::Sound::refresh()
{
    DENG2_ASSERT(d->getDriver().iSound().gen.Refresh);
    d->getDriver().iSound().gen.Refresh(d->buffer);
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

    struct ICd : public audiointerface_cd_t
    {
        ICd() { de::zapPtr(this); }
    } iCd;

    struct IMusic : public audiointerface_music_t
    {
        IMusic() { de::zapPtr(this); }
    } iMusic;

    struct ISound : public audiointerface_sfx_t
    {
        ISound() { de::zapPtr(this); }
    } iSound;

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
        if(iSound.gen.Init) iBase.Event(SFXEV_BEGIN);
        if(iCd.gen.Init)    iCd.gen.Update();
        if(iMusic.gen.Init) iMusic.gen.Update();
    }

    void systemFrameEnds(audio::System &)
    {
        DENG2_ASSERT(initialized);
        iBase.Event(SFXEV_END);
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
    LOG_AS("~audio::PluginDriver");
    deinitialize();  // If necessary.
}

PluginDriver *PluginDriver::newFromLibrary(LibraryFile &libFile)  // static
{
    try
    {
        std::unique_ptr<PluginDriver> driver(new PluginDriver);
        PluginDriver &inst = *driver;

        inst.d->library = Library_New(libFile.path().toUtf8().constData());
        if(!inst.d->library)
        {
            /// @todo fixme: Should not be failing here! -ds
            return nullptr;
        }

        de::Library &lib = libFile.library();

        lib.setSymbolPtr( inst.d->iBase.Init,     "DS_Init");
        lib.setSymbolPtr( inst.d->iBase.Shutdown, "DS_Shutdown");
        lib.setSymbolPtr( inst.d->iBase.Event,    "DS_Event");
        lib.setSymbolPtr( inst.d->iBase.Get,      "DS_Get");
        lib.setSymbolPtr( inst.d->iBase.Set,      "DS_Set", de::Library::OptionalSymbol);

        if(lib.hasSymbol("DS_SFX_Init"))
        {
            lib.setSymbolPtr( inst.d->iSound.gen.Init,      "DS_SFX_Init");
            //lib.setSymbolPtr( inst.d->iSound.gen.Shutdown,  "DM_SFX_Shutdown", de::Library::OptionalSymbol);
            lib.setSymbolPtr( inst.d->iSound.gen.Create,    "DS_SFX_CreateBuffer");
            lib.setSymbolPtr( inst.d->iSound.gen.Destroy,   "DS_SFX_DestroyBuffer");
            lib.setSymbolPtr( inst.d->iSound.gen.Load,      "DS_SFX_Load");
            lib.setSymbolPtr( inst.d->iSound.gen.Reset,     "DS_SFX_Reset");
            lib.setSymbolPtr( inst.d->iSound.gen.Play,      "DS_SFX_Play");
            lib.setSymbolPtr( inst.d->iSound.gen.Stop,      "DS_SFX_Stop");
            lib.setSymbolPtr( inst.d->iSound.gen.Refresh,   "DS_SFX_Refresh");
            lib.setSymbolPtr( inst.d->iSound.gen.Set,       "DS_SFX_Set");
            lib.setSymbolPtr( inst.d->iSound.gen.Setv,      "DS_SFX_Setv");
            lib.setSymbolPtr( inst.d->iSound.gen.Listener,  "DS_SFX_Listener");
            lib.setSymbolPtr( inst.d->iSound.gen.Listenerv, "DS_SFX_Listenerv");
            lib.setSymbolPtr( inst.d->iSound.gen.Getv,      "DS_SFX_Getv");
        }

        if(lib.hasSymbol("DM_Music_Init"))
        {
            lib.setSymbolPtr( inst.d->iMusic.gen.Init,      "DM_Music_Init");
            lib.setSymbolPtr( inst.d->iMusic.gen.Shutdown,  "DM_Music_Shutdown", de::Library::OptionalSymbol);
            lib.setSymbolPtr( inst.d->iMusic.gen.Update,    "DM_Music_Update");
            lib.setSymbolPtr( inst.d->iMusic.gen.Get,       "DM_Music_Get");
            lib.setSymbolPtr( inst.d->iMusic.gen.Set,       "DM_Music_Set");
            lib.setSymbolPtr( inst.d->iMusic.gen.Pause,     "DM_Music_Pause");
            lib.setSymbolPtr( inst.d->iMusic.gen.Stop,      "DM_Music_Stop");
            lib.setSymbolPtr( inst.d->iMusic.SongBuffer,    "DM_Music_SongBuffer", de::Library::OptionalSymbol);
            lib.setSymbolPtr( inst.d->iMusic.Play,          "DM_Music_Play",       de::Library::OptionalSymbol);
            lib.setSymbolPtr( inst.d->iMusic.PlayFile,      "DM_Music_PlayFile",   de::Library::OptionalSymbol);
        }

        if(lib.hasSymbol("DM_CDAudio_Init"))
        {
            lib.setSymbolPtr( inst.d->iCd.gen.Init,         "DM_CDAudio_Init");
            lib.setSymbolPtr( inst.d->iCd.gen.Shutdown,     "DM_CDAudio_Shutdown", de::Library::OptionalSymbol);
            lib.setSymbolPtr( inst.d->iCd.gen.Update,       "DM_CDAudio_Update");
            lib.setSymbolPtr( inst.d->iCd.gen.Set,          "DM_CDAudio_Set");
            lib.setSymbolPtr( inst.d->iCd.gen.Get,          "DM_CDAudio_Get");
            lib.setSymbolPtr( inst.d->iCd.gen.Pause,        "DM_CDAudio_Pause");
            lib.setSymbolPtr( inst.d->iCd.gen.Stop,         "DM_CDAudio_Stop");
            lib.setSymbolPtr( inst.d->iCd.Play,             "DM_CDAudio_Play");
        }

        return driver.release();
    }
    catch(de::Library::SymbolMissingError const &er)
    {
        LOG_AS("audio::PluginDriver");
        LOG_AUDIO_ERROR("") << er.asText();
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
    LOG_AS("audio::PluginDriver");

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
    LOG_AS("audio::PluginDriver");

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

    if(d->iCd.gen.Init != nullptr)
    {
        String const idKey = d->getPlayerPropertyAsString(d->cd, MUSIP_IDENTITYKEY);
        if(!idKey.isEmpty())
        {
            Record rec;
            rec.addNumber("type",        System::AUDIO_ICD);
            rec.addText  ("identityKey", DotPath(driverIdKey) / idKey);
            list << rec;  // A copy is made.
        }
        else DENG2_ASSERT(!"[MUSIP_IDENTITYKEY not defined]");
    }
    if(d->iMusic.gen.Init != nullptr)
    {
        String const idKey = d->getPlayerPropertyAsString(d->music, MUSIP_IDENTITYKEY);
        if(!idKey.isEmpty())
        {
            Record rec;
            rec.addNumber("type",        System::AUDIO_IMUSIC);
            rec.addText  ("identityKey", DotPath(driverIdKey) / idKey);
            list << rec;
        }
        else DENG2_ASSERT(!"[MUSIP_IDENTITYKEY not defined]");
    }
    if(d->iSound.gen.Init != nullptr)
    {
        String const idKey = d->getPlayerPropertyAsString(d->sound, SFXIP_IDENTITYKEY);
        if(!idKey.isEmpty())
        {
            Record rec;
            rec.addNumber("type",        System::AUDIO_ISFX);
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
    /// @throw MissingPlayerError  Unknown interface referenced.
    throw MissingPlayerError("PluginDriver::findPlayer", "Unknown playback interface \"" + interfaceIdentityKey + "\"");
}

IPlayer *PluginDriver::tryFindPlayer(String interfaceIdentityKey) const
{
    interfaceIdentityKey = interfaceIdentityKey.toLower();

    if(d->iCd.gen.Init != nullptr)
    {
        if(d->getPlayerPropertyAsString(d->cd   , MUSIP_IDENTITYKEY) == interfaceIdentityKey)
            return &d->cd;
    }
    if(d->iMusic.gen.Init != nullptr)
    {
        if(d->getPlayerPropertyAsString(d->music, MUSIP_IDENTITYKEY) == interfaceIdentityKey)
            return &d->music;
    }
    if(d->iSound.gen.Init != nullptr)
    {
        if(d->getPlayerPropertyAsString(d->sound, SFXIP_IDENTITYKEY) == interfaceIdentityKey)
            return &d->sound;
    }

    return nullptr;  // Not found.
}

audiointerface_cd_t &PluginDriver::iCd() const
{
    return d->iCd;
}

audiointerface_music_t &PluginDriver::iMusic() const
{
    return d->iMusic;
}

audiointerface_sfx_t &PluginDriver::iSound() const
{
    return d->iSound;
}

}  // namespace audio
