/** @file sdlmixerdriver.cpp  Audio driver for playback using SDL_mixer.
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

#ifndef DENG_DISABLE_SDLMIXER

#include "audio/drivers/sdlmixerdriver.h"

#include "audio/samplecache.h"

#include "world/thinkers.h"  // Thinker_IsMobjFunc()
#include "def_main.h"        // SF_* flags, remove me
#include "sys_system.h"      // Sys_Sleep()

#include <de/Log>
#include <de/Observers>
#include <de/concurrency.h>
#include <de/memory.h>
#include <de/timer.h>        // TICSPERSEC
#include <SDL.h>
#include <SDL_mixer.h>
#include <cstdlib>
#include <cstring>
#include <QBitArray>
#include <QList>
#include <QtAlgorithms>

using namespace de;

namespace audio {

static Mix_Music *lastMusic;
static QBitArray usedChannels;

/**
 * This is the hook we ask SDL_mixer to call when music playback finishes.
 */
#ifdef DENG2_DEBUG
static void musicPlaybackFinished()
{
    LOG_AUDIO_VERBOSE("[SDLMixer] Music playback finished");
}
#endif

static dint firstUnusedChannel()
{
    for(dint i = 0; i < usedChannels.count(); ++i)
    {
        if(!usedChannels.testBit(i))
            return i;
    }
    return -1;
}

static dint acquireChannel()
{
    dint channel = firstUnusedChannel();
    if(channel < 0)
    {
        usedChannels.resize(usedChannels.count() + 1);
        channel = usedChannels.count() - 1;

        // Make sure we have enough channels allocated.
        Mix_AllocateChannels(usedChannels.count());
        Mix_UnregisterAllEffects(channel);
    }
    usedChannels.setBit(channel, true);
    return channel;
}

static void releaseChannel(dint channel)
{
    if(channel < 0) return;

    Mix_HaltChannel(channel);
    usedChannels.setBit(channel, false);
}

// ----------------------------------------------------------------------------------

SdlMixerDriver::MusicPlayer::MusicPlayer()
{}

dint SdlMixerDriver::MusicPlayer::initialize()
{
    if(!_initialized)
    {
#ifdef DENG2_DEBUG
        Mix_HookMusicFinished(musicPlaybackFinished);
#endif
        _initialized = true;
    }
    return _initialized;
}

void SdlMixerDriver::MusicPlayer::deinitialize()
{
    if(!_initialized) return;

#ifdef DENG2_DEBUG
    Mix_HookMusicFinished(nullptr);
#endif
    _initialized = false;
}

void SdlMixerDriver::MusicPlayer::update()
{
    // Nothing to update.
}

void SdlMixerDriver::MusicPlayer::setVolume(dfloat newVolume)
{
    if(!_initialized) return;
    Mix_VolumeMusic(dint( MIX_MAX_VOLUME * newVolume ));
}

bool SdlMixerDriver::MusicPlayer::isPlaying() const
{
    if(!_initialized) return false;
    return Mix_PlayingMusic();
}

void SdlMixerDriver::MusicPlayer::pause(dint pause)
{
    if(!_initialized) return;

    if(pause)
    {
        Mix_PauseMusic();
    }
    else
    {
        Mix_ResumeMusic();
    }
}

void SdlMixerDriver::MusicPlayer::stop()
{
    if(!_initialized) return;

    Mix_HaltMusic();
}

bool SdlMixerDriver::MusicPlayer::canPlayBuffer() const
{
    return false;
}

void *SdlMixerDriver::MusicPlayer::songBuffer(duint)
{
    return nullptr;
}

dint SdlMixerDriver::MusicPlayer::play(dint)
{
    return false;
}

bool SdlMixerDriver::MusicPlayer::canPlayFile() const
{
    return _initialized;
}

