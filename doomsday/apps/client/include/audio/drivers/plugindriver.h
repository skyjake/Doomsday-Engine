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
        CdPlayer(PluginDriver &driver);
        friend class PluginDriver;

        bool _initialized = false;
        audiointerface_cd_t _imp;
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
        MusicPlayer(PluginDriver &driver);
        friend class PluginDriver;

        bool _initialized = false;
        audiointerface_music_t _imp;
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
        SoundPlayer(PluginDriver &driver);
        friend class PluginDriver;

        bool _initialized = false;
        audiointerface_sfx_t _imp;
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
    PluginDriver();

    DENG2_PRIVATE(d)
};

}  // namespace audio

#endif  // CLIENT_AUDIO_PLUGINDRIVER_H
