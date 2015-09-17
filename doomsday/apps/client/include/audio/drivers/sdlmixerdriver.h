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

#include <de/liblegacy.h>
#include "api_audiod.h"
#include "api_audiod_sfx.h"

#include "audio/system.h"
#include <de/String>

namespace audio {

/**
 * Provides an audio driver for playback usnig SDL_mixer.
 */
class SdlMixerDriver : public audio::System::IDriver
{
public:
    SdlMixerDriver();

    /**
     * If the driver is still initialized it will be automatically deinitialized
     * when this is called.
     */
    virtual ~SdlMixerDriver();

public:  // Sound players: -------------------------------------------------------

    class CdPlayer : public ICdPlayer
    {
    public:
        de::String name() const;

        de::dint init();
        void shutdown();

        void update();
        void set(de::dint prop, de::dfloat value);
        de::dint get(de::dint prop, void *value) const;
        void pause(de::dint pause);
        void stop();

        de::dint play(de::dint track, de::dint looped);

    private:
        CdPlayer(SdlMixerDriver &driver);
        friend class SdlMixerDriver;

        bool _initialized = false;
    };

    class MusicPlayer : public IMusicPlayer
    {
    public:
        de::String name() const;

        de::dint init();
        void shutdown();

        void update();
        void set(de::dint prop, de::dfloat value);
        de::dint get(de::dint prop, void *value) const;
        void pause(de::dint pause);
        void stop();

        bool canPlayBuffer() const;
        void *songBuffer(de::duint length);
        de::dint play(de::dint looped);

        bool canPlayFile() const;
        de::dint playFile(char const *filename, de::dint looped);

    private:
        MusicPlayer(SdlMixerDriver &driver);
        friend class SdlMixerDriver;

        bool _initialized = false;
    };

    class SoundPlayer : public ISoundPlayer
    {
    public:
        de::String name() const;

        de::dint init();

        Sound *makeSound(bool stereoPositioning, de::dint bitsPer, de::dint rate);
        sfxbuffer_t *create(de::dint flags, de::dint bits, de::dint rate);

        void destroy(sfxbuffer_t *buffer);
        void load(sfxbuffer_t *buffer, sfxsample_t *sample);
        void reset(sfxbuffer_t *buffer);
        void play(sfxbuffer_t *buffer);
        void stop(sfxbuffer_t *buffer);
        void refresh(sfxbuffer_t *buffer);
        void set(sfxbuffer_t *buffer, de::dint prop, de::dfloat value);
        void setv(sfxbuffer_t *buffer, de::dint prop, de::dfloat *values);
        void listener(de::dint prop, de::dfloat value);
        void listenerv(de::dint prop, de::dfloat *values);
        de::dint getv(de::dint prop, void *values) const;

    private:
        SoundPlayer(SdlMixerDriver &driver);
        friend class SdlMixerDriver;

        bool _initialized = false;
    };

public:  // Implements audio::System::IDriver: -----------------------------------

    void initialize();
    void deinitialize();

    Status status() const;

    de::String identityKey() const;
    de::String title() const;

    bool hasCd() const;
    bool hasMusic() const;
    bool hasSfx() const;

    ICdPlayer /*const*/ &iCd() const;
    IMusicPlayer /*const*/ &iMusic() const;
    ISoundPlayer /*const*/ &iSfx() const;
    de::DotPath interfacePath(IPlayer &player) const;

private:
    DENG2_PRIVATE(d)
};

}  // namespace audio

#endif  // CLIENT_AUDIO_SDLMIXERDRIVER_H
#endif  // !DENG_DISABLE_SDLMIXER
