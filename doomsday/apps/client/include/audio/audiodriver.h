/** @file audiodriver.h  Logical Audio Driver Model.
 * @ingroup audio
 *
 * @authors Copyright © 2012-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef AUDIO_AUDIODRIVER_H
#define AUDIO_AUDIODRIVER_H

#ifndef __cplusplus
#  error "audiodriver.h requires C++"
#endif
#ifdef __SERVER__
#  error "audio" is not available in a SERVER build
#endif

#include "api_audiod.h"
#include "api_audiod_mus.h"
#include "api_audiod_sfx.h"
#include <de/error.h>
#include <de/string.h>

/**
 * Models a logical audio driver, suitable for both built-in drivers and plugins.
 *
 * @ingroup audio
 */
class AudioDriver
{
public:
    /// Base class for load related errors. @ingroup errors
    DE_ERROR(LoadError);

    /**
     * Logical driver status.
     */
    enum Status
    {
        Invalid,     ///< Invalid state (i.e., not yet loaded).
        Loaded,      ///< Library is loaded but not yet in use.
        Initialized  ///< Library is loaded and initialized ready for use.
    };

    /**
     * Construct a new AudioDriver (invalid until loaded).
     */
    AudioDriver();

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

    inline bool isInvalid    () const { return status() == Invalid;     }
    inline bool isLoaded     () const { return status() >= Loaded;      }
    inline bool isInitialized() const { return status() == Initialized; }

    /**
     * Load the audio driver library and import symbols.
     *
     * @note Once loaded the driver must be @ref initialized before use.
     */
    void load(const de::String &identifier);

    /**
     * Unload the audio driver
     */
    void unload();

    /**
     * Initialize the audio driver if necessary, ready for use.
     */
    void initialize();

    /**
     * Deinitialize the audio driver if necessary, so that it may be unloaded.
     */
    void deinitialize();

    /**
     * Returns the plugin library for the loaded audio driver, if any (may return
     * @c nullptr if not yet loaded, or this is a built-in driver).
     */
    de::String extensionName() const;

    static bool isAvailable(const de::String &identifier);

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
     * Returns the @em Sfx interface for the audio driver. The Sfx interface is
     * used for playback of sound effects.
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
    DE_PRIVATE(d)
};

de::String AudioDriver_GetName(audiodriverid_t id);

#endif  // AUDIO_AUDIODRIVER_H