dint SdlMixerDriver::MusicPlayer::playFile(String const &filename, dint looped)
{
    if(!_initialized) return false;

    // Free any previously loaded music.
    if(lastMusic)
    {
        Mix_HaltMusic();
        Mix_FreeMusic(lastMusic);
    }

    lastMusic = Mix_LoadMUS(filename.toUtf8().constData());
    if(!lastMusic)
    {
        LOG_AS("SdlMixerDriver::MusicPlayer");
        LOG_AUDIO_ERROR("Failed to load music: %s") << Mix_GetError();
        return false;
    }

    return !Mix_PlayMusic(lastMusic, looped ? -1 : 1);
}

// ----------------------------------------------------------------------------------

DENG2_PIMPL_NOREF(SdlMixerDriver::SoundPlayer)
, DENG2_OBSERVES(SampleCache, SampleRemove)
{
    bool initialized = false;
    QList<SdlMixerDriver::Sound *> sounds;

    thread_t refreshThread      = nullptr;
    volatile bool refreshPaused = false;
    volatile bool refreshing    = false;

    ~Instance()
    {
        // Should be deinitialized by now.
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
                for(SdlMixerDriver::Sound *sound : inst.sounds)
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
        for(SdlMixerDriver::Sound *sound : sounds)
        {
            if(!sound->isValid()) continue;

            if(sound->samplePtr() && sound->samplePtr()->soundId == sample.soundId)
            {
                // Stop and unload.
                sound->reset();
            }
        }
        resumeRefresh();
    }
};

SdlMixerDriver::SoundPlayer::SoundPlayer()
    : d(new Instance)
{}

dint SdlMixerDriver::SoundPlayer::initialize()
{
    if(!d->initialized)
    {
        audio::System::get().sampleCache().audienceForSampleRemove() += d;
        d->initialized = true;
    }
    return true;
}

void SdlMixerDriver::SoundPlayer::deinitialize()
{
    if(!d->initialized) return;

    // Cancel sample cache removal notification - we intend to clear sounds.
    audio::System::get().sampleCache().audienceForSampleRemove() -= d;

    // Stop any sounds still playing (note: does not affect refresh).
    for(SdlMixerDriver::Sound *sound : d->sounds)
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

    d->clearSounds();
}

bool SdlMixerDriver::SoundPlayer::anyRateAccepted() const
{
    // No - please upsample for us.
    return false;
}

void SdlMixerDriver::SoundPlayer::allowRefresh(bool allow)
{
    if(!d->initialized) return;

    if(allow)
    {
        d->resumeRefresh();
    }
    else
    {
        d->pauseRefresh();
    }
}

void SdlMixerDriver::SoundPlayer::listener(dint, dfloat)
{
    // Not supported.
}

void SdlMixerDriver::SoundPlayer::listenerv(dint, dfloat *)
{
    // Not supported.
}

Sound *SdlMixerDriver::SoundPlayer::makeSound(bool stereoPositioning, dint bytesPer, dint rate)
{
    if(!d->initialized) return nullptr;
    std::unique_ptr<Sound> sound(new SdlMixerDriver::Sound(stereoPositioning, bytesPer, rate));
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
            throw Error("SdlMixerDriver::SoundPlayer::makeSound", "Failed starting the refresh thread");
        }
    }
    return d->sounds.last();
}

/**
 * @note Loading must be done prior to setting properties, because the driver might defer
 * creation of the actual data buffer.
 */
