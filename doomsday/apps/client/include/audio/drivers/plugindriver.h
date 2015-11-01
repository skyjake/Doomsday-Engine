/** @file plugindriver.h  Audio driver for playback using a Doomsday audio plugin.
 * @ingroup audio
 *
 * @authors Copyright © 2012-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2013-2015 Daniel Swanson <danij@dengine.net>
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

#ifndef CLIENT_AUDIO_PLUGINDRIVER_H
#define CLIENT_AUDIO_PLUGINDRIVER_H

#ifdef __SERVER__
#  error "audio" is not available in a SERVER build
#endif

#include "audio/channel.h"
#include "audio/idriver.h"
#include <doomsday/library.h>
#include <de/LibraryFile>
#include <de/String>

struct audiointerface_cd_s;
struct audiointerface_music_s;
struct audiointerface_sfx_s;

namespace audio {

/**
 * Provides a plugin based audio driver.
 */
class PluginDriver : public IDriver
{
public:
    /**
     * If the driver is still initialized it will be automatically deinitialized
     * when this is called, before then unloading the underlying Library.
     */
    virtual ~PluginDriver();

    /**
     * Returns @c true if the given @a library appears to be useable with PluginDriver.
     *
     * @see interpretFile()
     */
    static bool recognize(de::LibraryFile &library);

    /**
     * Attempt to interpret the given @a library as a PluginDriver.
     *
     * @return  Pointer to a new PluginDriver instance if successful. Ownership is
     * given to the caller.
     *
     * @see recognize()
     */
    static PluginDriver *interpretFile(de::LibraryFile *library);

    /**
     * Returns the plugin library for the loaded audio driver.
     */
    ::Library *library() const;

public:  //- Playback channels: ---------------------------------------------------------

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
        CdChannel(PluginDriver &driver);
        ~CdChannel();
        //friend class PluginDriver;

    private:
        PluginDriver *_driver = nullptr;
        PlayingMode _mode = NotPlaying;
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
        MusicChannel(PluginDriver &driver);
        ~MusicChannel();
        //friend class PluginDriver;

    private:
        PluginDriver *_driver = nullptr;
        PlayingMode _mode = NotPlaying;
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

        bool anyRateAccepted() const override;

    //private:
        SoundChannel(PluginDriver &driver);
        ~SoundChannel();
        //friend class PluginDriver;

    private:
        DENG2_PRIVATE(d)
    };

public:  //- Implements audio::System::IDriver: -----------------------------------------

    void initialize();
    void deinitialize();

    Status status() const;

    de::String identityKey() const;
    de::String title() const;

    void initInterface(de::String const &identityKey) override;
    void deinitInterface(de::String const &identityKey) override;

    QList<de::Record> listInterfaces() const;

    IChannelFactory &channelFactory() const override;

    de::LoopResult forAllChannels(Channel::Type type,
        std::function<de::LoopResult (Channel const &)> callback) const override;

    void allowRefresh(bool allow) override;

public:
    struct audiointerface_cd_s    &iCd   () const;
    struct audiointerface_music_s &iMusic() const;
    struct audiointerface_sfx_s   &iSound() const;

private:
    PluginDriver();
    DENG2_PRIVATE(d)
};

}  // namespace audio

#endif  // CLIENT_AUDIO_PLUGINDRIVER_H
