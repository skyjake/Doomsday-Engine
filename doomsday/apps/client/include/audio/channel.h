/** @file channel.h  Logical sound playback channel.
 * @ingroup audio
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2007-2015 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2007 Jamie Jones <jamie_jones_au@yahoo.com.au>
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

#ifndef CLIENT_AUDIO_CHANNEL_H
#define CLIENT_AUDIO_CHANNEL_H

#include "api_audiod_sfx.h"  // sfxbuffer_t
#include "world/p_object.h"  // mobj_t
#include <de/Error>
#include <de/Vector>
#include <functional>

// Channel flags.
#define SFXCF_NO_ORIGIN         ( 0x1 )  ///< Sound is coming from a mystical emitter.
#define SFXCF_NO_ATTENUATION    ( 0x2 )  ///< Sound is very, very loud.
#define SFXCF_NO_UPDATE         ( 0x4 )  ///< Channel update is skipped.

namespace audio {

/**
 * Models a logical sound playback channel (for sound effects).
 */
class Channel
{
public:
    /// No sound buffer is assigned to the channel. @ingroup errors
    DENG2_ERROR(MissingBufferError);

public:
    Channel();
    virtual ~Channel();

    /**
     * Determines whether a sound buffer is assigned to the channel.
     */
    bool hasBuffer() const;

    /**
     * Returns the sound buffer assigned to the channel.
     */
    sfxbuffer_t       &buffer();
    sfxbuffer_t const &buffer() const;
    void setBuffer(sfxbuffer_t *newBuffer);

    void releaseBuffer();

    /**
     * Stop any sound currently playing on the channel.
     * @note Just stopping a channel doesn't affect refresh!
     */
    void stop();

    int flags() const;
    void setFlags(int newFlags);

    /**
     * Returns the current sound frequency adjustment: 1.0 is normal.
     */
    float frequency() const;
    void setFrequency(float newFrequency);

    /**
     * Returns the current channel volume: 1.0 is max.
     */
    float volume() const;
    void setVolume(float newVolume);

    /**
     * Returns the attributed sound-emitter if any (may be @c nullptr).
     */
    struct mobj_s *emitter() const;
    void setEmitter(struct mobj_s *newEmitter);

    void setFixedOrigin(de::Vector3d const &newOrigin);

    /**
     * Calculate priority points for sound currently playing on the channel. These
     * points are used to determine which sounds can be overridden by new sounds.
     * Zero is the lowest priority.
     */
    float priority() const;

    /**
     * Updates the channel properties based on 2D/3D position calculations. Listener
     * may be @c nullptr. Sounds emitted from the listener object are considered to
     * be inside the listener's head.
     */
    void updatePriority();

    int startTime() const;
    void setStartTime(int newStartTime);

private:
    DENG2_PRIVATE(d)
};

/**
 * Implements high-level logic for managing a set of logical sound channels.
 */
class Channels
{
public:
    /**
     * Construct a new (empty) sound Channel set.
     */
    Channels();
    virtual ~Channels();

    /**
     * Returns the total number of sound Channels.
     */
    int count() const;

    /**
     * Returns the total number of Channels currently playing a/the sound sample
     * associated with the given @a soundId.
     */
    int countPlaying(int soundId);

    /**
     * Returns @a true if at least one Channel is currently playing a/the sound sample
     * associated with the given sound @a id.
     */
    inline bool isPlaying(int id) { return countPlaying(id) > 0; }

    /**
     * Add @a newChannel to the set.
     *
     * @param newChannel  Channel to add. Ownership is given to the set. If this
     * channel is already owned by the set - nothing happens.
     *
     * @return  Same as @a newChannel, for caller convenience.
     */
    Channel &add(Channel &newChannel);

    /**
     * Attempt to find an unused Channel suitable for playing a sample with the given
     * format.
     *
     * @param use3D
     * @param bytes
     * @param rate
     * @param soundId
     */
    Channel *tryFindVacant(bool use3D, int bytes, int rate, int soundId) const;

    /**
     * Iterate through the Channels, executing @a callback for each.
     */
    de::LoopResult forAll(std::function<de::LoopResult (Channel &)> callback) const;

public:
    void refreshAll();
    void releaseAllBuffers();

private:
    DENG2_PRIVATE(d)
};

}  // namespace audio

// Debug visual: -----------------------------------------------------------------

extern int showSoundInfo;
extern byte refMonitor;

/**
 * Draws debug information on-screen.
 */
void UI_AudioChannelDrawer();

#endif  // CLIENT_AUDIO_CHANNEL_H