DENG2_PIMPL_NOREF(SdlMixerDriver::Sound)
, DENG2_OBSERVES(System, FrameEnds)
{
    dint flags = 0;                   ///< SFXCF_* flags.
    dfloat frequency = 0;             ///< Frequency adjustment: 1.0 is normal.
    dfloat volume = 0;                ///< Sound volume: 1.0 is max.

    SoundEmitter *emitter = nullptr;  ///< Emitter for the sound, if any (not owned).
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
        // Ensure to stop playback and release the channel, if we haven't already.
        stop(buffer);
        releaseChannel(dint( buffer.cursor ));

        // Cancel frame notifications.
        System::get().audienceForFrameEnds() -= this;
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
        DENG2_ASSERT(valid);

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
            if(emitter && emitter == (ddmobj_base_t *)System::get().worldStageListenerPtr())
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
            if(emitter && emitter != (ddmobj_base_t *)System::get().worldStageListenerPtr() &&
               Thinker_IsMobjFunc(emitter->thinker.function))
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
               (emitter && emitter == (ddmobj_base_t *)System::get().worldStageListenerPtr()))
            {
                dist = 1;
                finalPan = 0;
            }
            else
            {
                // Calculate roll-off attenuation. [.125/(.125+x), x=0..1]
                Ranged const attenRange = System::get().worldStageSoundVolumeAttenuationRange();

                dist = System::get().distanceToWorldStageListener(origin);

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
                if(mobj_t *listener = System::get().worldStageListenerPtr())
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

    void load(sfxbuffer_t &buf, sfxsample_t &sample)
    {
        // Does the buffer already have a sample loaded?
        if(buf.sample)
        {
            // Is the same one?
            if(buf.sample->soundId == sample.soundId)
                return;

            // Free the existing data.
            buf.sample = nullptr;
            Mix_FreeChunk((Mix_Chunk *) buf.ptr);
        }

        dsize const size = 8 + 4 + 8 + 16 + 8 + sample.size;
        static char localBuf[0x40000];
        char *conv = nullptr;
        if(size <= sizeof(localBuf))
        {
            conv = localBuf;
        }
        else
        {
            conv = (char *) M_Malloc(size);
        }

        // Transfer the sample to SDL_mixer by converting it to WAVE format.
        qstrcpy(conv, "RIFF");
        *(Uint32 *) (conv + 4) = DD_ULONG(4 + 8 + 16 + 8 + sample.size);
        qstrcpy(conv + 8, "WAVE");

        // Format chunk.
        qstrcpy(conv + 12, "fmt ");
        *(Uint32 *) (conv + 16) = DD_ULONG(16);
        /**
         * WORD wFormatTag;         // Format category
         * WORD wChannels;          // Number of channels
         * uint dwSamplesPerSec;    // Sampling rate
         * uint dwAvgBytesPerSec;   // For buffer estimation
         * WORD wBlockAlign;        // Data block size
         * WORD wBitsPerSample;     // Sample size
         */
        *(Uint16 *) (conv + 20) = DD_USHORT(1);
        *(Uint16 *) (conv + 22) = DD_USHORT(1);
        *(Uint32 *) (conv + 24) = DD_ULONG(sample.rate);
        *(Uint32 *) (conv + 28) = DD_ULONG(sample.rate * sample.bytesPer);
        *(Uint16 *) (conv + 32) = DD_USHORT(sample.bytesPer);
        *(Uint16 *) (conv + 34) = DD_USHORT(sample.bytesPer * 8);

        // Data chunk.
        qstrcpy(conv + 36, "data");
        *(Uint32 *) (conv + 40) = DD_ULONG(sample.size);
        std::memcpy(conv + 44, sample.data, sample.size);

        buf.ptr = Mix_LoadWAV_RW(SDL_RWFromMem(conv, 44 + sample.size), 1);
        if(!buf.ptr)
        {
            LOG_AS("DS_SDLMixer_SFX_Load");
            LOG_AUDIO_WARNING("Failed loading sample: %s") << Mix_GetError();
        }

        if(conv != localBuf)
        {
            M_Free(conv);
        }

        buf.sample = &sample;
    }

    void stop(sfxbuffer_t &buf)
    {
        if(!buf.sample) return;

        Mix_HaltChannel(buf.cursor);
        //usedChannels.setBit(buf->cursor, false);
        buf.flags &= ~SFXBF_PLAYING;
    }

    void reset(sfxbuffer_t &buf)
    {
        stop(buf);
        buf.sample = nullptr;

        // Unallocate the resources of the source.
        Mix_FreeChunk((Mix_Chunk *) buf.ptr);
        buf.ptr = nullptr;
    }

    void refresh(sfxbuffer_t &buf)
    {
        // Can only be done if there is a sample and the buffer is playing.
        if(!buf.sample || !isPlaying(buf))
            return;

        duint const nowTime = Timer_RealMilliseconds();

        /**
         * Have we passed the predicted end of sample?
         * @note This test fails if the game has been running for about 50 days,
         * since the millisecond counter overflows. It only affects sounds that
         * are playing while the overflow happens, though.
         */
        if(!(buf.flags & SFXBF_REPEAT) && nowTime >= buf.endTime)
        {
            // Time for the sound to stop.
            buf.flags &= ~SFXBF_PLAYING;
        }
    }

    void play(sfxbuffer_t &buf)
    {
        // Playing is quite impossible without a sample.
        if(!buf.sample) return;

        // Update the volume at which the sample will be played.
        Mix_Volume(buf.cursor, buf.written);
        Mix_PlayChannel(buf.cursor, (Mix_Chunk *) buf.ptr, (buf.flags & SFXBF_REPEAT ? -1 : 0));

        // Calculate the end time (milliseconds).
        buf.endTime = Timer_RealMilliseconds() + buf.milliseconds();

        // The buffer is now playing.
        buf.flags |= SFXBF_PLAYING;
    }

    bool isPlaying(sfxbuffer_t &buf) const
    {
        return (buf.flags & SFXBF_PLAYING) != 0;
    }

    /// @param newPan  (-1 ... +1)
    void setPan(sfxbuffer_t &buf, dfloat newPan)
    {
        auto const right = dint( (newPan + 1) * 127 );
        Mix_SetPanning(buf.cursor, 254 - right, right);
    }

    void setVolume(sfxbuffer_t &buf, dfloat newVolume)
    {
        // 'written' is used for storing the volume of the channel.
        buf.written = duint( newVolume * MIX_MAX_VOLUME );
        Mix_Volume(buf.cursor, buf.written);
    }

    // Unsupported sound buffer property manipulation:

    void setFrequency(sfxbuffer_t &, dfloat) {}
    void setOrigin(sfxbuffer_t &, Vector3d const &) {}
    void setPositioning(sfxbuffer_t &, bool) {}
    void setVelocity(sfxbuffer_t &, Vector3d const &) {}
    void setVolumeAttenuationRange(sfxbuffer_t &, Ranged const &) {}
};

