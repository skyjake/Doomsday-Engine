/** @file sfxchannel.h  Logical sound channel (for sound effects).
 * @ingroup audio
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef CLIENT_AUDIO_SFXCHANNEL_H
#define CLIENT_AUDIO_SFXCHANNEL_H

#ifndef __cplusplus
#  error "sfxchannel.h requires C++"
#endif

#include "api_audiod_sfx.h"  // sfxbuffer_t
#include "world/p_object.h"  // mobj_t
#include <de/error.h>
#include <de/vector.h>
#include <functional>

// Channel flags.
#define SFXCF_NO_ORIGIN         ( 0x1 )  ///< Sound is coming from a mystical emitter.
#define SFXCF_NO_ATTENUATION    ( 0x2 )  ///< Sound is very, very loud.
#define SFXCF_NO_UPDATE         ( 0x4 )  ///< Channel update is skipped.

namespace audio {

/**
 * Models a logical sound channel (for sound effects).
 */
class SfxChannel
{
public:
    /// No sound buffer is assigned to the channel. @ingroup errors
    DE_ERROR(MissingBufferError);

public:
    SfxChannel();
    ~SfxChannel();

    /**
     * Determines whether a sound buffer is assigned to the channel.
     */
    bool hasBuffer() const;

    /**
     * Returns the sound buffer assigned to the channel.
     */
    sfxbuffer_t       &buffer();
    const sfxbuffer_t &buffer() const;
    void setBuffer(sfxbuffer_t *newBuffer);

    /**
     * Stop any sound currently playing on the channel.
     * @note Just stopping a sound buffer doesn't affect refresh.
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
     * Returns the current sound volume: 1.0 is max.
     */
    float volume() const;
    void setVolume(float newVolume);

    /**
     * Returns the attributed sound emitter if any (may be @c nullptr).
     */
    const struct mobj_s *emitter() const;
    void setEmitter(const struct mobj_s *newEmitter);

    void setFixedOrigin(const de::Vec3d &newOrigin);

    /**
     * Returns the @em effective origin point in the soundstage for the channel. This point
     * is used to apply positional audio effects during playback (both 3D and stereo models).
     */
    de::Vec3d origin() const;

    /**
     * Calculate priority points for a sound playing on the channel. They are used to determine
     * which sounds can be cancelled by new sounds. Zero is the lowest priority.
     */
    float priority() const;

    /**
     * Updates the channel properties based on 2D/3D position calculations. Listener may be
     * @c nullptr. Sounds emitted from the listener object are considered to be inside the
     * listener's head.
     */
    void updatePriority();

    int startTime() const;
    void setStartTime(int newStartTime);

private:
    DE_PRIVATE(d)
};

/**
 * Implements high-level logic for managing a set of logical sound channels.
 */
class SfxChannels
{
public:
    /**
     * Construct a new SfxChannel set (comprising @a count channels).
     */
    SfxChannels(int count);

    /**
     * Returns the total number of channels.
     */
    int count() const;

    /**
     * Returns the total number of sound channels currently playing a/the sound sample
     * associated with the given sound @a id.
     */
    int countPlaying(int id);

    /**
     * Returns @a true if one or more sound channels is currently playing a/the sound sample
     * associated with the given sound @a id.
     */
    inline bool isPlaying(int id) { return countPlaying(id) > 0; }

    /**
     * Attempt to find an unused SfxChannel suitable for playing a sound sample.
     *
     * @param use3D
     * @param bytes
     * @param rate
     * @param sampleId
     */
    SfxChannel *tryFindVacant(bool use3D, int bytes, int rate, int sampleId) const;

    void refreshAll();

    /**
     * Iterate through the channels making a callback for each.
     *
     * @param func  Callback to make for each SfxChannel.
     */
    de::LoopResult forAll(const std::function<de::LoopResult (SfxChannel &)>& func) const;

private:
    DE_PRIVATE(d)
};

}  // namespace audio

// Debug visual: -----------------------------------------------------------------

extern int showSoundInfo;
extern byte refMonitor;

/**
 * Draws debug information on-screen.
 */
void Sfx_ChannelDrawer();

#endif  // CLIENT_AUDIO_SFXCHANNEL_H
