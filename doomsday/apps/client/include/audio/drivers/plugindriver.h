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
#include "audio/sound.h"
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
     * @see newFromLibrary()
     */
    static bool recognize(de::LibraryFile &library);

    /**
     * Attempt to load a new PluginDriver from the given @a library.
     *
     * @return  Pointer to a new PluginDriver instance if successful. Ownership is
     * given to the caller.
     *
     * @see recognize()
     */
    static PluginDriver *newFromLibrary(de::LibraryFile &library);

    /**
     * Returns the plugin library for the loaded audio driver.
     */
    ::Library *library() const;

public:  // Sound players: -------------------------------------------------------

    class CdPlayer : public ICdPlayer
    {
    public:
        de::dint initialize();
        void deinitialize();
        PluginDriver &driver() const;

        void update();
        void setVolume(de::dfloat newVolume);
        bool isPlaying() const;
        void pause(de::dint pause);
        void stop();

        de::dint play(de::dint track, de::dint looped);

    private:
        CdPlayer(PluginDriver &driver);
        friend class PluginDriver;

        bool _initialized     = false;
        bool _needInit        = true;
        PluginDriver *_driver = nullptr;
    };

    class MusicPlayer : public IMusicPlayer
    {
    public:
        de::dint initialize();
        void deinitialize();
        PluginDriver &driver() const;

        void update();
        void setVolume(de::dfloat newVolume);
        bool isPlaying() const;
        void pause(de::dint pause);
        void stop();

        bool canPlayBuffer() const;
        void *songBuffer(de::duint length);
        de::dint play(de::dint looped);

        bool canPlayFile() const;
        de::dint playFile(char const *filename, de::dint looped);

    private:
        MusicPlayer(PluginDriver &driver);
        friend class PluginDriver;

        bool _initialized     = false;
        bool _needInit        = true;
        PluginDriver *_driver = nullptr;
    };

    class SoundPlayer : public ISoundPlayer
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

        Sound *makeSound(bool stereoPositioning, de::dint bitsPer, de::dint rate);

    private:
        SoundPlayer(PluginDriver &driver);
        friend class PluginDriver;

        DENG2_PRIVATE(d)
    };

    class Sound : public audio::Sound
    {
    public:
        Sound(SoundPlayer &player);
        virtual ~Sound();

        bool hasBuffer() const;
        sfxbuffer_t const &buffer() const;
        void setBuffer(sfxbuffer_t *newBuffer);
        void format(bool stereoPositioning, de::dint bytesPer, de::dint rate);
        int flags() const;
        void setFlags(int newFlags);
        struct mobj_s *emitter() const;
        void setEmitter(struct mobj_s *newEmitter);
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

    de::dint playerCount() const;
    de::String playerName(IPlayer const &player) const;
    de::LoopResult forAllPlayers(std::function<de::LoopResult (IPlayer &)> callback) const;

public:
    audiointerface_cd_t &iCd() const;
    audiointerface_music_t &iMusic() const;
    audiointerface_sfx_t &iSound() const;

private:
    PluginDriver();

    DENG2_PRIVATE(d)
};

}  // namespace audio

#endif  // CLIENT_AUDIO_PLUGINDRIVER_H
