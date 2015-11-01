/** @file idriver.cpp  Interface for audio playback (a "driver").
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2007-2015 Daniel Swanson <danij@dengine.net>
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

#ifndef CLIENT_AUDIO_IDRIVER_H
#define CLIENT_AUDIO_IDRIVER_H

#ifdef __SERVER__
#  error "audio" is not available in a SERVER build
#endif

#include "audio/system.h"
#include "audio/channel.h"
#include <de/Error>
#include <de/Record>
#include <de/String>
#include <functional>

namespace audio {

/**
 * Interface for an audio component which provides audio playback on the local system.
 *
 * @ingroup audio
 */
class IDriver
{
public:
    /// Base class for property read errors. @ingroup errors
    DENG2_ERROR(ReadPropertyError);

    /// Base class for property write errors. @ingroup errors
    DENG2_ERROR(WritePropertyError);

    /**
     * Logical driver statuses.
     */
    enum Status
    {
        Loaded,      ///< Driver is loaded but not yet in use.
        Initialized  ///< Driver is loaded and initialized ready for use.
    };

    /**
     * Implementers of this interface are expected to automatically @ref deinitialize()
     * before this is called.
     */
    virtual ~IDriver() {}

    DENG2_AS_IS_METHODS()

    /// Returns a reference to the singleton audio::System instance.
    static inline System &audioSystem() { return System::get(); }

    inline bool isInitialized() const { return status() == Initialized; }

    /**
     * Returns the logical driver status.
     *
     * @see statusAsText()
     */
    virtual Status status() const = 0;

    /**
     * Returns a human-friendly, textual description of the current, high-level logical
     * status of the driver.
     *
     * @see status()
     */
    de::String statusAsText() const;

    /**
     * Returns detailed information about the driver as styled text. Printed by the
     * "inspectaudiodriver" console command, for instance.
     *
     * @see identityKey(), title(), statusAsText()
     */
    de::String description() const;

    /**
     * Initialize the driver if necessary, ready for use.
     */
    virtual void initialize() = 0;

    /**
     * Deinitialize the driver if necessary, so that it may be unloaded.
     */
    virtual void deinitialize() = 0;

    /**
     * Returns the textual, symbolic identifier of the audio driver (lower case), for
     * use in Config.
     *
     * @note An audio driver may have multiple identifiers, in which case they will
     * be returned here and delimited with ';' characters.
     *
     * @todo Once the driver configuration is stored persistently in Config we should
     * remove the alternative identifiers at this time. -ds
     *
     * @see title()
     */
    virtual de::String identityKey() const = 0;

    /**
     * Returns the human-friendly title of the audio driver.
     *
     * @see description(), identityKey()
     */
    virtual de::String title() const = 0;

    /**
     * Perform any initialization necessary before playback can begin.
     */
    virtual void initInterface(de::String const &identityKey) {}

    /**
     * Perform any deinitializaion necessary to end playback before the driver is unloaded.
     */
    virtual void deinitInterface(de::String const &identityKey) {}

    /**
     * Returns a listing of the logical playback interfaces implemented by the driver.
     * It is irrelevant whether said interfaces are presently available.
     *
     * Naturally, this means the driver must support interface enumeration @em before
     * driver initialization. The driver and/or interface may still fail to initialize
     * later, though.
     *
     * Each interface record must contain at least the following required elements:
     *
     * - (TextValue)"identityKey"   : Driver-unique, textual, symbolic identifier for
     *   the player interface (lowercase), for use in Config.
     *
     * - (NumberValue)"channelType" : Channel::Type identifier.
     *
     * @todo The playback interface could also declare which audio formats it is capable
     * of playing (e.g., MIDI only, CD tracks only). -jk
     */
    virtual QList<de::Record> listInterfaces() const = 0;

public:  //- Playback Channels: ---------------------------------------------------------

    /**
     * Construct a new playback Channel of the given @a type (note: ownership is retained).
     */
    virtual Channel *makeChannel(Channel::Type type) = 0;

    /**
     * Iterate through available playback Channels of the given @a type, and execute
     * @a callback for each.
     */
    virtual de::LoopResult forAllChannels(Channel::Type type,
        std::function<de::LoopResult (Channel const &)> callback) const = 0;

    /**
     * Called by the audio::System to temporarily enable/disable refreshing of sound data
     * buffers in order to perform a critical task which operates on the current state of
     * that data.
     *
     * For example, when selecting a Channel on which to playback a sound it is imperative
     * the Channel states do not change while doing so.
     *
     * @todo Belongs at channel/buffer level. -ds
     */
    virtual void allowRefresh(bool allow = true) = 0;
};

}  // namespace audio

#endif  // CLIENT_AUDIO_IDRIVER_H