SdlMixerDriver::Sound::Sound(bool stereoPositioning, dint bytesPer, dint rate)
    : d(new Instance)
{
    format(stereoPositioning, bytesPer, rate);
}

SdlMixerDriver::Sound::~Sound()
{}

bool SdlMixerDriver::Sound::isValid() const
{
    return d->valid;
}

sfxsample_t const *SdlMixerDriver::Sound::samplePtr() const
{
    return d->buffer.sample;
}

void SdlMixerDriver::Sound::format(bool stereoPositioning, dint bytesPer, dint rate)
{
    // Do we need to (re)configure the sample data buffer?
    if(d->buffer.rate != rate || d->buffer.bytes != bytesPer)
    {
        stop();
        DENG2_ASSERT(!d->isPlaying(d->buffer));

        // Release the previously acquired channel, if any.
        /// @todo Probably unnecessary given we plan to acquire again, momentarily...
        releaseChannel(dint( d->buffer.cursor ));

        de::zap(d->buffer);
        d->buffer.bytes  = bytesPer;
        d->buffer.rate   = rate;
        d->buffer.flags  = stereoPositioning ? 0 : SFXBF_3D;
        d->buffer.freq   = rate;  // Modified by calls to Set(SFXBP_FREQUENCY).

        // The cursor is used to keep track of the channel on which the sample is playing.
        d->buffer.cursor = duint( acquireChannel() );
        d->valid = true;
    }
}

dint SdlMixerDriver::Sound::flags() const
{
    return d->flags;
}

/// @todo Use QFlags -ds
void SdlMixerDriver::Sound::setFlags(dint newFlags)
{
    d->flags = newFlags;
}

