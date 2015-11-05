/** @file sdlmixerdriver.cpp  Audio driver for playback using SDL_mixer.
 *
 * @authors Copyright © 2003-2015 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2015 Daniel Swanson <danij@dengine.net>
 *
 * @par License
 * LGPL: http://www.gnu.org/licenses/lgpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser
 * General Public License for more details. You should have received a copy of
 * the GNU Lesser General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#ifndef DENG_DISABLE_SDLMIXER

#include "audio/drivers/sdlmixerdriver.h"

#include "audio/listener.h"
#include "audio/samplecache.h"
#include "audio/sound.h"

#include "world/thinkers.h"  // Thinker_IsMobjFunc()
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

// --------------------------------------------------------------------------------------

SdlMixerDriver::MusicChannel::MusicChannel() : audio::MusicChannel()
{}

PlayingMode SdlMixerDriver::MusicChannel::mode() const
{
    if(!Mix_PlayingMusic())
        return NotPlaying;

    return _mode;
}

void SdlMixerDriver::MusicChannel::play(PlayingMode mode)
{
    if(isPlaying()) return;
    if(mode == NotPlaying) return;

    if(lastMusic)
    {
        if(Mix_PlayMusic(lastMusic, mode == Looping ? -1 : 1))
        {
            _mode = mode;
            return;
        }
        throw Error("SdlMixerDriver::MusicChannel::play", "Failed to play source \"" + _sourcePath + "\"");
    }
    throw Error("SdlMixerDriver::MusicChannel::play", "No source is bound");
}

void SdlMixerDriver::MusicChannel::stop()
{
    if(!isPlaying()) return;
    Mix_HaltMusic();
}

bool SdlMixerDriver::MusicChannel::isPaused() const
{
    if(!isPlaying()) return false;
    return CPP_BOOL( Mix_PausedMusic() );
}

void SdlMixerDriver::MusicChannel::pause()
{
    if(!isPlaying()) return;
    Mix_PauseMusic();
}

void SdlMixerDriver::MusicChannel::resume()
{
    if(!isPlaying()) return;
    Mix_ResumeMusic();
}

Channel &SdlMixerDriver::MusicChannel::setFrequency(de::dfloat newFrequency)
{
    // Not supported.
    return *this;
}

Channel &SdlMixerDriver::MusicChannel::setVolume(dfloat newVolume)
{
    Mix_VolumeMusic(dint( MIX_MAX_VOLUME * newVolume ));
    return *this;
}

dfloat SdlMixerDriver::MusicChannel::frequency() const
{
    return 1; // Always.
}

Positioning SdlMixerDriver::MusicChannel::positioning() const
{
    return StereoPositioning;  // Always.
}

dfloat SdlMixerDriver::MusicChannel::volume() const
{
    return de::clamp(0, Mix_VolumeMusic(-1), MIX_MAX_VOLUME) / dfloat( MIX_MAX_VOLUME );
}

bool SdlMixerDriver::MusicChannel::canPlayBuffer() const
{
    return false;
}

void *SdlMixerDriver::MusicChannel::songBuffer(duint)
{
    stop();
    _sourcePath.clear();
    return nullptr;
}

bool SdlMixerDriver::MusicChannel::canPlayFile() const
{
    return true;
}

void SdlMixerDriver::MusicChannel::bindFile(String const &sourcePath)
{
    if(_sourcePath == sourcePath) return;

    stop();

    _sourcePath = sourcePath;
    // Free any previously loaded music.
    if(lastMusic)
    {
        Mix_HaltMusic();
        Mix_FreeMusic(lastMusic);
    }

    lastMusic = Mix_LoadMUS(_sourcePath.toUtf8().constData());
    if(!lastMusic)
    {
        LOG_AS("SdlMixerDriver::MusicChannel");
        LOG_AUDIO_ERROR("Failed binding file:\n") << Mix_GetError();
    }
}

// --------------------------------------------------------------------------------------

/**
 * @note Loading must be done prior to setting properties, because the driver might defer
 * creation of the actual data buffer.
 */
