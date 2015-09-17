/** @file sdlmixerdriver.cpp  Audio driver for playback using SDL_mixer.
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

#ifndef DENG_DISABLE_SDLMIXER

#include "audio/drivers/sdlmixerdriver.h"

#include "audio/sound.h"
#include <de/Log>
#include <de/memoryzone.h>
#include <de/timer.h>
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

/**
 * Returns the length of the buffer in milliseconds.
 */
static duint getBufferLength(sfxbuffer_t const &buf)
{
    DENG2_ASSERT(buf.sample);
    return 1000 * buf.sample->numSamples / buf.freq;
}

static dint getFreeChannel()
{
    for(dint i = 0; i < numChannels; ++i)
    {
        if(!usedChannels[i])
            return i;
    }
    return -1;
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

sfxbuffer_t *SdlMixerDriver::SoundPlayer::create(dint flags, dint bits, dint rate)
{
    /// @todo fixme: We have ownership - ensure the buffer is destroyed when the
    /// SoundPlayer is. -ds
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

Sound *SdlMixerDriver::SoundPlayer::makeSound(bool stereoPositioning, int bitsPer, int rate)
{
    std::unique_ptr<Sound> sound(new Sound(*this));
    sound->setBuffer(create(stereoPositioning ? 0 : SFXBF_3D, bitsPer, rate));
    return sound.release();
}

void SdlMixerDriver::SoundPlayer::destroy(sfxbuffer_t *buf)
{
    if(!buf) return;

    Mix_HaltChannel(buf->cursor);
    usedChannels[buf->cursor] = false;
    Z_Free(buf);
}

void SdlMixerDriver::SoundPlayer::load(sfxbuffer_t *buf, sfxsample_t *sample)
{
    DENG2_ASSERT(buf && sample);

    // Does the buffer already have a sample loaded?
    if(buf->sample)
    {
        // Is the same one?
        if(buf->sample->soundId == sample->soundId)
            return;

        // Free the existing data.
        buf->sample = nullptr;
        Mix_FreeChunk((Mix_Chunk *) buf->ptr);
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
    strcpy(conv, "RIFF");
    *(Uint32 *) (conv + 4) = DD_ULONG(4 + 8 + 16 + 8 + sample->size);
    strcpy(conv + 8, "WAVE");

    // Format chunk.
    strcpy(conv + 12, "fmt ");
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
    strcpy(conv + 36, "data");
    *(Uint32 *) (conv + 40) = DD_ULONG(sample->size);
    std::memcpy(conv + 44, sample->data, sample->size);

    buf->ptr = Mix_LoadWAV_RW(SDL_RWFromMem(conv, 44 + sample->size), 1);
    if(!buf->ptr)
    {
        LOG_AS("DS_SDLMixer_SFX_Load");
        LOG_AUDIO_WARNING("Failed loading sample: %s") << Mix_GetError();
    }

    if(conv != localBuf)
    {
        M_Free(conv);
    }

    buf->sample = sample;
}

void SdlMixerDriver::SoundPlayer::stop(sfxbuffer_t *buf)
{
    DENG2_ASSERT(buf);

    if(!buf->sample) return;

    Mix_HaltChannel(buf->cursor);
    //usedChannels[buf->cursor] = false;
    buf->flags &= ~SFXBF_PLAYING;
}

void SdlMixerDriver::SoundPlayer::reset(sfxbuffer_t *buf)
{
    DENG2_ASSERT(buf);

    stop(buf);
    buf->sample = nullptr;

    // Unallocate the resources of the source.
    Mix_FreeChunk((Mix_Chunk *) buf->ptr);
    buf->ptr = nullptr;
}

void SdlMixerDriver::SoundPlayer::refresh(sfxbuffer_t *buf)
{
    DENG2_ASSERT(buf);

    // Can only be done if there is a sample and the buffer is playing.
    if(!buf->sample || !(buf->flags & SFXBF_PLAYING))
        return;

    duint const nowTime = Timer_RealMilliseconds();

    /**
     * Have we passed the predicted end of sample?
     * @note This test fails if the game has been running for about 50 days,
     * since the millisecond counter overflows. It only affects sounds that
     * are playing while the overflow happens, though.
     */
    if(!(buf->flags & SFXBF_REPEAT) && nowTime >= buf->endTime)
    {
        // Time for the sound to stop.
        buf->flags &= ~SFXBF_PLAYING;
    }
}

bool SdlMixerDriver::SoundPlayer::needsRefresh() const
{
    return true;
}

void SdlMixerDriver::SoundPlayer::play(sfxbuffer_t *buf)
{
    DENG2_ASSERT(buf);

    // Playing is quite impossible without a sample.
    if(!buf->sample) return;

    // Update the volume at which the sample will be played.
    Mix_Volume(buf->cursor, buf->written);
    Mix_PlayChannel(buf->cursor, (Mix_Chunk *) buf->ptr, (buf->flags & SFXBF_REPEAT ? -1 : 0));

    // Calculate the end time (milliseconds).
    buf->endTime = Timer_RealMilliseconds() + getBufferLength(*buf);

    // The buffer is now playing.
    buf->flags |= SFXBF_PLAYING;
}

void SdlMixerDriver::SoundPlayer::setFrequency(sfxbuffer_t *, dfloat)
{
    // Not supported.
}

void SdlMixerDriver::SoundPlayer::setOrigin(sfxbuffer_t *buffer, Vector3d const &newOrigin)
{
    // Not supported.
}

/// @param newPan  (-1 ... +1)
void SdlMixerDriver::SoundPlayer::setPan(sfxbuffer_t *buffer, dfloat newPan)
{
    auto const right = dint( (newPan + 1) * 127 );
    Mix_SetPanning(buffer->cursor, 254 - right, right);
}

void SdlMixerDriver::SoundPlayer::setPositioning(sfxbuffer_t *buffer, bool headRelative)
{
    // Not supported.
}

void SdlMixerDriver::SoundPlayer::setVelocity(sfxbuffer_t *buffer, Vector3d const &newVelocity)
{
    // Not supported.
}

void SdlMixerDriver::SoundPlayer::setVolume(sfxbuffer_t *buffer, dfloat newVolume)
{
    // 'written' is used for storing the volume of the channel.
    buffer->written = duint( newVolume * MIX_MAX_VOLUME );
    Mix_Volume(buffer->cursor, buffer->written);
}

void SdlMixerDriver::SoundPlayer::setVolumeAttenuationRange(sfxbuffer_t *buffer, Ranged const &newRange)
{
    // Not supported.
}

void SdlMixerDriver::SoundPlayer::listener(dint, dfloat)
{
    // Not supported.
}

void SdlMixerDriver::SoundPlayer::listenerv(dint, dfloat *)
{
    // Not supported.
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

bool SdlMixerDriver::hasCd() const
{
    return false;
}

bool SdlMixerDriver::hasMusic() const
{
    return d->initialized;
}

bool SdlMixerDriver::hasSfx() const
{
    return d->initialized;
}

ICdPlayer /*const*/ &SdlMixerDriver::iCd() const
{
    return d->iCd;
}

IMusicPlayer /*const*/ &SdlMixerDriver::iMusic() const
{
    return d->iMusic;
}

ISoundPlayer /*const*/ &SdlMixerDriver::iSfx() const
{
    return d->iSfx;
}

}  // namespace audio

#endif  // !DENG_DISABLE_SDLMIXER
