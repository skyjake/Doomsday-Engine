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
#include "audio/channel.h"
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

public:  //- Playback Channels: ---------------------------------------------------------

    class CdChannel : public audio::CdChannel
    {
    public:
        PlayingMode mode() const override;
        void play(PlayingMode mode) override;
        void stop() override;
        bool isPaused() const override;
        void pause() override;
        void resume() override;
        Channel &setVolume(de::dfloat value) override;

        void bindTrack(de::dint track);

    //private:
        CdChannel();
        //friend class DummyDriver;

    private:
        PlayingMode _mode = NotPlaying;
        bool _paused      = false;
        de::dint _track   = -1;
    };

    class MusicChannel : public audio::MusicChannel
    {
    public:
        PlayingMode mode() const override;
        void play(PlayingMode mode) override;
        void stop() override;
        bool isPaused() const override;
        void pause() override;
        void resume() override;
        Channel &setVolume(de::dfloat value) override;

        bool canPlayBuffer() const;
        void *songBuffer(de::duint length);
        bool canPlayFile() const;
        void bindFile(de::String const &sourcePath);

    //private:
        MusicChannel();
        //friend class DummyDriver;

    private:
        PlayingMode _mode      = NotPlaying;
        bool _paused           = false;
        de::String _sourcePath;
    };

    class SoundChannel : public audio::SoundChannel
    {
    public:
        PlayingMode mode() const override;
        void play(PlayingMode mode) override;
        void stop() override;
        bool isPaused() const override;
        void pause() override;
        void resume() override;

        SoundEmitter *emitter() const;
        de::dfloat frequency() const;
        de::Vector3d origin() const;
        Positioning positioning() const;
        de::dfloat volume() const;

        audio::SoundChannel &setEmitter(SoundEmitter *newEmitter);
        audio::SoundChannel &setFrequency(de::dfloat newFrequency);
        audio::SoundChannel &setOrigin(de::Vector3d const &newOrigin);
        Channel             &setVolume(de::dfloat newVolume) override;

        de::dint flags() const;
        void setFlags(de::dint newFlags);

        void update();
        void reset();

        bool format(Positioning positioning, de::dint bytesPer, de::dint rate);
        bool isValid() const;

        void load(sfxsample_t const &sample);

        de::dint bytes() const;
        de::dint rate() const;
        de::dint startTime() const override;
        de::duint endTime() const override;

        sfxsample_t const *samplePtr() const;

        void updateEnvironment();

    //private:
        SoundChannel();
        //friend class DummyDriver;

    private:
        DENG2_PRIVATE(d)
    };

public:  //- Implements audio::System::IDriver: -----------------------------------------

    void initialize();
    void deinitialize();

    Status status() const;

    de::String identityKey() const;
    de::String title() const;

    QList<de::Record> listInterfaces() const;

    Channel *makeChannel(PlaybackInterfaceType type) override;

    de::LoopResult forAllChannels(PlaybackInterfaceType type,
        std::function<de::LoopResult (Channel const &)> callback) const override;

    void allowRefresh(bool allow) override;

private:
    DENG2_PRIVATE(d)
};

}  // namespace audio

#endif  // CLIENT_AUDIO_DUMMYDRIVER_H
