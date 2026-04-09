/** @file sys_audiod_sdlmixer.cpp  SDL3_mixer, for SFX, Ext and Mus interfaces.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef DE_DISABLE_SDLMIXER

#include "de_base.h"
#include "audio/sys_audiod_sdlmixer.h"

#include <cstdlib>
#include <cstring>
#include <SDL3/SDL.h>
#include <SDL3_mixer/SDL_mixer.h>
#undef main

#include <de/legacy/timer.h>
#include <de/log.h>

#include "api_audiod.h"
#include "api_audiod_sfx.h"
#include "api_audiod_mus.h"

int         DS_SDLMixerInit(void);
void        DS_SDLMixerShutdown(void);
void        DS_SDLMixerEvent(int type);

int         DS_SDLMixer_SFX_Init(void);
sfxbuffer_t* DS_SDLMixer_SFX_CreateBuffer(int flags, int bits, int rate);
void        DS_SDLMixer_SFX_DestroyBuffer(sfxbuffer_t* buf);
void        DS_SDLMixer_SFX_Load(sfxbuffer_t* buf, struct sfxsample_s* sample);
void        DS_SDLMixer_SFX_Reset(sfxbuffer_t* buf);
void        DS_SDLMixer_SFX_Play(sfxbuffer_t* buf);
void        DS_SDLMixer_SFX_Stop(sfxbuffer_t* buf);
void        DS_SDLMixer_SFX_Refresh(sfxbuffer_t* buf);
void        DS_SDLMixer_SFX_Set(sfxbuffer_t* buf, int prop, float value);
void        DS_SDLMixer_SFX_Setv(sfxbuffer_t* buf, int prop, float* values);
void        DS_SDLMixer_SFX_Listener(int prop, float value);
void        DS_SDLMixer_SFX_Listenerv(int prop, float* values);

int         DS_SDLMixer_Music_Init(void);
void        DS_SDLMixer_Music_Update(void);
void        DS_SDLMixer_Music_Set(int prop, float value);
int         DS_SDLMixer_Music_Get(int prop, void* value);
void        DS_SDLMixer_Music_Pause(int pause);
void        DS_SDLMixer_Music_Stop(void);
int         DS_SDLMixer_Music_PlayFile(const char* fileName, int looped);

static dd_bool  sdlInitOk    = false;
static float    musicVolume  = 1.f;

audiodriver_t audiod_sdlmixer = {
    DS_SDLMixerInit,
    DS_SDLMixerShutdown,
    DS_SDLMixerEvent
};

audiointerface_sfx_t audiod_sdlmixer_sfx = { {
    DS_SDLMixer_SFX_Init,
    DS_SDLMixer_SFX_CreateBuffer,
    DS_SDLMixer_SFX_DestroyBuffer,
    DS_SDLMixer_SFX_Load,
    DS_SDLMixer_SFX_Reset,
    DS_SDLMixer_SFX_Play,
    DS_SDLMixer_SFX_Stop,
    DS_SDLMixer_SFX_Refresh,
    DS_SDLMixer_SFX_Set,
    DS_SDLMixer_SFX_Setv,
    DS_SDLMixer_SFX_Listener,
    DS_SDLMixer_SFX_Listenerv
} };

audiointerface_music_t audiod_sdlmixer_music = { {
    DS_SDLMixer_Music_Init,
    NULL,
    DS_SDLMixer_Music_Update,
    DS_SDLMixer_Music_Set,
    DS_SDLMixer_Music_Get,
    DS_SDLMixer_Music_Pause,
    DS_SDLMixer_Music_Stop },
    NULL,
    NULL,
    DS_SDLMixer_Music_PlayFile,
};

// PRIVATE DATA DEFINITIONS ------------------------------------------------

/// The SDL3_mixer device context. Owns all tracks.
static MIX_Mixer *sdlMixer       = nullptr;

/// Dedicated track for the music interface.
static MIX_Track *musicTrack     = nullptr;

/// Last loaded music audio object (freed when a new one is loaded).
static MIX_Audio *lastMusicAudio = nullptr;

// CODE --------------------------------------------------------------------

/**
 * Retrieve the MIX_Track stored in a sfxbuffer (in ptr3D).
 */
static inline MIX_Track *bufTrack(sfxbuffer_t *buf)
{
    return static_cast<MIX_Track *>(buf->ptr3D);
}

/**
 * @return Length of the buffer in milliseconds.
 */
