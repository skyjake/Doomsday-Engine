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

#include "api_audiod.h"      // AUDIOP_* flags
#include "api_audiod_mus.h"
#include "api_audiod_sfx.h"
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
    PluginDriver &driver;          ///< Owning driver.
    bool noUpdate    = false;      ///< @c true if skipping updates (when stopped, before deletion).
    dint startTime   = 0;          ///< When playback last started (Ticks).

    Positioning positioning = { StereoPositioning };
    dfloat frequency = 1;          ///< {0..1} Frequency/pitch adjustment factor.
    dfloat volume    = 1;          ///< {0..1} Volume adjustment factor.

    Listener *listener = nullptr;  ///< Listener for the sound, if any (not owned).

    // Only necessary when using 3D positioning.
    /// @todo optimize: stop observing when this changes. -ds
    bool needEnvironmentUpdate = false;

    struct EmitterData
    {
        bool noOrigin            = true;     ///< @c true if the originator is some mystical emitter.
        bool noVolumeAttenuation = true;     ///< @c true if (distance based) volume attenuation is disabled.
        SoundEmitter *tracking   = nullptr;  ///< Emitter to track, if any (not owned).
        Vector3d origin;                     ///< Emit from here (synced with emitter).

        void updateOriginIfNeeded()
        {
            // Only if we are tracking an emitter.
            if(!tracking) return;

            origin = Vector3d(tracking->origin);

            // When tracking a map-object set the Z axis position to the object's center.
            if(Thinker_IsMobjFunc(tracking->thinker.function))
            {
                origin.z += ((mobj_t *)tracking)->height / 2;
            }
        }
    } emitter;

    struct Buffer
    {
        /// No data is attached. @ingroup errors
        DENG2_ERROR(MissingDataError);

        dint sampleBytes  = 1;        ///< Bytes per sample (1 or 2).
        dint sampleRate   = 11025;    ///< Number of samples per second.
        sfxbuffer_t *data = nullptr;  ///< External data buffer, if any (not owned).

        ~Buffer() { DENG2_ASSERT(data == nullptr); }

        sfxbuffer_t &getData() const
        {
            if(data) return *data;
            /// @throw MissingDataError  No data is currently attached.
            throw MissingDataError("audio::PluginDriver::SoundChannel::Instance", "No data attached");
        }
    } buffer;

    Instance(PluginDriver &owner) : driver(owner) {}

    ~Instance()
    {
        // Stop observing the configured listener (if we haven't already).
        observeListener(nullptr);
    }

    /**
     * Determines whether the channel is configured such that the emitter *is* the listener.
     */
    bool emitterIsListener() const
    {
        return listener
               && emitter.tracking
               && emitter.tracking == (ddmobj_base_t const *)listener->trackedMapObject();
    }

    /**
     * Determines whether the channel is configured to use a moveable emitter.
     */
    bool emitterIsMoving() const
    {
        return emitter.tracking
               && !emitterIsListener()
               && Thinker_IsMobjFunc(emitter.tracking->thinker.function);
    }

    /**
     * Determines whether the channel is configured to play an "originless" sound.
     */
    bool noOrigin() const
    {
        return emitter.noOrigin || emitterIsListener();
    }

    /**
     * Begin observing @a newListener for porientation/translation and environment changes
     * which we'll apply to the channel when beginning playback (and updating each frame).
     *
     * @note Listeners are Stage components and not simple properties of a/the currently
     * playing sound in order to minimize the effects of playing new sounds on previously
     * configured channels (i.e., tracking Stage changes independently).
     */
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

    /**
     * Writes deferred Listener and/or Environment changes to the audio driver.
     *
     * @param force  Usually updates are only necessary during playback. Use @c true to
     * override this check and write the changes regardless.
     */
    void writeDeferredProperties(bool force = false)
    {
        if(!buffer.data) return;
        sfxbuffer_t *buf = &buffer.getData();

        // Disabled?
        if(noUpdate) return;

        // Updates are only necessary during playback.
        if(!(buf->flags & SFXBF_PLAYING) && !force) return;

        // When tracking an emitter we need the latest origin coordinates.
        emitter.updateOriginIfNeeded();

        // Frequency is common to both 2D and 3D sounds.
        driver.iSound().gen.Set(buf, SFXBP_FREQUENCY, frequency);

        // Use Absolute/Relative-Positioning (in 3D)?
        if(buf->flags & SFXBF_3D)
        {
            // Volume is affected only by maxvol.
            driver.iSound().gen.Set(buf, SFXBP_VOLUME, volume * System::get().soundVolume() / 255.0f);

            if(emitterIsListener())
            {
                // Position relative to the listener.
                dfloat vec[] = { 0, 0, 0 };
                driver.iSound().gen.Set (buf, SFXBP_RELATIVE_MODE, 1/*headRelative*/);
                driver.iSound().gen.Setv(buf, SFXBP_POSITION, vec);
            }
            else
            {
                // Position at the origin of the emitter.
                dfloat vec[3]; emitter.origin.toVector3f().decompose(vec);
                driver.iSound().gen.Set (buf, SFXBP_RELATIVE_MODE, 0/*absolute*/);
                driver.iSound().gen.Setv(buf, SFXBP_POSITION, vec);
            }

            // Update the emitter velocity.
            if(emitterIsMoving())
            {
                DENG2_ASSERT(emitter.tracking);
                dfloat vec[3]; (Vector3d(((mobj_t *)emitter.tracking)->mom) * TICSPERSEC).toVector3f().decompose(vec);
                driver.iSound().gen.Setv(buf, SFXBP_VELOCITY, vec);
            }
            else
            {
                // Not moving.
                dfloat vec[] = { 0, 0, 0 };
                driver.iSound().gen.Setv(buf, SFXBP_VELOCITY, vec);
            }
        }
        // Use StereoPositioning.
        else
        {
            dfloat volAtten = 1;  // No attenuation.
            dfloat panning  = 0;  // No panning.

            if(listener && !noOrigin())
            {
                // Apply listener angle based source panning?
                if(mobj_t const *tracking = listener->trackedMapObject())
                {
                    dfloat angle = listener->angleFrom(emitter.origin);
                    // We want a signed angle.
                    if(angle > 180) angle -= 360;

                    if(angle <= 90 && angle >= -90)  // Front half.
                    {
                        panning = -angle / 90;
                    }
                    else  // Back half.
                    {
                        panning = (angle + (angle > 0 ? -180 : 180)) / 90;
                    }
                }

                // Apply listener distance based volume attenuation?
                if(!emitter.noVolumeAttenuation)
                {
                    ddouble const dist      = listener->distanceFrom(emitter.origin);
                    Ranged const attenRange = listener->volumeAttenuationRange();

                    if(dist >= attenRange.start)
                    {
                        if(dist > attenRange.end)
                        {
                            // Won't be heard.
                            volAtten = 0;
                        }
                        else
                        {
                            // Calculate roll-off attenuation [.125/(.125+x), x=0..1]
                            // Apply a linear factor to ensure absolute silence at the
                            // maximum distance.
                            ddouble ip = (dist - attenRange.start) / attenRange.size();
                            volAtten = de::clamp<dfloat>(0, .125f / (.125f + ip) * (1 - ip), 1);
                        }
                    }
                }
            }

            if(!de::fequal(panning, 0))
            {
                // Dampen sounds coming from behind the listener.
                volAtten *= (1 + (panning > 0 ? panning : -panning)) / 2;
            }

            driver.iSound().gen.Set(buf, SFXBP_VOLUME, volume * volAtten * System::get().soundVolume() / 255.0f);
            driver.iSound().gen.Set(buf, SFXBP_PAN, panning);
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

    void systemFrameEnds(System &)
    {
        writeDeferredProperties();
    }
};

PluginDriver::SoundChannel::SoundChannel(PluginDriver &owner)
    : audio::SoundChannel()
    , d(new Instance(owner))
{}

Channel::PlayingMode PluginDriver::SoundChannel::mode() const
{
    if(!d->buffer.data || !(d->buffer.getData().flags & SFXBF_PLAYING)) return NotPlaying;
    if( d->buffer.getData().flags & SFXBF_REPEAT)                       return Looping;
    if( d->buffer.getData().flags & SFXBF_DONT_STOP)                    return OnceDontDelete;
    return Once;
}

void PluginDriver::SoundChannel::play(PlayingMode mode)
{
    if(isPlaying()) return;

    if(mode == NotPlaying) return;

    DENG2_ASSERT(d->buffer.data);
    sfxbuffer_t *buf = &d->buffer.getData();
    buf->flags &= ~(SFXBF_REPEAT | SFXBF_DONT_STOP);
    switch(mode)
    {
    case Looping:        buf->flags |= SFXBF_REPEAT;    break;
    case OnceDontDelete: buf->flags |= SFXBF_DONT_STOP; break;
    default: break;
    }

    // When playing on a sound stage with a Listener, we may need to update the channel
    // dynamically during playback.
    d->observeListener(&System::get().worldStage().listener());

    // Flush deferred property value changes to the assigned data buffer.
    if(d->driver.iSound().gen.Listener)
    {
        d->driver.iSound().gen.Listener(SFXLP_UPDATE, 0);
    }
    d->writeDeferredProperties(true/*force*/);

    // 3D sounds need a few extra properties set up.
    if(buf->flags & SFXBF_3D)
    {
        DENG2_ASSERT(d->driver.iSound().gen.Set);

        // Configure the attentuation distances.
        // This is only done once, when the sound is first played (i.e., here).
        if(d->emitter.noVolumeAttenuation)
        {
            d->driver.iSound().gen.Set(buf, SFXBP_MIN_DISTANCE, 10000);
            d->driver.iSound().gen.Set(buf, SFXBP_MAX_DISTANCE, 20000);
        }
        else
        {
            Ranged const &range = System::get().worldStage().listener().volumeAttenuationRange();
            d->driver.iSound().gen.Set(buf, SFXBP_MIN_DISTANCE, dfloat( range.start ));
            d->driver.iSound().gen.Set(buf, SFXBP_MAX_DISTANCE, dfloat( range.end ));
        }
    }

    DENG2_ASSERT(d->driver.iSound().gen.Play);
    d->driver.iSound().gen.Play(buf);

    d->startTime = Timer_Ticks();  // Note the current time.
}

void PluginDriver::SoundChannel::stop()
{
    if(!d->buffer.data) return;
    DENG2_ASSERT(d->driver.iSound().gen.Stop);
    d->driver.iSound().gen.Stop(&d->buffer.getData());
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
    return d->emitter.tracking;
}

dfloat PluginDriver::SoundChannel::frequency() const
{
    return d->frequency;
}

Vector3d PluginDriver::SoundChannel::origin() const
{
    return d->emitter.origin;
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
    d->emitter.tracking = newEmitter;
    return *this;
}

audio::SoundChannel &PluginDriver::SoundChannel::setFrequency(dfloat newFrequency)
{
    d->frequency = newFrequency;
    return *this;
}

audio::SoundChannel &PluginDriver::SoundChannel::setOrigin(Vector3d const &newOrigin)
{
    d->emitter.origin = newOrigin;
    return *this;
}

Channel &PluginDriver::SoundChannel::setVolume(dfloat newVolume)
{
    d->volume = newVolume;
    return *this;
}

dint PluginDriver::SoundChannel::flags() const
{
    dint flags = 0;

    if(d->emitter.noOrigin)            flags |= SFXCF_NO_ORIGIN;
    if(d->emitter.noVolumeAttenuation) flags |= SFXCF_NO_ATTENUATION;

    if(d->noUpdate) flags |= SFXCF_NO_UPDATE;

    return flags;
}

void PluginDriver::SoundChannel::setFlags(dint flags)
{
    d->emitter.noOrigin            = CPP_BOOL(flags & SFXCF_NO_ORIGIN);
    d->emitter.noVolumeAttenuation = CPP_BOOL(flags & SFXCF_NO_ATTENUATION);

    d->noUpdate = CPP_BOOL(flags & SFXCF_NO_UPDATE);
}

void PluginDriver::SoundChannel::update()
{
    if(!d->buffer.data) return;
    DENG2_ASSERT(d->driver.iSound().gen.Refresh);
    d->driver.iSound().gen.Refresh(d->buffer.data);
}

void PluginDriver::SoundChannel::reset()
{
    if(!d->buffer.data) return;
    DENG2_ASSERT(d->driver.iSound().gen.Reset);
    d->driver.iSound().gen.Reset(d->buffer.data);
}

bool PluginDriver::SoundChannel::format(Positioning positioning, dint bytesPer, dint rate)
{
    // We may need to replace the playback data buffer.
    if(!d->buffer.data
       || d->positioning        != positioning
       || d->buffer.sampleBytes != bytesPer
       || d->buffer.sampleRate  != rate)
    {
        stop();

        DENG2_ASSERT(!isPlaying());
        if(d->buffer.data)
        {
            // Cancel frame notifications - we'll soon have no buffer to update.
            System::get().audienceForFrameEnds() -= d;

            d->driver.iSound().gen.Destroy(d->buffer.data);
            d->buffer.data = nullptr;
        }

        /// @todo Don't duplicate state! -ds
        d->positioning        = positioning;
        d->buffer.sampleBytes = bytesPer;
        d->buffer.sampleRate  = rate;

        d->buffer.data =
            d->driver.iSound().gen.Create(d->positioning == AbsolutePositioning ? SFXBF_3D : 0,
                                          d->buffer.sampleBytes * 8, d->buffer.sampleRate);
        if(d->buffer.data)
        {
            // We want notification when the frame ends in order to flush deferred property writes.
            System::get().audienceForFrameEnds() += d;
        }
    }
    return isValid();
}

bool PluginDriver::SoundChannel::isValid() const
{
    return d->buffer.data != nullptr;
}

void PluginDriver::SoundChannel::load(sfxsample_t const &sample)
{
    // Don't reload if a sample with the same sound ID is already loaded.
    sfxbuffer_t &buffer = d->buffer.getData();
    if(!buffer.sample || buffer.sample->soundId != sample.soundId)
    {
        DENG2_ASSERT(d->driver.iSound().gen.Load);
        d->driver.iSound().gen.Load(&buffer, &const_cast<sfxsample_t &>(sample));
    }
}

dint PluginDriver::SoundChannel::bytes() const
{
    return d->buffer.sampleBytes;
}

dint PluginDriver::SoundChannel::rate() const
{
    return d->buffer.sampleRate;
}

dint PluginDriver::SoundChannel::startTime() const
{
    return d->startTime;
}

duint PluginDriver::SoundChannel::endTime() const
{
    return isValid() && d->buffer.data ? d->buffer.getData().endTime : 0;
}

sfxsample_t const *PluginDriver::SoundChannel::samplePtr() const
{
    return d->buffer.data ? d->buffer.getData().sample : nullptr;
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
        dfloat position[4]; Vector4f(listener.position().toVector3f(), 0).decompose(position);
        d->driver.iSound().gen.Listenerv(SFXLP_POSITION, position);

        dfloat orientation[2]; listener.orientation().toVector2f().decompose(orientation);
        d->driver.iSound().gen.Listenerv(SFXLP_ORIENTATION, orientation);

        dfloat velocity[4]; Vector4f(listener.velocity().toVector3f() * TICSPERSEC, 0).decompose(velocity);
        d->driver.iSound().gen.Listenerv(SFXLP_VELOCITY, velocity);
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

// --------------------------------------------------------------------------------------

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

    struct CdPlayer : public IPlayer, public audiointerface_cd_t
    {
        bool initialized     = false;
        bool needInit        = true;
        PluginDriver *driver = nullptr;

        CdPlayer(PluginDriver &driver) : driver(&driver)
        {
            de::zap(gen);
            Play = nullptr;
        }

        dint initialize()
        {
            if(needInit)
            {
                needInit = false;
                DENG2_ASSERT(gen.Init);
                initialized = gen.Init();
            }
            return initialized;
        }

        void deinitialize()
        {
            if(!initialized) return;

            initialized = false;
            if(gen.Shutdown)
            {
                gen.Shutdown();
            }
            needInit = true;
        }
    } cd;

    struct MusicPlayer : public IPlayer, public audiointerface_music_t
    {
        bool initialized     = false;
        bool needInit        = true;
        PluginDriver *driver = nullptr;

        MusicPlayer(PluginDriver &driver) : driver(&driver)
        {
            de::zap(gen);
            SongBuffer = nullptr;
            Play       = nullptr;
            PlayFile   = nullptr;
        }

        dint initialize()
        {
            if(needInit)
            {
                needInit = false;
                DENG2_ASSERT(gen.Init);
                initialized = gen.Init();
            }
            return initialized;
        }

        void deinitialize()
        {
            if(!initialized) return;

            initialized = false;
            if(gen.Shutdown)
            {
                gen.Shutdown();
            }
            needInit = true;
        }
    } music;

    struct SoundPlayer : public ISoundPlayer, public audiointerface_sfx_t
    , DENG2_OBSERVES(SampleCache, SampleRemove)
    {
        PluginDriver *driver = nullptr;
        bool needInit        = true;
        bool initialized     = false;

        thread_t refreshThread      = nullptr;
        volatile bool refreshPaused = false;
        volatile bool refreshing    = false;

        SoundPlayer(PluginDriver &driver) : driver(&driver)
        {
            de::zap(gen);
        }

        ~SoundPlayer() { DENG2_ASSERT(!initialized); }

        /**
         * Returns @c true if any frequency/sample rate is permitted for audio data.
         */
        bool anyRateAccepted() const
        {
            dint anyRateAccepted = 0;
            if(gen.Getv)
            {
                gen.Getv(SFXIP_ANY_SAMPLE_RATE_ACCEPTED, &anyRateAccepted);
            }
            return CPP_BOOL( anyRateAccepted );
        }

        /**
         * Returns @c true if manual refreshing of playback Channels is needed.
         */
        bool needsRefresh() const
        {
            if(!initialized) return false;
            dint disableRefresh = false;
            if(gen.Getv)
            {
                gen.Getv(SFXIP_DISABLE_CHANNEL_REFRESH, &disableRefresh);
            }
            return !disableRefresh;
        }

        dint initialize()
        {
            if(needInit)
            {
                needInit = false;
                DENG2_ASSERT(gen.Init);
                initialized = gen.Init();

                if(initialized)
                {
                    if(gen.Listener && gen.Listenerv)
                    {
                        // Change the primary buffer format to match the channel format.
                        dfloat pformat[2] = { dfloat(::sfxBits), dfloat(::sfxRate) };
                        gen.Listenerv(SFXLP_PRIMARY_FORMAT, pformat);

                        // This is based on the scientific calculations that if the DOOM marine
                        // is 56 units tall, 60 is about two meters.
                        //// @todo Derive from the viewheight.
                        gen.Listener(SFXLP_UNITS_PER_METER, 30);
                        gen.Listener(SFXLP_DOPPLER, 1.5f);

                        dfloat rev[4] = { 0, 0, 0, 0 };
                        gen.Listenerv(SFXLP_REVERB, rev);
                        gen.Listener(SFXLP_UPDATE, 0);
                    }

                    audio::System::get().sampleCache().audienceForSampleRemove() += this;
                }
            }
            return initialized;
        }

        void deinitialize()
        {
            if(!initialized) return;

            // Cancel sample cache removal notification - we intend to clear sounds.
            audio::System::get().sampleCache().audienceForSampleRemove() -= this;

            // Stop any channels still playing (note: does not affect refresh).
            for(Channel *channel : driver->d->channels[AUDIO_ISFX])
            {
                channel->stop();
            }

            initialized = false;  // Signal the refresh thread to stop.
            pauseRefresh();       // Stop further refreshing if in progress.

            if(refreshThread)
            {
                // Wait for the refresh thread to stop.
                Sys_WaitThread(refreshThread, 2000, nullptr);
                refreshThread = nullptr;
            }

            /*if(gen.Shutdown)
            {
                gen.Shutdown();
            }*/

            qDeleteAll(driver->d->channels[AUDIO_ISFX]);
            driver->d->channels[AUDIO_ISFX].clear();

            needInit = true;
        }

        void allowRefresh(bool allow)
        {
            if(!initialized) return;
            if(!needsRefresh()) return;

            if(allow)
            {
                resumeRefresh();
            }
            else
            {
                pauseRefresh();
            }
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
        static dint C_DECL RefreshThread(void *player)
        {
            auto &inst = *static_cast<SoundPlayer *>(player);

            // We'll continue looping until the player is deinitialized.
            while(inst.initialized)
            {
                // The bit is swapped on each refresh (debug info).
                //::refMonitor ^= 1;

                if(!inst.refreshPaused)
                {
                    // Do the refresh.
                    inst.refreshing = true;
                    for(Channel *channel : inst.driver->d->channels[AUDIO_ISFX])
                    {
                        if(channel->isPlaying())
                        {
                            channel->as<SoundChannel>().update();
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

        /**
         * The given @a sample will soon no longer exist. All channels currently loaded
         * with it must be reset.
         */
        void sampleCacheAboutToRemove(Sample const &sample)
        {
            pauseRefresh();
            for(Channel *base : driver->d->channels[AUDIO_ISFX])
            {
                auto &ch = base->as<SoundChannel>();

                if(!ch.isValid()) continue;

                if(ch.samplePtr() && ch.samplePtr()->soundId == sample.soundId)
                {
                    // Stop and unload.
                    ch.reset();
                }
            }
            resumeRefresh();
        }
    } sound;

    struct ChannelSet : public QList<Channel *>
    {
        ~ChannelSet() { DENG2_ASSERT(isEmpty()); }
    } channels[PlaybackInterfaceTypeCount];

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

    void clearChannels()
    {
        for(ChannelSet &set : channels)
        {
            qDeleteAll(set);
            set.clear();
        }
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

Channel *PluginDriver::makeChannel(PlaybackInterfaceType type)
{
    if(!d->initialized)
        return nullptr;

    switch(type)
    {
    case AUDIO_ICD:
        // Initialize this interface now if we haven't already.
        if(d->cd.initialize())
        {
            std::unique_ptr<Channel> channel(new CdChannel(*this));
            d->channels[type] << channel.get();
            return channel.release();
        }
        break;

    case AUDIO_IMUSIC:
        if(d->music.initialize())
        {
            std::unique_ptr<Channel> channel(new MusicChannel(*this));
            d->channels[type] << channel.get();
            return channel.release();
        }
        break;

    case AUDIO_ISFX:
        if(d->sound.initialize())
        {
            std::unique_ptr<Channel> channel(new SoundChannel(*this));
            d->channels[type] << channel.get();
            if(d->channels[type].count() == 1)
            {
                if(d->sound.gen.Listenerv)
                {
                    // Change the primary buffer format to match the channel format.
                    dfloat pformat[2] = { dfloat(::sfxBits), dfloat(::sfxRate) };
                    d->sound.gen.Listenerv(SFXLP_PRIMARY_FORMAT, pformat);

                    dfloat rev[4] = { 0, 0, 0, 0 };
                    d->sound.gen.Listenerv(SFXLP_REVERB, rev);
                    d->sound.gen.Listener(SFXLP_UPDATE, 0);
                }

                // Start the channel refresh thread. It will stop on its own when it notices that
                // the player is deinitialized.
                d->sound.refreshing    = false;
                d->sound.refreshPaused = false;

                // Start the refresh thread.
                d->sound.refreshThread = Sys_StartThread(Instance::SoundPlayer::RefreshThread, &d->sound);
                if(!d->sound.refreshThread)
                {
                    throw Error("PluginDriver::makeChannel", "Failed starting the refresh thread");
                }
            }
            return channel.release();
        }
        break;

    default: break;
    }

    return nullptr;
}

LoopResult PluginDriver::forAllChannels(PlaybackInterfaceType type,
    std::function<LoopResult (Channel const &)> callback) const
{
    for(Channel const *ch : d->channels[type])
    {
        if(auto result = callback(*ch))
            return result;
    }
    return LoopContinue;
}

struct audiointerface_cd_s &PluginDriver::iCd() const
{
    return d->cd;
}

struct audiointerface_music_s &PluginDriver::iMusic() const
{
    return d->music;
}

struct audiointerface_sfx_s &PluginDriver::iSound() const
{
    return d->sound;
}

}  // namespace audio
