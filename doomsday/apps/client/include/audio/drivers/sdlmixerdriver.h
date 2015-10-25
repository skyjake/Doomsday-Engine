/** @file sdlmixerdriver.h  Audio driver for playback using SDL_mixer.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2007-2015 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2006-2007 Jamie Jones <yagisan@dengine.net>
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
#ifndef CLIENT_AUDIO_SDLMIXERDRIVER_H
#define CLIENT_AUDIO_SDLMIXERDRIVER_H

#ifdef __SERVER__
#  error "audio" is not available in a SERVER build
#endif

#include "api_audiod_sfx.h"  ///< sfxbuffer_t @todo remove me
#include "audio/channel.h"
#include "audio/system.h"
#include <de/String>
#include <de/liblegacy.h>

namespace audio {

/**
 * Provides an audio driver for playback usnig SDL_mixer.
 */
class SdlMixerDriver : public audio::System::IDriver
{
public:
    SdlMixerDriver();

    /**
     * If the driver is still initialized it will be automatically deinitialized when this
     * is called.
     */
    virtual ~SdlMixerDriver();

public:  // Sound players: --------------------------------------------------------------

    class MusicPlayer : public IPlayer
    {
    public:
        de::dint initialize();
        void deinitialize();

        Channel *makeChannel();

        de::LoopResult forAllChannels(std::function<de::LoopResult (Channel const &)> callback) const;

    private:
        MusicPlayer();
        friend class SdlMixerDriver;

        bool _initialized = false;
    };

    class SoundPlayer : public ISoundPlayer
    {
    public:
        de::dint initialize();
        void deinitialize();

        bool anyRateAccepted() const;
        void allowRefresh(bool allow);

        void listener(de::dint prop, de::dfloat value);
        void listenerv(de::dint prop, de::dfloat *values);

        Channel *makeChannel();

        de::LoopResult forAllChannels(std::function<de::LoopResult (Channel const &)> callback) const;

    private:
        SoundPlayer();
        friend class SdlMixerDriver;
        
        DENG2_PRIVATE(d)
    };

    class MusicChannel : public audio::MusicChannel
    {
    public:
        Channel &setVolume(de::dfloat newVolume);
        void stop();

        void update();
        bool isPlaying() const;
        void pause(de::dint pause);

        bool canPlayBuffer() const;
        void *songBuffer(de::duint length);
        de::dint play(de::dint looped);

        bool canPlayFile() const;
        de::dint playFile(de::String const &filename, de::dint looped);

    public:
        MusicChannel();
        friend class MusicPlayer;
    };

    class SoundChannel : public audio::SoundChannel
    {
    public:
        PlayingMode mode() const;

        void play(PlayingMode mode);
        void stop();

        SoundEmitter *emitter() const;
        de::dfloat frequency() const;
        de::Vector3d origin() const;
        Positioning positioning() const;
        de::dfloat volume() const;

        audio::SoundChannel &setEmitter(SoundEmitter *newEmitter);
        audio::SoundChannel &setFrequency(de::dfloat newFrequency);
        audio::SoundChannel &setOrigin(de::Vector3d const &newOrigin);
        Channel             &setVolume(de::dfloat newVolume);

        de::dint flags() const;
        void setFlags(de::dint newFlags);

        void update();
        void reset();

        bool format(Positioning positioning, de::dint bytesPer, de::dint rate);
        bool isValid() const;

        void load(sfxsample_t const &sample);

        de::dint bytes() const;
        de::dint rate() const;
        de::dint startTime() const;
        de::dint endTime() const;

        sfxsample_t const *samplePtr() const;

        void updateEnvironment();

    private:
        SoundChannel();
        friend class SoundPlayer;

        DENG2_PRIVATE(d)
    };

public:  // Implements audio::System::IDriver: ------------------------------------------

    void initialize();
    void deinitialize();

    Status status() const;

    de::String identityKey() const;
    de::String title() const;

    QList<de::Record> listInterfaces() const;

    IPlayer &findPlayer   (de::String interfaceIdentityKey) const;
    IPlayer *tryFindPlayer(de::String interfaceIdentityKey) const;

    de::LoopResult forAllChannels(PlaybackInterfaceType type,
        std::function<de::LoopResult (Channel const &)> callback) const;

private:
    DENG2_PRIVATE(d)
};

}  // namespace audio

#endif  // CLIENT_AUDIO_SDLMIXERDRIVER_H
#endif  // !DENG_DISABLE_SDLMIXER
