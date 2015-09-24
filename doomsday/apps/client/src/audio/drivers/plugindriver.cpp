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
#include "audio/sound.h"
#include "world/thinkers.h"
#include "def_main.h"        // SF_* flags, remove me
#include <de/Library>
#include <de/Log>
#include <de/Observers>
#include <de/NativeFile>
#include <de/timer.h>        // TICSPERSEC
#include <QList>
#include <QtAlgorithms>

using namespace de;

namespace audio {

PluginDriver::CdPlayer::CdPlayer(PluginDriver &driver) : ICdPlayer(driver)
{}

String PluginDriver::CdPlayer::name() const
{
    char buf[256];  /// @todo This could easily overflow...
    DENG2_ASSERT(driver().as<PluginDriver>().iCd().gen.Get);
    if(driver().as<PluginDriver>().iCd().gen.Get(MUSIP_ID, buf)) return buf;

    DENG2_ASSERT(!"[MUSIP_ID not defined]");
    return "unnamed_music";
}

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

PluginDriver::MusicPlayer::MusicPlayer(PluginDriver &driver) : IMusicPlayer(driver)
{}

String PluginDriver::MusicPlayer::name() const
{
    char buf[256];  /// @todo This could easily overflow...
    DENG2_ASSERT(driver().as<PluginDriver>().iMusic().gen.Get);
    if(driver().as<PluginDriver>().iMusic().gen.Get(MUSIP_ID, buf)) return buf;

    DENG2_ASSERT(!"[MUSIP_ID not defined]");
    return "unnamed_music";
}

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
{
    bool needInit    = true;
    bool initialized = false;
    QList<PluginDriver::Sound *> sounds;

    ~Instance()
    {
        // Should have been deinitialized by now.
        DENG2_ASSERT(!initialized);
    }

    void clearSounds()
    {
        qDeleteAll(sounds);
        sounds.clear();
    }
};

PluginDriver::SoundPlayer::SoundPlayer(PluginDriver &driver)
    : ISoundPlayer(driver)
    , d(new Instance)
{}

String PluginDriver::SoundPlayer::name() const
{
    /// @todo SFX interfaces aren't named yet.
    return "unnamed_sfx";
}

dint PluginDriver::SoundPlayer::initialize()
{
    if(d->needInit)
    {
        d->needInit = false;
        DENG2_ASSERT(driver().as<PluginDriver>().iSound().gen.Init);
        d->initialized = driver().as<PluginDriver>().iSound().gen.Init();
    }
    return d->initialized;
}

void PluginDriver::SoundPlayer::deinitialize()
{
    if(!d->initialized) return;

    d->initialized = false;
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
    
    inline audiointerface_sfx_t &getDriverISound()
    {
        DENG2_ASSERT(player != nullptr);
        return player->driver().as<PluginDriver>().iSound();
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
            if(emitter && emitter == System::get().sfxListener())
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
            if(emitter && emitter != System::get().sfxListener() &&
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
               (emitter && emitter == System::get().sfxListener()))
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
                if(mobj_t *listener = System::get().sfxListener())
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

    void load(sfxbuffer_t &buf, sfxsample_t &sample)
    {
        DENG2_ASSERT(getDriverISound().gen.Load);
        getDriverISound().gen.Load(&buf, &sample);
    }

    void stop(sfxbuffer_t &buf)
    {
        DENG2_ASSERT(getDriverISound().gen.Stop);
        getDriverISound().gen.Stop(&buf);
    }

    void reset(sfxbuffer_t &buf)
    {
        DENG2_ASSERT(getDriverISound().gen.Reset);
        getDriverISound().gen.Reset(&buf);
    }

    void play(sfxbuffer_t &buf)
    {
        DENG2_ASSERT(getDriverISound().gen.Play);
        getDriverISound().gen.Play(&buf);
    }

    bool isPlaying(sfxbuffer_t &buf) const
    {
        return (buf.flags & SFXBF_PLAYING) != 0;
    }

    void refresh(sfxbuffer_t &buf)
    {
        DENG2_ASSERT(getDriverISound().gen.Refresh);
        getDriverISound().gen.Refresh(&buf);
    }

    void setFrequency(sfxbuffer_t &buf, dfloat newFrequency)
    {
        DENG2_ASSERT(getDriverISound().gen.Set);
        getDriverISound().gen.Set(&buf, SFXBP_FREQUENCY, newFrequency);
    }

    void setOrigin(sfxbuffer_t &buf, Vector3d const &newOrigin)
    {
        DENG2_ASSERT(getDriverISound().gen.Setv);
        dfloat vec[3]; newOrigin.toVector3f().decompose(vec);
        getDriverISound().gen.Setv(&buf, SFXBP_POSITION, vec);
    }

    void setPan(sfxbuffer_t &buf, dfloat newPan)
    {
        DENG2_ASSERT(getDriverISound().gen.Set);
        getDriverISound().gen.Set(&buf, SFXBP_PAN, newPan);
    }

    void setPositioning(sfxbuffer_t &buf, bool headRelative)
    {
        DENG2_ASSERT(getDriverISound().gen.Set);
        getDriverISound().gen.Set(&buf, SFXBP_RELATIVE_MODE, dfloat( headRelative ));
    }

    void setVelocity(sfxbuffer_t &buf, Vector3d const &newVelocity)
    {
        DENG2_ASSERT(getDriverISound().gen.Setv);
        dfloat vec[3]; newVelocity.toVector3f().decompose(vec);
        getDriverISound().gen.Setv(&buf, SFXBP_VELOCITY, vec);
    }

    void setVolume(sfxbuffer_t &buf, dfloat newVolume)
    {
        DENG2_ASSERT(getDriverISound().gen.Set);
        getDriverISound().gen.Set(&buf, SFXBP_VOLUME, newVolume);
    }

    void setVolumeAttenuationRange(sfxbuffer_t &buf, Ranged const &newRange)
    {
        DENG2_ASSERT(getDriverISound().gen.Set);
        getDriverISound().gen.Set(&buf, SFXBP_MIN_DISTANCE, dfloat( newRange.start ));
        getDriverISound().gen.Set(&buf, SFXBP_MAX_DISTANCE, dfloat( newRange.end ));
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

void PluginDriver::Sound::releaseBuffer()
{
    stop();
    if(!hasBuffer()) return;

    // Cancel frame notifications - we'll soon have no buffer to update.
    System::get().audienceForFrameEnds() -= d;

    d->getDriverISound().gen.Destroy(d->buffer);
    d->buffer = nullptr;
}

void PluginDriver::Sound::setBuffer(sfxbuffer_t *newBuffer)
{
    releaseBuffer();
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
        setBuffer(d->getDriverISound().gen.Create(stereoPositioning ? 0 : SFXBF_3D, bytesPer, rate));
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

void PluginDriver::Sound::setFixedOrigin(Vector3d const &newOrigin)
{
    d->origin = newOrigin;
}

dfloat PluginDriver::Sound::priority() const
{
    if(!isPlaying())
        return SFX_LOWEST_PRIORITY;

    if(d->flags & SFXCF_NO_ORIGIN)
        return System::get().rateSoundPriority(0, 0, d->volume, d->startTime);

    // d->origin is set to emitter->xyz during updates.
    ddouble origin[3]; d->origin.decompose(origin);
    return System::get().rateSoundPriority(0, origin, d->volume, d->startTime);
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
        d->load(*d->buffer, sample);
    }
}

void PluginDriver::Sound::stop()
{
    d->stop(*d->buffer);
}

void PluginDriver::Sound::reset()
{
    d->reset(*d->buffer);
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
    d->refresh(*d->buffer);
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

    audiointerface_cd_t iCd;
    audiointerface_music_t iMusic;
    audiointerface_sfx_t iSound;

    CdPlayer cd;
    MusicPlayer music;
    SoundPlayer sound;

    Instance(Public *i)
        : Base(i)
        , cd   (self)
        , music(self)
        , sound(self)
    {
        de::zap(iCd);
        de::zap(iMusic);
        de::zap(iSound);
    }

    ~Instance()
    {
        // Should have been deinitialized by now.
        DENG2_ASSERT(!initialized);

        // Unload the library.
        Library_Delete(library);
    }

    /**
     * Lookup the value of a named @em string property from the driver.
     */
    String getPropertyAsString(dint prop)
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
            lib.setSymbolPtr( inst.d->iSound.gen.Getv,      "DS_SFX_Getv", de::Library::OptionalSymbol);
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

dint PluginDriver::playerCount() const
{
    dint count = 0;
    if(d->initialized)
    {
        if(d->iCd   .gen.Init != nullptr) count += 1;
        if(d->iMusic.gen.Init != nullptr) count += 1;
        if(d->iSound.gen.Init != nullptr) count += 1;
    }
    return count;
}

PluginDriver::IPlayer const *PluginDriver::tryFindPlayer(String name) const
{
    if(!name.isEmpty() && d->initialized)
    {
        name = name.lower();
        if(d->cd   .name() == name) return &d->cd;
        if(d->music.name() == name) return &d->music;
        if(d->sound.name() == name) return &d->sound;
    }
    return nullptr;  // Not found.
}

PluginDriver::IPlayer const &PluginDriver::findPlayer(String name) const
{
    if(auto *player = tryFindPlayer(name)) return *player;
    /// @throw MissingPlayerError  Unknown identity key specified.
    throw MissingPlayerError("PluginDriver::findPlayer", "Unknown player \"" + name + "\"");
}

LoopResult PluginDriver::forAllPlayers(std::function<LoopResult (IPlayer &)> callback) const
{
    if(d->initialized)
    {
        if(d->iCd.gen.Init != nullptr)
        {
            if(auto result = callback(d->cd))    return result;
        }
        if(d->iMusic.gen.Init != nullptr)
        {
            if(auto result = callback(d->music)) return result;
        }
        if(d->iSound.gen.Init != nullptr)
        {
            if(auto result = callback(d->sound))   return result;
        }
    }
    return LoopContinue;  // Continue iteration.
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