static unsigned int getBufLength(sfxbuffer_t* buf)
{
    if (!buf)
        return 0;
    return 1000 * buf->sample->numSamples / buf->freq;
}

/**
 * SDL_AudioStream get-callback for SFXBF_STREAM buffers.
 *
 * Called by SDL from the audio mixing thread whenever the mixer needs more
 * audio data. buf->sample->data is interpreted as an sfxstreamfunc_t that
 * pulls PCM data from the upstream source (e.g. the FluidSynth ring buffer).
 *
 * The stream produces stereo S16 PCM at the sample rate specified when the
 * buffer was created (always 44100 Hz for FluidSynth).
 */
static void sfxStreamGetCallback(void *userdata,
                                 SDL_AudioStream *stream,
                                 int additional_amount,
                                 int /*total_amount*/)
{
    auto *buf = static_cast<sfxbuffer_t *>(userdata);
    if (!buf->sample || !(buf->flags & SFXBF_PLAYING))
        return;

    sfxstreamfunc_t func = reinterpret_cast<sfxstreamfunc_t>(buf->sample->data);

    char temp[4096];
    while (additional_amount > 0)
    {
        const int chunk = (additional_amount < (int)sizeof(temp))
                              ? additional_amount
                              : (int)sizeof(temp);
        const int got = func(buf, temp, chunk);
        if (got <= 0)
            break;
        SDL_PutAudioStreamData(stream, temp, got);
        additional_amount -= got;
    }
}

#if _DEBUG
static void musicStoppedCallback(void * /*userdata*/, MIX_Track * /*track*/)
{
    LOG_AUDIO_VERBOSE("[SDLMixer] Music playback finished");
}
#endif

int DS_SDLMixerInit(void)
{
    if (sdlInitOk)
        return true;

    if (!MIX_Init())
    {
        LOG_AUDIO_ERROR("Failed initializing SDL_mixer: %s") << SDL_GetError();
        return false;
    }

    {
        const int ver = MIX_Version();
        LOG_AUDIO_VERBOSE("SDL_mixer version: %d.%d.%d")
            << SDL_VERSIONNUM_MAJOR(ver)
            << SDL_VERSIONNUM_MINOR(ver)
            << SDL_VERSIONNUM_MICRO(ver);
    }

    // Request a standard stereo output format; SDL_mixer converts as needed.
    SDL_AudioSpec spec;
    SDL_zero(spec);
    spec.format   = SDL_AUDIO_S16;
    spec.channels = 2;
    spec.freq     = 44100;

    sdlMixer = MIX_CreateMixerDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec);
    if (!sdlMixer)
    {
        LOG_AUDIO_ERROR("Failed to open SDL_mixer device: %s") << SDL_GetError();
        MIX_Quit();
        return false;
    }

    // Query the actual output format for informational logging.
    SDL_AudioSpec actual;
    if (MIX_GetMixerFormat(sdlMixer, &actual))
    {
        LOG_AUDIO_VERBOSE("SDLMixer configuration:");
        LOG_AUDIO_VERBOSE("  " _E(>) "Output: %s\nFormat: 0x%x\nFrequency: %i Hz")
            << (actual.channels > 1 ? "stereo" : "mono")
            << (int)actual.format
            << actual.freq;
    }

    // Allocate a dedicated track for music so it can be managed independently.
    musicTrack = MIX_CreateTrack(sdlMixer);
    if (!musicTrack)
    {
        LOG_AUDIO_WARNING("Failed to create music track: %s") << SDL_GetError();
    }

    sdlInitOk = true;
    return true;
}

void DS_SDLMixerShutdown(void)
{
    if (!sdlInitOk)
        return;

    sdlInitOk = false;

    musicTrack = nullptr; // Owned by the mixer; destroyed with it below.

    if (lastMusicAudio)
    {
        MIX_DestroyAudio(lastMusicAudio);
        lastMusicAudio = nullptr;
    }

    // MIX_DestroyMixer stops all audio, destroys all tracks, and calls
    // SDL_QuitSubSystem(SDL_INIT_AUDIO) for us.
    MIX_DestroyMixer(sdlMixer);
    sdlMixer = nullptr;

    MIX_Quit();
}

void DS_SDLMixerEvent(int)
{
    // Not supported.
}

int DS_SDLMixer_SFX_Init(void)
{
    return sdlInitOk;
}