bool SdlMixerDriver::Sound::stereoPositioning() const
{
    return (d->buffer.flags & SFXBF_3D) == 0;
}

dint SdlMixerDriver::Sound::bytes() const
{
    return d->buffer.bytes;
}

dint SdlMixerDriver::Sound::rate() const
{
    return d->buffer.rate;
}

SoundEmitter *SdlMixerDriver::Sound::emitter() const
{
    return d->emitter;
}

void SdlMixerDriver::Sound::setEmitter(SoundEmitter *newEmitter)
{
    d->emitter = newEmitter;
}

void SdlMixerDriver::Sound::setOrigin(Vector3d const &newOrigin)
{
    d->origin = newOrigin;
}

Vector3d SdlMixerDriver::Sound::origin() const
{
    return d->origin;
}

void SdlMixerDriver::Sound::load(sfxsample_t &sample)
{
    // Don't reload if a sample with the same sound ID is already loaded.
    if(!d->buffer.sample || d->buffer.sample->soundId != sample.soundId)
    {
        d->load(d->buffer, sample);
    }
}

void SdlMixerDriver::Sound::stop()
{
    d->stop(d->buffer);
}

void SdlMixerDriver::Sound::reset()
{
    d->reset(d->buffer);
}

void SdlMixerDriver::Sound::play(PlayingMode mode)
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
            d->setVolumeAttenuationRange(d->buffer, System::get().worldStageSoundVolumeAttenuationRange());
        }
    }

    d->play(d->buffer);
    d->startTime = Timer_Ticks();  // Note the current time.
}

dint SdlMixerDriver::Sound::startTime() const
{
    return d->startTime;
}

dint SdlMixerDriver::Sound::endTime() const
{
    return d->buffer.endTime;
}

audio::Sound &SdlMixerDriver::Sound::setFrequency(dfloat newFrequency)
{
    d->frequency = newFrequency;
    return *this;
}

audio::Sound &SdlMixerDriver::Sound::setVolume(dfloat newVolume)
{
    d->volume = newVolume;
    return *this;
}

audio::Sound::PlayingMode SdlMixerDriver::Sound::mode() const
{
    if(!d->isPlaying(d->buffer))          return NotPlaying;
    if(d->buffer.flags & SFXBF_REPEAT)    return Looping;
    if(d->buffer.flags & SFXBF_DONT_STOP) return OnceDontDelete;
    return Once;
}

dfloat SdlMixerDriver::Sound::frequency() const
{
    return d->frequency;
}

dfloat SdlMixerDriver::Sound::volume() const
{
    return d->volume;
}

void SdlMixerDriver::Sound::refresh()
{
    d->refresh(d->buffer);
}

// ----------------------------------------------------------------------------------

DENG2_PIMPL(SdlMixerDriver)
, DENG2_OBSERVES(audio::System, FrameBegins)
{
    bool initialized = false;

    MusicPlayer music;
    SoundPlayer sound;

    Instance(Public *i)
        : Base(i)
        , music()
        , sound()
    {}

    ~Instance()
    {
        // Should have been deinitialized by now.
        DENG2_ASSERT(!initialized);
    }

    void systemFrameBegins(audio::System &)
    {
        DENG2_ASSERT(initialized);
        music.update();
    }
};

SdlMixerDriver::SdlMixerDriver() : d(new Instance(this))
{}

SdlMixerDriver::~SdlMixerDriver()
{
    LOG_AS("~audio::SdlMixerDriver");
    deinitialize();  // If necessary.
}

