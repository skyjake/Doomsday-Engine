/** @file dummydriver.cpp  Dummy audio driver.
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

#ifndef CLIENT_AUDIO_DUMMYDRIVER_H
#define CLIENT_AUDIO_DUMMYDRIVER_H

#ifdef __SERVER__
#  error "audio" is not available in a SERVER build
#endif

#include "api_audiod_sfx.h"  ///< sfxbuffer_t @todo remove me
#include "audio/sound.h"
#include "audio/system.h"
#include <de/String>
#include <de/liblegacy.h>

namespace audio {

/**
 * Provides a null-op audio driver.
 */
class DummyDriver : public audio::System::IDriver
{
public:
    DummyDriver();

    /**
     * If the driver is still initialized it will be automatically deinitialized
     * when this is called.
     */
    virtual ~DummyDriver();

public:  // Sound players: -------------------------------------------------------

    class CdPlayer : public ICdPlayer
    {
    public:
        de::dint initialize();
        void deinitialize();

        void update();
        void setVolume(de::dfloat newVolume);
        bool isPlaying() const;
        void pause(de::dint pause);
        void stop();

        de::dint play(de::dint track, de::dint looped);

    private:
        CdPlayer();
        friend class DummyDriver;

        bool _initialized = false;
    };

    class MusicPlayer : public IMusicPlayer
    {
    public:
        de::dint initialize();
        void deinitialize();

        void update();
        void setVolume(de::dfloat value);
        bool isPlaying() const;
        void pause(de::dint pause);
        void stop();

        bool canPlayBuffer() const;
        void *songBuffer(de::duint length);
        de::dint play(de::dint looped);

        bool canPlayFile() const;
        de::dint playFile(char const *filename, de::dint looped);

    private:
        MusicPlayer();
        friend class DummyDriver;

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

        Sound *makeSound(bool stereoPositioning, de::dint bitsPer, de::dint rate);

    private:
        SoundPlayer();
        friend class DummyDriver;

        DENG2_PRIVATE(d)
    };

    class Sound : public audio::Sound
    {
    public:
        Sound(bool stereoPositioning, de::dint bytesPer, de::dint rate);
        virtual ~Sound();

        bool isValid() const;
        sfxsample_t const *samplePtr() const;
        void format(bool stereoPositioning, de::dint bytesPer, de::dint rate);
        int flags() const;
        void setFlags(int newFlags);
        SoundEmitter *emitter() const;
        void setEmitter(SoundEmitter *newEmitter);
        void setOrigin(de::Vector3d const &newOrigin);
        de::Vector3d origin() const;
        audio::Sound &setFrequency(de::dfloat newFrequency);
        audio::Sound &setVolume(de::dfloat newVolume);
        bool isPlaying() const;
        de::dfloat frequency() const;
        de::dfloat volume() const;
        void load(sfxsample_t &sample);
        void stop();
        void reset();
        void play();
        void setPlayingMode(de::dint sfFlags);
        de::dint startTime() const;
        de::dint endTime() const;
        void refresh();

    private:
        DENG2_PRIVATE(d)
    };

public:  // Implements audio::System::IDriver: -----------------------------------

    void initialize();
    void deinitialize();

    Status status() const;

    de::String identityKey() const;
    de::String title() const;

    QList<de::Record> listInterfaces() const;

    IPlayer &findPlayer   (de::String interfaceIdentityKey) const;
    IPlayer *tryFindPlayer(de::String interfaceIdentityKey) const;

private:
    DENG2_PRIVATE(d)
};

}  // namespace audio

#endif  // CLIENT_AUDIO_DUMMYDRIVER_H