sfxbuffer_t* DS_SDLMixer_SFX_CreateBuffer(int flags, int bits, int rate)
{
    auto *buf = static_cast<sfxbuffer_t *>(Z_Calloc(sizeof(sfxbuffer_t), PU_APPSTATIC, 0));

    buf->bytes = bits / 8;
    buf->rate  = rate;
    buf->flags = flags;
    buf->freq  = rate; // May be changed later via SFXBP_FREQUENCY.

    // Each buffer gets its own track. ptr3D holds the MIX_Track*.
    buf->ptr3D = MIX_CreateTrack(sdlMixer);
    if (!buf->ptr3D)
    {
        LOG_AUDIO_WARNING("DS_SDLMixer_SFX_CreateBuffer: MIX_CreateTrack failed: %s")
            << SDL_GetError();
    }

    return buf;
}

void DS_SDLMixer_SFX_DestroyBuffer(sfxbuffer_t* buf)
{
    if (!buf)
        return;

    if (buf->flags & SFXBF_STREAM)
    {
        // Detach and destroy the SDL_AudioStream.
        if (auto *track = bufTrack(buf))
            MIX_SetTrackAudioStream(track, nullptr);

        if (buf->ptr)
        {
            SDL_DestroyAudioStream(static_cast<SDL_AudioStream *>(buf->ptr));
            buf->ptr = nullptr;
        }
    }
    else
    {
        // Destroy the loaded MIX_Audio.
        if (buf->ptr)
        {
            MIX_DestroyAudio(static_cast<MIX_Audio *>(buf->ptr));
            buf->ptr = nullptr;
        }
    }

    // Destroy the track last so the above detach is safe.
    if (buf->ptr3D)
    {
        MIX_DestroyTrack(static_cast<MIX_Track *>(buf->ptr3D));
        buf->ptr3D = nullptr;
    }

    Z_Free(buf);
}

void DS_SDLMixer_SFX_Load(sfxbuffer_t* buf, struct sfxsample_s* sample)
{
    if (!buf || !sample)
        return;

    if (buf->flags & SFXBF_STREAM)
    {
        // For streaming buffers, sample->data is an sfxstreamfunc_t callback
        // (see api_audiod_sfx.h). Create an SDL_AudioStream that invokes it
        // whenever the mixer needs more audio.

        // Free any existing stream first.
        if (buf->ptr)
        {
            if (auto *track = bufTrack(buf))
                MIX_SetTrackAudioStream(track, nullptr);
            SDL_DestroyAudioStream(static_cast<SDL_AudioStream *>(buf->ptr));
            buf->ptr = nullptr;
        }

        buf->sample = sample;

        // FluidSynth always renders stereo S16 at the given sample rate.
        SDL_AudioSpec streamSpec;
        SDL_zero(streamSpec);
        streamSpec.format   = (sample->bytesPer == 2) ? SDL_AUDIO_S16 : SDL_AUDIO_S8;
        streamSpec.channels = 2;
        streamSpec.freq     = sample->rate;

        auto *stream = SDL_CreateAudioStream(&streamSpec, &streamSpec);
        if (!stream)
        {
            LOG_AUDIO_WARNING("DS_SDLMixer_SFX_Load: SDL_CreateAudioStream failed: %s")
                << SDL_GetError();
            return;
        }

        // The get-callback feeds data pulled from the sfxstreamfunc_t into the
        // stream whenever SDL_mixer needs more samples.
        SDL_SetAudioStreamGetCallback(stream, sfxStreamGetCallback, buf);

        buf->ptr = stream;
    }
    else
    {
        // Does the buffer already have a sample loaded?
        if (buf->sample)
        {
            if (buf->sample->id == sample->id)
                return; // Same sample; nothing to do.

            // Discard the existing audio.
            buf->sample = nullptr;
            MIX_DestroyAudio(static_cast<MIX_Audio *>(buf->ptr));
            buf->ptr = nullptr;
        }

        // Transfer the sample to SDL_mixer as an in-memory WAVE file.
        static char localBuf[0x40000];
        char *conv  = nullptr;
        size_t size = 44 + sample->size;

        if (size <= sizeof(localBuf))
            conv = localBuf;
        else
            conv = (char *) M_Malloc(size);

        // RIFF/WAVE header.
        strcpy(conv, "RIFF");
        *(Uint32 *)(conv + 4) = DD_ULONG(36 + sample->size);
        strcpy(conv + 8, "WAVE");

        // "fmt " chunk.
        strcpy(conv + 12, "fmt ");
        *(Uint32 *)(conv + 16) = DD_ULONG(16);
        *(Uint16 *)(conv + 20) = DD_USHORT(1);                      // PCM
        *(Uint16 *)(conv + 22) = DD_USHORT(1);                      // mono
        *(Uint32 *)(conv + 24) = DD_ULONG(sample->rate);
        *(Uint32 *)(conv + 28) = DD_ULONG(sample->rate * sample->bytesPer);
        *(Uint16 *)(conv + 32) = DD_USHORT(sample->bytesPer);
        *(Uint16 *)(conv + 34) = DD_USHORT(sample->bytesPer * 8);

        // "data" chunk.
        strcpy(conv + 36, "data");
        *(Uint32 *)(conv + 40) = DD_ULONG(sample->size);
        memcpy(conv + 44, sample->data, sample->size);

        auto *audio = MIX_LoadAudio_IO(sdlMixer,
                                       SDL_IOFromMem(conv, (int)size),
                                       /*predecode=*/true,
                                       /*closeio=*/true);
        if (!audio)
        {
            LOG_AS("DS_SDLMixer_SFX_Load");
            LOG_AUDIO_WARNING("Failed loading sample: %s") << SDL_GetError();
        }

        if (conv != localBuf)
            M_Free(conv);

        buf->ptr    = audio;
        buf->sample = sample;
    }
}

