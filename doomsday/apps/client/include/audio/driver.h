/** @file driver.h  Logical audio driver (model).
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

#ifndef CLIENT_AUDIO_DRIVER_H
#define CLIENT_AUDIO_DRIVER_H

#ifdef __SERVER__
#  error "audio" is not available in a SERVER build
#endif

#include "api_audiod.h"
#include "api_audiod_mus.h"
#include "api_audiod_sfx.h"
#include <doomsday/library.h>
#include <de/Error>
#include <de/String>

namespace audio {

/**
 * Models a logical audio driver, suitable for both built-in drivers and plugins.
 */
class Driver
{
public:
    /// Loaded driver returned "not successful" when trying to read a property. @ingroup errors
    DENG2_ERROR(ReadPropertyError);

    /// Loaded driver returned "not successful" when trying to write a property. @ingroup errors
    DENG2_ERROR(WritePropertyError);

    /**
     * Logical driver status.
     */
    enum Status
    {
        Loaded,      ///< Library is loaded but not yet in use.
        Initialized  ///< Library is loaded and initialized ready for use.
    };

    /**
     * If the driver is still initialized it will be automatically deinitialized
     * when this is called, before then unloading the underlying Library.
     */
    virtual ~Driver();

    /**
     * Returns @c true if the given @a library appears to be useable with Driver.
     *
     * @see newFromLibrary()
     */
    static bool recognize(de::LibraryFile &library);

    /**
     * Attempt to load a new Driver from the given @a library.
     *
     * @return  Pointer to a new Driver instance if successful. Ownership is given
     * to the caller.
     *
     * @see recognize()
     */
    static Driver *newFromLibrary(de::LibraryFile &library);

    /**
     * Returns the textual, symbolic identifier of the audio driver (lower case),
     * for use in Config.
     *
     * @note An audio driver may be have multiple identifiers, in which case they
     * will be returned here and delimited with ';' characters.
     *
     * @todo Once the audio driver/interface configuration is stored persistently
     * in Config we should remove the alternative identifiers at this time. -ds
     */
    de::String identifier() const;

    /**
     * Returns the human-friendly name of the audio driver if loaded; otherwise a
     * zero-length string is returned.
     */
    de::String name() const;

    /**
     * Returns the logical driver status.
     */
    Status status() const;

    /**
     * Returns a human-friendly, textual description of the logical driver status.
     */
    de::String statusAsText() const;

    inline bool isLoaded     () const { return status() >= Loaded;      }
    inline bool isInitialized() const { return status() == Initialized; }

    /**
     * Initialize the audio driver if necessary, ready for use.
     */
    void initialize();

    /**
     * Deinitialize the audio driver if necessary, so that it may be unloaded.
     */
    void deinitialize();

    /**
     * Returns the plugin library for the loaded audio driver (may return
     * @c nullptr if this is a built-in driver).
     */
    ::Library *library() const;

public:  // Interfaces: -----------------------------------------------------------

    /**
     * Returns the @em Base interface for the audio driver. The Base interface is
     * used for high-level tasks such as (de)initializing the audio driver.
     */
    audiodriver_t /*const*/ &iBase() const;

    /// Returns @c true if the audio driver provides @em Sfx playback.
    bool hasSfx() const;

    /// Returns @c true if the audio driver provides @em Music playback.
    bool hasMusic() const;

    /// Returns @c true if the audio driver provides @em CD playback.
    bool hasCd() const;

    /**
     * Returns the @em Sfx interface for the audio driver. The Sfx interface is used
     * for playback of sound effects.
     */
    audiointerface_sfx_t /*const*/ &iSfx() const;

    /**
     * Returns the @em Music interface for the audio driver. The Music interface is
     * used for playback of music (i.e., complete songs).
     */
    audiointerface_music_t /*const*/ &iMusic() const;

    /**
     * Returns the @em CD interface for the audio driver. The CD interface is used
     * for playback of music by streaming it from a compact disk.
     */
    audiointerface_cd_t /*const*/ &iCd() const;

    /**
     * Returns the human-friendly name for @a anyAudioInterface.
     */
    de::String interfaceName(void *anyAudioInterface) const;

private:
    Driver();

    DENG2_PRIVATE(d)
};

}  // namespace audio

#endif  // CLIENT_AUDIO_DRIVER_H