void SdlMixerDriver::initialize()
{
    LOG_AS("audio::SdlMixerDriver");

    // Already been here?
    if(d->initialized) return;

    if(SDL_InitSubSystem(SDL_INIT_AUDIO))
    {
        LOG_AUDIO_ERROR("Error initializing SDL audio: %s") << SDL_GetError();
        return;
    }

    SDL_version compVer; SDL_MIXER_VERSION(&compVer);
    SDL_version const *linkVer = Mix_Linked_Version();
    DENG2_ASSERT(linkVer);

    if(SDL_VERSIONNUM(linkVer->major, linkVer->minor, linkVer->patch) >
       SDL_VERSIONNUM(compVer.major, compVer.minor, compVer.patch))
    {
        LOG_AUDIO_WARNING("Linked version of SDL_mixer (%u.%u.%u) is "
                          "newer than expected (%u.%u.%u)")
            << linkVer->major << linkVer->minor << linkVer->patch
            << compVer.major << compVer.minor << compVer.patch;
    }

    if(Mix_OpenAudio(MIX_DEFAULT_FREQUENCY, MIX_DEFAULT_FORMAT, 2, 1024))
    {
        LOG_AUDIO_ERROR("Failed initializing SDL_mixer: %s") << Mix_GetError();
        return;
    }

    duint16 format;
    dint freq, channels;
    Mix_QuerySpec(&freq, &format, &channels);

    // Announce capabilites.
    LOG_AUDIO_VERBOSE("SDLMixer configuration:");
    LOG_AUDIO_VERBOSE("  " _E(>) "Output: %s"
                      "\nFormat: %x (%x)"
                      "\nFrequency: %iHz (%iHz)"
                      "\nInitial Channels: %i")
            << (channels > 1? "stereo" : "mono")
            << format << (duint16) AUDIO_S16LSB
            << freq << (dint) MIX_DEFAULT_FREQUENCY
            << MIX_CHANNELS;

    // Prepare to play simultaneous sounds.
    /*numChannels =*/ Mix_AllocateChannels(MIX_CHANNELS);
    usedChannels.clear();

    // We want notification when a new audio frame begins.
    audioSystem().audienceForFrameBegins() += d;

    // Everything is OK.
    d->initialized = true;
}

void SdlMixerDriver::deinitialize()
{
    LOG_AS("audio::SdlMixerDriver");

    // Already been here?
    if(!d->initialized) return;

    d->initialized = false;

    // Stop receiving notifications:
    audioSystem().audienceForFrameBegins() -= d;

    usedChannels.clear();

    if(lastMusic)
    {
        Mix_HaltMusic();
        Mix_FreeMusic(lastMusic);
        lastMusic = nullptr;
    }

    Mix_CloseAudio();
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
}

audio::System::IDriver::Status SdlMixerDriver::status() const
{
    if(d->initialized) return Initialized;
    return Loaded;
}

String SdlMixerDriver::identityKey() const
{
    return "sdlmixer";
}

String SdlMixerDriver::title() const
{
    return "SDL_mixer";
}

QList<Record> SdlMixerDriver::listInterfaces() const
{
    QList<Record> list;
    {
        Record rec;
        rec.addNumber("type",        System::AUDIO_IMUSIC);
        rec.addText  ("identityKey", DotPath(identityKey()) / "music");
        list << rec;
    }
    {
        Record rec;
        rec.addNumber("type",        System::AUDIO_ISFX);
        rec.addText  ("identityKey", DotPath(identityKey()) / "sfx");
        list << rec;
    }
    return list;
}

IPlayer &SdlMixerDriver::findPlayer(String interfaceIdentityKey) const
{
    if(IPlayer *found = tryFindPlayer(interfaceIdentityKey)) return *found;
    /// @throw UnknownInterfaceError  Unknown interface referenced.
    throw UnknownInterfaceError("SdlMixerDriver::findPlayer", "Unknown playback interface \"" + interfaceIdentityKey + "\"");
}

IPlayer *SdlMixerDriver::tryFindPlayer(String interfaceIdentityKey) const
{
    interfaceIdentityKey = interfaceIdentityKey.toLower();

    if(interfaceIdentityKey == "music") return &d->music;
    if(interfaceIdentityKey == "sfx")   return &d->sound;

    return nullptr;  // Not found.
}

}  // namespace audio

#endif  // !DENG_DISABLE_SDLMIXER