/**
 * Stops the buffer and makes it forget about its sample.
 */
void DS_SDLMixer_SFX_Reset(sfxbuffer_t* buf)
{
    if (!buf)
        return;

    DS_SDLMixer_SFX_Stop(buf);
    buf->sample = nullptr;

    if (buf->flags & SFXBF_STREAM)
    {
        if (auto *track = bufTrack(buf))
            MIX_SetTrackAudioStream(track, nullptr);

        if (buf->ptr)
        {
            SDL_DestroyAudioStream(static_cast<SDL_AudioStream *>(buf->ptr));
            buf->ptr = nullptr;
        }
    }
    else
    {
        if (buf->ptr)
        {
            MIX_DestroyAudio(static_cast<MIX_Audio *>(buf->ptr));
            buf->ptr = nullptr;
        }
    }
}

void DS_SDLMixer_SFX_Play(sfxbuffer_t* buf)
{
    if (!buf || !buf->sample)
        return;

    auto *track = bufTrack(buf);
    if (!track)
        return;

    if (buf->flags & SFXBF_STREAM)
    {
        auto *stream = static_cast<SDL_AudioStream *>(buf->ptr);
        if (!stream)
            return;

        MIX_SetTrackAudioStream(track, stream);

        // Mark playing before MIX_PlayTrack so the get-callback can produce
        // data as soon as the mixer thread starts mixing this track.
        buf->flags |= SFXBF_PLAYING;

        // Keep the track alive even when the stream temporarily runs dry
        // (FluidSynth synthesis may lag behind momentarily).
        SDL_PropertiesID props = SDL_CreateProperties();
        SDL_SetBooleanProperty(props, MIX_PROP_PLAY_HALT_WHEN_EXHAUSTED_BOOLEAN, false);
        MIX_PlayTrack(track, props);
        SDL_DestroyProperties(props);
    }
    else
    {
        auto *audio = static_cast<MIX_Audio *>(buf->ptr);
        if (!audio)
            return;

        MIX_SetTrackAudio(track, audio);

        buf->flags |= SFXBF_PLAYING;

        SDL_PropertiesID props = 0;
        if (buf->flags & SFXBF_REPEAT)
        {
            props = SDL_CreateProperties();
            SDL_SetNumberProperty(props, MIX_PROP_PLAY_LOOPS_NUMBER, -1LL);
        }
        MIX_PlayTrack(track, props);
        if (props)
            SDL_DestroyProperties(props);

        // Record the predicted end time for Refresh to detect completion.
        buf->endTime = Timer_RealMilliseconds() + getBufLength(buf);
    }
}

void DS_SDLMixer_SFX_Stop(sfxbuffer_t* buf)
{
    if (!buf || !buf->sample)
        return;

    if (auto *track = bufTrack(buf))
        MIX_StopTrack(track, 0);

    buf->flags &= ~SFXBF_PLAYING;
}

