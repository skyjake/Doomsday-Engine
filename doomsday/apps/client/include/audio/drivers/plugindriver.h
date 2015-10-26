/** @file plugindriver.h  Plugin based audio driver.
 * @ingroup audio
 *
 * @authors Copyright © 2012-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2013-2015 Daniel Swanson <danij@dengine.net>
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
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#ifndef CLIENT_AUDIO_PLUGINDRIVER_H
#define CLIENT_AUDIO_PLUGINDRIVER_H

#ifdef __SERVER__
#  error "audio" is not available in a SERVER build
#endif

#include "api_audiod_mus.h"  ///< @todo remove me
#include "api_audiod_sfx.h"  ///< @todo remove me
#include "audio/channel.h"
#include "audio/system.h"
#include <doomsday/library.h>
#include <de/LibraryFile>
#include <de/String>

namespace audio {

/**
 * Provides a plugin based audio driver.
 */
class PluginDriver : public audio::System::IDriver
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

public:  // Sound players: -------------------------------------------------------

    class CdPlayer : public IPlayer, public audiointerface_cd_t
    {
    public:
        de::dint initialize();
        void deinitialize();
        PluginDriver &driver() const;

        Channel *makeChannel();

        de::LoopResult forAllChannels(std::function<de::LoopResult (Channel const &)> callback) const;

    private:
        CdPlayer(PluginDriver &driver);
        friend class PluginDriver;

        bool _initialized     = false;
        bool _needInit        = true;
        PluginDriver *_driver = nullptr;
    };

    class MusicPlayer : public IPlayer, public audiointerface_music_t
    {
    public:
        de::dint initialize();
        void deinitialize();
        PluginDriver &driver() const;

        Channel *makeChannel();

        de::LoopResult forAllChannels(std::function<de::LoopResult (Channel const &)> callback) const;

    private:
        MusicPlayer(PluginDriver &driver);
        friend class PluginDriver;

        bool _initialized     = false;
        bool _needInit        = true;
        PluginDriver *_driver = nullptr;
    };

    class SoundPlayer : public ISoundPlayer, public audiointerface_sfx_t
    {
    public:
        /**
         * Returns @c true if the plugin requires refreshing Sounds manually.
         */
        bool needsRefresh() const;

        de::dint initialize();
        void deinitialize();
        PluginDriver &driver() const;

        bool anyRateAccepted() const;
        void allowRefresh(bool allow);

        void listener(de::dint prop, de::dfloat value);
        void listenerv(de::dint prop, de::dfloat *values);

        Channel *makeChannel();

        de::LoopResult forAllChannels(std::function<de::LoopResult (Channel const &)> callback) const;

    private:
        SoundPlayer(PluginDriver &driver);
        friend class PluginDriver;

        DENG2_PRIVATE(d)
    };

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

    private:
        CdChannel(PluginDriver &driver);
        friend class CdPlayer;

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

    private:
        MusicChannel(PluginDriver &driver);
        friend class MusicPlayer;

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
        de::dint startTime() const;
        de::dint endTime() const;

        sfxsample_t const *samplePtr() const;

        void updateEnvironment();

    private:
        SoundChannel(PluginDriver &owner);
        friend class SoundPlayer;

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

    de::LoopResult forAllChannels(PlaybackInterfaceType type,
        std::function<de::LoopResult (Channel const &)> callback) const;

public:
    CdPlayer    &iCd   () const;
    MusicPlayer &iMusic() const;
    SoundPlayer &iSound() const;

private:
    PluginDriver();

    DENG2_PRIVATE(d)
};

}  // namespace audio

#endif  // CLIENT_AUDIO_PLUGINDRIVER_H
