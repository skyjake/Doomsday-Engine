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

public:  // Implements audio::System::IDriver: -----------------------------------

    void initialize();
    void deinitialize();

    Status status() const;

    de::String description() const;
    de::String identityKey() const;
    de::String title() const;

    bool hasCd() const;
    bool hasMusic() const;
    bool hasSfx() const;

    audiointerface_cd_t /*const*/ &iCd() const;
    audiointerface_music_t /*const*/ &iMusic() const;
    audiointerface_sfx_t /*const*/ &iSfx() const;
    de::String interfaceName(void *playbackInterface) const;

private:
    PluginDriver();

    DENG2_PRIVATE(d)
};

}  // namespace audio

#endif  // CLIENT_AUDIO_PLUGINDRIVER_H