void DS_SDLMixer_SFX_Refresh(sfxbuffer_t* buf)
{
    if (!buf || !buf->sample || !(buf->flags & SFXBF_PLAYING))
        return;

    // Streaming buffers play continuously until explicitly stopped.
    if (buf->flags & SFXBF_STREAM)
        return;

    // For non-repeating static samples, check whether the predicted end time
    // has passed. (This test wraps after ~50 days of uptime, but only affects
    // sounds that happen to be playing at the moment of overflow.)
    if (!(buf->flags & SFXBF_REPEAT) && Timer_RealMilliseconds() >= buf->endTime)
    {
        buf->flags &= ~SFXBF_PLAYING;
    }
}

void DS_SDLMixer_SFX_Set(sfxbuffer_t* buf, int prop, float value)
{
    if (!buf)
        return;

    auto *track = bufTrack(buf);

    switch (prop)
    {
    case SFXBP_VOLUME:
        if (track)
            MIX_SetTrackGain(track, value);
        break;

    case SFXBP_PAN: // -1 .. +1, left to right
        if (track)
        {
            // Linear pan: center = (0.5, 0.5), hard-left = (1.0, 0.0), hard-right = (0.0, 1.0).
            const float t   = (value + 1.f) * 0.5f;
            MIX_StereoGains gains;
            gains.left  = 1.f - t;
            gains.right = t;
            MIX_SetTrackStereo(track, &gains);
        }
        break;

    default:
        break;
    }
}

void DS_SDLMixer_SFX_Setv(sfxbuffer_t *, int, float *)
{
    // Not supported.
}

void DS_SDLMixer_SFX_Listener(int, float)
{
    // Not supported.
}

void DS_SDLMixer_SFX_Listenerv(int, float *)
{
    // Not supported.
}

int DS_SDLMixer_Music_Init(void)
{
#if _DEBUG
    if (musicTrack)
        MIX_SetTrackStoppedCallback(musicTrack, musicStoppedCallback, nullptr);
#endif
    return sdlInitOk;
}

void DS_SDLMixer_Music_Update(void)
{
    // Nothing to update.
}

void DS_SDLMixer_Music_Set(int prop, float value)
{
    if (!sdlInitOk || !musicTrack)
        return;

    switch (prop)
    {
    case MUSIP_VOLUME:
        musicVolume = value;
        MIX_SetTrackGain(musicTrack, value);
        break;

    default:
        break;
    }
}

int DS_SDLMixer_Music_Get(int prop, void* value)
{
    if (!sdlInitOk)
        return false;

    switch (prop)
    {
    case MUSIP_ID:
        strcpy((char *) value, "SDLMixer::Music");
        break;

    case MUSIP_PLAYING:
        return musicTrack && MIX_TrackPlaying(musicTrack);

    default:
        return false;
    }
    return true;
}

void DS_SDLMixer_Music_Pause(int pause)
{
    if (!sdlInitOk || !musicTrack)
        return;

    if (pause)
        MIX_PauseTrack(musicTrack);
    else
        MIX_ResumeTrack(musicTrack);
}

void DS_SDLMixer_Music_Stop(void)
{
    if (!sdlInitOk || !musicTrack)
        return;

    MIX_StopTrack(musicTrack, 0);
}

int DS_SDLMixer_Music_PlayFile(const char* filename, int looped)
{
    if (!sdlInitOk || !musicTrack)
        return false;

    // Stop and release any previously loaded music.
    if (lastMusicAudio)
    {
        MIX_StopTrack(musicTrack, 0);
        MIX_DestroyAudio(lastMusicAudio);
        lastMusicAudio = nullptr;
    }

    // Stream large music files from disk rather than loading them fully into RAM.
    lastMusicAudio = MIX_LoadAudio(sdlMixer, filename, /*predecode=*/false);
    if (!lastMusicAudio)
    {
        LOG_AS("DS_SDLMixer_Music_PlayFile");
        LOG_AUDIO_ERROR("Failed to load music: %s") << SDL_GetError();
        return false;
    }

    MIX_SetTrackAudio(musicTrack, lastMusicAudio);
    MIX_SetTrackGain(musicTrack, musicVolume);

    SDL_PropertiesID props = SDL_CreateProperties();
    SDL_SetNumberProperty(props, MIX_PROP_PLAY_LOOPS_NUMBER, looped ? -1LL : 0LL);
    const bool result = MIX_PlayTrack(musicTrack, props);
    SDL_DestroyProperties(props);

    return result;
}

#endif // DE_DISABLE_SDLMIXER
