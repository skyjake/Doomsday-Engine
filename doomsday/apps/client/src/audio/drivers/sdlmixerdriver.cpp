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

#include "audio/sound.h"
#include "world/thinkers.h"
#include "def_main.h"        // SF_* flags, remove me
#include <de/Log>
#include <de/Observers>
#include <de/memoryzone.h>
#include <de/timer.h>        // TICSPERSEC
#include <SDL.h>
#include <SDL_mixer.h>
#include <cstdlib>
#include <cstring>

using namespace de;

#define DEFAULT_MIDI_COMMAND    "" //"timidity"

namespace audio {

static dint numChannels;
static bool *usedChannels;

static Mix_Music *lastMusic;

static dint getFreeChannel()
{
    for(dint i = 0; i < numChannels; ++i)
    {
        if(!usedChannels[i])
            return i;
    }
    return -1;
}

static void deleteBuffer(sfxbuffer_t &buf)
{
    Mix_HaltChannel(buf.cursor);
    usedChannels[buf.cursor] = false;
    Z_Free(&buf);
}

static sfxbuffer_t *newBuffer(dint flags, dint bits, dint rate)
{
    /// @todo fixme: We have ownership - ensure the buffer is destroyed when
    /// SdlMixerDriver::Sound is. -ds
    auto *buf = (sfxbuffer_t *) Z_Calloc(sizeof(sfxbuffer_t), PU_APPSTATIC, 0);

    buf->bytes = bits / 8;
    buf->rate  = rate;
    buf->flags = flags;
    buf->freq  = rate;  // Modified by calls to Set(SFXBP_FREQUENCY).

    // The cursor is used to keep track of the channel on which the sample
    // is playing.
    buf->cursor = getFreeChannel();
    if((dint)buf->cursor < 0)
    {
        buf->cursor = numChannels++;
        usedChannels = (bool *) M_Realloc(usedChannels, sizeof(usedChannels[0]) * numChannels);

        // Make sure we have enough channels allocated.
        Mix_AllocateChannels(numChannels);

        Mix_UnregisterAllEffects(buf->cursor);
    }

    usedChannels[buf->cursor] = true;

    return buf;
}

/**
 * Returns the length of the buffer in milliseconds.
 */
static duint getBufferLength(sfxbuffer_t const &buf)
{
    DENG2_ASSERT(buf.sample);
    return 1000 * buf.sample->numSamples / buf.freq;
}

// ----------------------------------------------------------------------------------

SdlMixerDriver::CdPlayer::CdPlayer(SdlMixerDriver &driver) : ICdPlayer(driver)
{}

String SdlMixerDriver::CdPlayer::name() const
{
    return "cd";
}

dint SdlMixerDriver::CdPlayer::init()
{
    return _initialized = true;
}

void SdlMixerDriver::CdPlayer::shutdown()
{
    _initialized = false;
}

void SdlMixerDriver::CdPlayer::update()
{}

void SdlMixerDriver::CdPlayer::setVolume(dfloat)
{}

bool SdlMixerDriver::CdPlayer::isPlaying() const
{
    return false;
}

void SdlMixerDriver::CdPlayer::pause(dint)
{}

void SdlMixerDriver::CdPlayer::stop()
{}

dint SdlMixerDriver::CdPlayer::play(dint, dint)
{
    return true;
}

// ----------------------------------------------------------------------------------

/**
 * This is the hook we ask SDL_mixer to call when music playback finishes.
 */
#ifdef DENG2_DEBUG
static void musicPlaybackFinished()
{
    LOG_AUDIO_VERBOSE("[SDLMixer] Music playback finished");
}
#endif

SdlMixerDriver::MusicPlayer::MusicPlayer(SdlMixerDriver &driver) : IMusicPlayer(driver)
{}

String SdlMixerDriver::MusicPlayer::name() const
{
    return "music";
}

dint SdlMixerDriver::MusicPlayer::init()
{
#ifdef DENG2_DEBUG
    Mix_HookMusicFinished(musicPlaybackFinished);
#endif

    return _initialized = true;
}

void SdlMixerDriver::MusicPlayer::shutdown()
{
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
    return true;
}

dint SdlMixerDriver::MusicPlayer::playFile(char const *filename, dint looped)
{
    if(!_initialized) return false;

    // Free any previously loaded music.
    if(lastMusic)
    {
        Mix_HaltMusic();
        Mix_FreeMusic(lastMusic);
    }

    lastMusic = Mix_LoadMUS(filename);
    if(!lastMusic)
    {
        LOG_AS("SdlMixerDriver::MusicPlayer");
        LOG_AUDIO_ERROR("Failed to load music: %s") << Mix_GetError();
        return false;
    }

    return !Mix_PlayMusic(lastMusic, looped ? -1 : 1);
}

// ----------------------------------------------------------------------------------

SdlMixerDriver::SoundPlayer::SoundPlayer(SdlMixerDriver &driver) : ISoundPlayer(driver)
{}

String SdlMixerDriver::SoundPlayer::name() const
{
    return "sfx";
}

dint SdlMixerDriver::SoundPlayer::init()
{
    return _initialized = true;
}

bool SdlMixerDriver::SoundPlayer::anyRateAccepted() const
{
    // No - please upsample for us.
    return false;
}

bool SdlMixerDriver::SoundPlayer::needsRefresh() const
{
    return true;
}

void SdlMixerDriver::SoundPlayer::listener(dint, dfloat)
{
    // Not supported.
}

void SdlMixerDriver::SoundPlayer::listenerv(dint, dfloat *)
{
    // Not supported.
}

Sound *SdlMixerDriver::SoundPlayer::makeSound(bool stereoPositioning, dint bitsPer, dint rate)
{
    std::unique_ptr<Sound> sound(new SdlMixerDriver::Sound);
    sound->setBuffer(newBuffer(stereoPositioning ? 0 : SFXBF_3D, bitsPer, rate));
    return sound.release();
}

/**
 * @note Loading must be done prior to setting properties, because the driver might defer
 * creation of the actual data buffer.
 */
DENG2_PIMPL_NOREF(SdlMixerDriver::Sound)
, DENG2_OBSERVES(System, FrameEnds)
{
    dint flags = 0;                  ///< SFXCF_* flags.
    dfloat frequency = 0;            ///< Frequency adjustment: 1.0 is normal.
    dfloat volume = 0;               ///< Sound volume: 1.0 is max.

    mobj_t *emitter = nullptr;       ///< Mobj emitter for the sound, if any (not owned).
    Vector3d origin;                 ///< Emit from here (synced with emitter).

    sfxbuffer_t *buffer = nullptr;   ///< Assigned sound buffer, if any (not owned).
    dint startTime = 0;              ///< When the assigned sound sample was last started.
    
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
        //usedChannels[buf->cursor] = false;
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
        buf.endTime = Timer_RealMilliseconds() + getBufferLength(buf);

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

SdlMixerDriver::Sound::Sound() : d(new Instance)
{}

SdlMixerDriver::Sound::~Sound()
{
    releaseBuffer();
}

bool SdlMixerDriver::Sound::hasBuffer() const
{
    return d->buffer != nullptr;
}

sfxbuffer_t const &SdlMixerDriver::Sound::buffer() const
{
    if(d->buffer) return *d->buffer;
    /// @throw MissingBufferError  No sound buffer is currently assigned.
    throw MissingBufferError("audio::SdlMixerDriver::Sound::buffer", "No data buffer is assigned");
}

void SdlMixerDriver::Sound::releaseBuffer()
{
    stop();
    if(!hasBuffer()) return;

    // Cancel frame notifications - we'll soon have no buffer to update.
    System::get().audienceForFrameEnds() -= d;

    deleteBuffer(*d->buffer);
    d->buffer = nullptr;
}

void SdlMixerDriver::Sound::setBuffer(sfxbuffer_t *newBuffer)
{
    releaseBuffer();
    d->buffer = newBuffer;

    if(d->buffer)
    {
        // We want notification when the frame ends in order to flush deferred property writes.
        System::get().audienceForFrameEnds() += d;
    }
}

void SdlMixerDriver::Sound::format(bool stereoPositioning, dint bytesPer, dint rate)
{
    // Do we need to (re)create the sound data buffer?
    if(   !d->buffer
       || (d->buffer->rate != rate || d->buffer->bytes != bytesPer))
    {
        setBuffer(newBuffer(stereoPositioning ? 0 : SFXBF_3D, bytesPer, rate));
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

mobj_t *SdlMixerDriver::Sound::emitter() const
{
    return d->emitter;
}

void SdlMixerDriver::Sound::setEmitter(mobj_t *newEmitter)
{
    d->emitter = newEmitter;
}

void SdlMixerDriver::Sound::setFixedOrigin(Vector3d const &newOrigin)
{
    d->origin = newOrigin;
}

dfloat SdlMixerDriver::Sound::priority() const
{
    if(!isPlaying())
        return SFX_LOWEST_PRIORITY;

    if(d->flags & SFXCF_NO_ORIGIN)
        return System::get().rateSoundPriority(0, 0, d->volume, d->startTime);

    // d->origin is set to emitter->xyz during updates.
    ddouble origin[3]; d->origin.decompose(origin);
    return System::get().rateSoundPriority(0, origin, d->volume, d->startTime);
}

void SdlMixerDriver::Sound::load(sfxsample_t &sample)
{
    if(!d->buffer)
    {
        /// @throw MissingBufferError  Loading is impossible without a buffer...
        throw MissingBufferError("audio::SdlMixerDriver::Sound::load", "Attempted with no data buffer assigned");
    }

    // Don't reload if a sample with the same sound ID is already loaded.
    if(!d->buffer->sample || d->buffer->sample->soundId != sample.soundId)
    {
        d->load(*d->buffer, sample);
    }
}

void SdlMixerDriver::Sound::stop()
{
    d->stop(*d->buffer);
}

void SdlMixerDriver::Sound::reset()
{
    d->reset(*d->buffer);
}

void SdlMixerDriver::Sound::play()
{
    if(!d->buffer)
    {
        /// @throw MissingBufferError  Playing is obviously impossible without data to play...
        throw MissingBufferError("audio::SdlMixerDriver::Sound::play", "Attempted with no data buffer assigned");
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

void SdlMixerDriver::Sound::setPlayingMode(dint sfFlags)
{
    if(d->buffer)
    {
        d->buffer->flags &= ~(SFXBF_REPEAT | SFXBF_DONT_STOP);
        if(sfFlags & SF_REPEAT)    d->buffer->flags |= SFXBF_REPEAT;
        if(sfFlags & SF_DONT_STOP) d->buffer->flags |= SFXBF_DONT_STOP;
    }
}

dint SdlMixerDriver::Sound::startTime() const
{
    return d->startTime;
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

bool SdlMixerDriver::Sound::isPlaying() const
{
    return d->buffer && d->isPlaying(*d->buffer);
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
    d->refresh(*d->buffer);
}

// ----------------------------------------------------------------------------------

DENG2_PIMPL(SdlMixerDriver)
, DENG2_OBSERVES(audio::System, FrameBegins)
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

    void systemFrameBegins(audio::System &)
    {
        DENG2_ASSERT(initialized);
        iMusic.update();
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
    usedChannels = nullptr;

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

    M_Free(usedChannels); usedChannels = nullptr;

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

dint SdlMixerDriver::playerCount() const
{
    return d->initialized ? 3 : 0;
}

SdlMixerDriver::IPlayer const *SdlMixerDriver::tryFindPlayer(String name) const
{
    if(!name.isEmpty() && d->initialized)
    {
        name = name.lower();
        if(d->iCd   .name() == name) return &d->iCd;
        if(d->iMusic.name() == name) return &d->iMusic;
        if(d->iSfx  .name() == name) return &d->iSfx;
    }
    return nullptr;  // Not found.
}

SdlMixerDriver::IPlayer const &SdlMixerDriver::findPlayer(String name) const
{
    if(auto *player = tryFindPlayer(name)) return *player;
    /// @throw MissingPlayerError  Unknown identity key specified.
    throw MissingPlayerError("SdlMixerDriver::findPlayer", "Unknown player \"" + name + "\"");
}

LoopResult SdlMixerDriver::forAllPlayers(std::function<LoopResult (IPlayer &)> callback) const
{
    if(d->initialized)
    {
        if(auto result = callback(d->iCd))    return result;
        if(auto result = callback(d->iMusic)) return result;
        if(auto result = callback(d->iSfx))   return result;
    }
    return LoopContinue;  // Continue iteration.
}

}  // namespace audio

#endif  // !DENG_DISABLE_SDLMIXER