DENG2_PIMPL_NOREF(SdlMixerDriver::SoundChannel)
, DENG2_OBSERVES(System, FrameEnds)
{
    bool noUpdate  = false;           ///< @c true if skipping updates (when stopped, before deletion).
    dint startTime = 0;               ///< When the assigned sound sample was last started (Ticks).

    dfloat frequency = 1;             ///< Frequency adjustment: 1.0 is normal.
    dfloat volume    = 1;             ///< Sound volume: 1.0 is max.

    ::audio::Sound *sound = nullptr;    ///< Logical Sound currently being played (if any, not owned).
    Listener *listener = nullptr;  ///< Listener for the sound, if any (not owned).

    sfxbuffer_t buffer;
    bool valid = false;               ///< Set to @c true when in the valid state.

    Instance()
    {
        de::zap(buffer);
        // Lazy acquisition of SDLmixer channels (-1= not a valid channel).
        buffer.cursor = -1;

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

    inline Listener &getListener() const
    {
        DENG2_ASSERT(listener != nullptr);
        return *listener;
    }

    inline ::audio::Sound &getSound() const
    {
        DENG2_ASSERT(sound != nullptr);
        return *sound;
    }

    /**
     * Determines whether the channel is configured such that the soundstage Listener
     * *is* the SoundEmitter.
     */
    bool listenerIsSoundEmitter() const
    {
        return listener
               && getSound().emitter()
               && getSound().emitter() == (ddmobj_base_t const *)getListener().trackedMapObject();
    }

    /**
     * Determines whether the channel is configured to play an "originless" sound.
     */
    bool noOrigin() const
    {
        return getSound().flags().testFlag(SoundFlag::NoOrigin)
               || listenerIsSoundEmitter();
    }

    /**
     * Writes deferred Listener and/or Environment changes to the audio driver.
     *
     * @param force  Usually updates are only necessary during playback. Use @c true to
     * override this check and write the changes regardless.
     */
    void writeDeferredProperties(bool force = false)
    {
        DENG2_ASSERT(valid);

        // Disabled?
        if(noUpdate) return;

        // Updates are only necessary during playback.
        if(!isPlaying(buffer) && !force) return;

        // When tracking an emitter we need the latest origin coordinates.
        getSound().updateOriginFromEmitter();

        // Use Absolute/Relative-Positioning (in 3D)?
        if(buffer.flags & SFXBF_3D)
        {
            // Volume is affected only by maxvol.
            setVolume(buffer, volume * System::get().soundVolume() / 255.0f);
        }
        // Use StereoPositioning.
        else
        {
            dfloat volAtten = 1;  // No attenuation.
            dfloat panning  = 0;  // No panning.

            if(listener && !noOrigin())
            {
                // Apply listener distance based volume attenuation?
                if(!getSound().flags().testFlag(SoundFlag::NoVolumeAttenuation))
                {
                    ddouble const dist      = getListener().distanceFrom(getSound().origin());
                    Ranged const attenRange = getListener().volumeAttenuationRange();

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

                // Apply listener angle based source panning?
                if(mobj_t const *tracking = getListener().trackedMapObject())
                {
                    dfloat angle = getListener().angleFrom(getSound().origin());
                    // We want a signed angle.
                    if(angle > 180) angle -= 360;

                    if(angle <= 90 && angle >= -90)  // Front half.
                    {
                        panning = -angle / 90;
                    }
                    else  // Back half.
                    {
                        panning = (angle + (angle > 0 ? -180 : 180)) / 90;
 
                        // Dampen sounds coming from behind the listener.
                        volAtten *= (1 + (panning > 0 ? panning : -panning)) / 2;
                    }
                }
            }

            setVolume(buffer, volume * volAtten * System::get().soundVolume() / 255.0f);
            setPan(buffer, panning);
        }
    }

    void systemFrameEnds(System &)
    {
        writeDeferredProperties();
    }

    void load(sfxbuffer_t &buf, sfxsample_t *sample)
    {
        DENG2_ASSERT(sample != nullptr);

        // Does the buffer already have a sample loaded?
        if(buf.sample)
        {
            // Is the same one?
            if(buf.sample->effectId == sample->effectId)
                return;

            // Free the existing data.
            buf.sample = nullptr;
            Mix_FreeChunk((Mix_Chunk *) buf.ptr);
        }

        dsize const size = 8 + 4 + 8 + 16 + 8 + sample->size;
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
        *(Uint32 *) (conv + 4) = DD_ULONG(4 + 8 + 16 + 8 + sample->size);
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
        *(Uint32 *) (conv + 24) = DD_ULONG(sample->rate);
        *(Uint32 *) (conv + 28) = DD_ULONG(sample->rate * sample->bytesPer);
        *(Uint16 *) (conv + 32) = DD_USHORT(sample->bytesPer);
        *(Uint16 *) (conv + 34) = DD_USHORT(sample->bytesPer * 8);

        // Data chunk.
        qstrcpy(conv + 36, "data");
        *(Uint32 *) (conv + 40) = DD_ULONG(sample->size);
        std::memcpy(conv + 44, sample->data, sample->size);

        buf.ptr = Mix_LoadWAV_RW(SDL_RWFromMem(conv, 44 + sample->size), 1);
        if(!buf.ptr)
        {
            LOG_AS("DS_SDLMixer_SFX_Load");
            LOG_AUDIO_WARNING("Failed loading sample: %s") << Mix_GetError();
        }

        if(conv != localBuf)
        {
            M_Free(conv);
        }

        buf.sample = sample;
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
};

SdlMixerDriver::SoundChannel::SoundChannel()
    : audio::SoundChannel()
    , d(new Instance)
{
    format(StereoPositioning, 1, 11025);
}

PlayingMode SdlMixerDriver::SoundChannel::mode() const
{
    if(!d->isPlaying(d->buffer))          return NotPlaying;
    if(d->buffer.flags & SFXBF_REPEAT)    return Looping;
    if(d->buffer.flags & SFXBF_DONT_STOP) return OnceDontDelete;
    return Once;
}

void SdlMixerDriver::SoundChannel::play(PlayingMode mode)
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

    // Updating the channel should resume (presumably).
    d->noUpdate = false;

    // When playing on a sound stage with a Listener, we may need to update the channel
    // dynamically during playback.
    d->listener = &System::get().worldStage().listener();

    // Flush deferred property value changes to the assigned data buffer.
    d->writeDeferredProperties(true/*force*/);

#if 0
    // 3D sounds need a few extra properties set up.
    if(d->buffer.flags & SFXBF_3D)
    {
        // Configure the attentuation distances.
        // This is only done once, when the sound is first played (i.e., here).
        if(d->getSound().flags().testFlag(SoundFlag::NoVolumeAttenuation))
        {
            d->setVolumeAttenuationRange(d->buffer, Ranged(10000, 20000));
        }
        else
        {
            d->setVolumeAttenuationRange(d->buffer, d->getListener().volumeAttenuationRange());
        }
    }
#endif

    d->play(d->buffer);
    d->startTime = Timer_Ticks();  // Note the current time.
}

void SdlMixerDriver::SoundChannel::stop()
{
    d->stop(d->buffer);
}

bool SdlMixerDriver::SoundChannel::isPaused() const
{
    if(!isPlaying()) return false;
    return CPP_BOOL( Mix_Paused(d->buffer.cursor) );
}

void SdlMixerDriver::SoundChannel::pause()
{
    if(!isPlaying()) return;
    Mix_Pause(d->buffer.cursor);
}

void SdlMixerDriver::SoundChannel::resume()
{
    if(!isPlaying()) return;
    Mix_Resume(d->buffer.cursor);
}

void SdlMixerDriver::SoundChannel::suspend()
{
    if(!isPlaying()) return;
    d->noUpdate = true;
}

Channel &SdlMixerDriver::SoundChannel::setFrequency(dfloat newFrequency)
{
    d->frequency = newFrequency;  // Deferred until refresh.
    return *this;
}

Channel &SdlMixerDriver::SoundChannel::setVolume(dfloat newVolume)
{
    d->volume = newVolume;  // Deferred until refresh.
    return *this;
}

dfloat SdlMixerDriver::SoundChannel::frequency() const
{
    return d->frequency;
}

Positioning SdlMixerDriver::SoundChannel::positioning() const
{
    return (d->buffer.flags & SFXBF_3D) ? AbsolutePositioning : StereoPositioning;
}

dfloat SdlMixerDriver::SoundChannel::volume() const
{
    return d->volume;
}

void SdlMixerDriver::SoundChannel::update()
{
    d->refresh(d->buffer);
}

void SdlMixerDriver::SoundChannel::reset()
{
    d->reset(d->buffer);
}

::audio::Sound *SdlMixerDriver::SoundChannel::sound() const
{
    return isPlaying() ? &d->getSound() : nullptr;
}

void SdlMixerDriver::SoundChannel::load(sfxsample_t const &sample)
{
    // Don't reload if a sample with the same sound ID is already loaded.
    if(!d->buffer.sample || d->buffer.sample->effectId != sample.effectId)
    {
        d->load(d->buffer, &const_cast<sfxsample_t &>(sample));
    }
}

bool SdlMixerDriver::SoundChannel::format(Positioning positioning, dint bytesPer, dint rate)
{
    // Do we need to (re)configure the sample data buffer?
    if(this->positioning() != positioning
       || d->buffer.rate   != rate
       || d->buffer.bytes  != bytesPer)
    {
        stop();
        DENG2_ASSERT(!isPlaying());

        // Release the previously acquired channel, if any.
        /// @todo Probably unnecessary given we plan to acquire again, momentarily...
        releaseChannel(dint( d->buffer.cursor ));

        de::zap(d->buffer);
        d->buffer.bytes  = bytesPer;
        d->buffer.rate   = rate;
        d->buffer.flags  = positioning == AbsolutePositioning ? SFXBF_3D : 0;
        d->buffer.freq   = rate;  // Modified by calls to Set(SFXBP_FREQUENCY).

        // The cursor is used to keep track of the channel on which the sample is playing.
        d->buffer.cursor = duint( acquireChannel() );
        d->valid = true;
    }
    return isValid();
}

bool SdlMixerDriver::SoundChannel::isValid() const
{
    return d->valid;
}

dint SdlMixerDriver::SoundChannel::bytes() const
{
    return d->buffer.bytes;
}

dint SdlMixerDriver::SoundChannel::rate() const
{
    return d->buffer.rate;
}

dint SdlMixerDriver::SoundChannel::startTime() const
{
    return d->startTime;
}

duint SdlMixerDriver::SoundChannel::endTime() const
{
    return d->buffer.endTime;
}

void SdlMixerDriver::SoundChannel::updateEnvironment()
{
    // Not supported.
}

bool SdlMixerDriver::SoundChannel::anyRateAccepted() const
{
    // No - please upsample for us.
    return false;
}

// --------------------------------------------------------------------------------------

DENG2_PIMPL(SdlMixerDriver), public IChannelFactory
{
    bool initialized = false;

    struct MusicPlayer
    {
        bool needInit    = true;
        bool initialized = false;

        void initialize()
        {
            // Already been here?
            if(!needInit) return;

            needInit = false;
#ifdef DENG2_DEBUG
            Mix_HookMusicFinished(musicPlaybackFinished);
#endif
            initialized = true;
        }

        void deinitialize()
        {
            // Already been here?
            if(!initialized) return;

#ifdef DENG2_DEBUG
            Mix_HookMusicFinished(nullptr);
#endif
            initialized = false;
            needInit    = true;
        }
    };

    struct SoundPlayer : DENG2_OBSERVES(SampleCache, SampleRemove)
    {
        SdlMixerDriver &driver;
        bool needInit    = true;
        bool initialized = false;

        thread_t refreshThread      = nullptr;
        volatile bool refreshPaused = false;
        volatile bool refreshing    = false;

        SoundPlayer(SdlMixerDriver &driver) : driver(driver) {}
        ~SoundPlayer() { DENG2_ASSERT(!initialized); }

        void initialize()
        {
            // Already been here?
            if(!needInit) return;

            needInit = false;
            audio::System::get().sampleCache().audienceForSampleRemove() += this;
            initialized = true;
        }

        void deinitialize()
        {
            // Already been here?
            if(!initialized) return;

            // Cancel sample cache removal notification - we intend to clear sounds.
            audio::System::get().sampleCache().audienceForSampleRemove() -= this;

            // Stop any sounds still playing (note: does not affect refresh).
            for(Channel *channel : driver.d->channels[Channel::Sound])
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

            qDeleteAll(driver.d->channels[Channel::Sound]);
            driver.d->channels[Channel::Sound].clear();

            needInit = true;
        }

        /**
         * This is a high-priority thread that periodically checks if the channels need to be
         * updated with more data. The thread terminates when it notices that the sound player
         * is deinitialized.
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
                    for(Channel *channel : inst.driver.d->channels[Channel::Sound])
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
            for(Channel *base : driver.d->channels[Channel::Sound])
            {
                auto &ch = base->as<SoundChannel>();

                if(!ch.isValid()) continue;

                if(ch.sound() && ch.sound()->effectId() == sample.effectId)
                {
                    // Stop and unload.
                    ch.reset();
                }
            }
            resumeRefresh();
        }
    };

    MusicPlayer music;
    SoundPlayer sound;

    struct ChannelSet : public QList<Channel *>
    {
        ~ChannelSet() { DENG2_ASSERT(isEmpty()); }
    } channels[Channel::TypeCount];

    Instance(Public *i) : Base(i), sound(self) {}

    ~Instance() { DENG2_ASSERT(!initialized); }

    void clearChannels()
    {
        for(ChannelSet &set : channels)
        {
            qDeleteAll(set);
            set.clear();
        }
    }

    Channel *makeChannel(Channel::Type type) override
    {
        if(self.isInitialized())
        {
            switch(type)
            {
            case Channel::Music:
                if(music.initialized)
                {
                    std::unique_ptr<Channel> channel(new MusicChannel);
                    channels[type] << channel.get();
                    return channel.release();
                }
                break;

            case Channel::Sound:
                if(sound.initialized)
                {
                    std::unique_ptr<Channel> channel(new SoundChannel);
                    channels[type] << channel.get();
                    if(channels[type].count() == 1)
                    {
                        // Start the sound refresh thread. It will stop on its own when it notices that
                        // the player is deinitialized.
                        sound.refreshing    = false;
                        sound.refreshPaused = false;

                        // Start the refresh thread.
                        sound.refreshThread = Sys_StartThread(SoundPlayer::RefreshThread, &sound);
                        if(!sound.refreshThread)
                        {
                            throw Error("SdlMixerDriver::makeChannel", "Failed starting the refresh thread");
                        }
                    }
                    return channel.release();
                }
                break;

            default: break;
            }
        }
        return nullptr;
    }
};

SdlMixerDriver::SdlMixerDriver() : d(new Instance(this))
{}

SdlMixerDriver::~SdlMixerDriver()
{
    deinitialize();  // If necessary.
}

void SdlMixerDriver::initialize()
{
    LOG_AS("SdlMixerDriver");

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
                      "\nInitial channels: %i")
            << (channels > 1? "stereo" : "mono")
            << format << (duint16) AUDIO_S16LSB
            << freq << (dint) MIX_DEFAULT_FREQUENCY
            << MIX_CHANNELS;

    // Prepare to play simultaneous sounds.
    /*numChannels =*/ Mix_AllocateChannels(MIX_CHANNELS);
    usedChannels.clear();

    // Everything is OK.
    d->initialized = true;
}

void SdlMixerDriver::deinitialize()
{
    LOG_AS("SdlMixerDriver");

    // Already been here?
    if(!d->initialized) return;

    d->initialized = false;

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

IDriver::Status SdlMixerDriver::status() const
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

void SdlMixerDriver::initInterface(String const &identityKey)
{
    String const idKey = identityKey.toLower();
    for(Record const &def : listInterfaces())
    {
        if(def.gets("identityKey") != idKey) continue;

        switch(def.geti("channelType"))
        {
        case Channel::Music: d->music.initialize(); return;
        case Channel::Sound: d->sound.initialize(); return;

        default: return;
        }
    }
}

void SdlMixerDriver::deinitInterface(String const &identityKey)
{
    String const idKey = identityKey.toLower();
    for(Record const &def : listInterfaces())
    {
        if(def.gets("identityKey") != idKey) continue;

        switch(def.geti("channelType"))
        {
        case Channel::Music: d->music.deinitialize(); return;
        case Channel::Sound: d->sound.deinitialize(); return;

        default: return;
        }
    }
}

QList<Record> SdlMixerDriver::listInterfaces() const
{
    QList<Record> list;
    {
        Record rec;
        rec.addText  ("identityKey", DotPath(identityKey()) / "music");
        rec.addNumber("channelType", Channel::Music);
        list << rec;
    }
    {
        Record rec;
        rec.addText  ("identityKey", DotPath(identityKey()) / "sfx");
        rec.addNumber("channelType", Channel::Sound);
        list << rec;
    }
    return list;
}

void SdlMixerDriver::allowRefresh(bool allow)
{
    if(!isInitialized()) return;

    if(d->sound.initialized)
    {
        if(allow) d->sound.resumeRefresh();
        else      d->sound.pauseRefresh();
    }
}

IChannelFactory &SdlMixerDriver::channelFactory() const
{
    return *d;
}

LoopResult SdlMixerDriver::forAllChannels(Channel::Type type,
    std::function<LoopResult (Channel const &)> callback) const
{
    for(Channel const *ch : d->channels[type])
    {
        if(auto result = callback(*ch))
            return result;
    }
    return LoopContinue;
}

}  // namespace audio

#endif  // !DENG_DISABLE_SDLMIXER
