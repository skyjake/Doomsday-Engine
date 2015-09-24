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

#include "audio/sound.h"
#include <de/Observers>
#include <functional>

namespace audio {

/**
 * Implements high-level logic for managing a set of logical sound channels.
 */
class Channels
{
public:
    /// Audience to be notified when the channel mapping changes.
    DENG2_DEFINE_AUDIENCE2(Remapped, void channelsRemapped(Channels &))

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
     * Add a new channel using the given @a sound.
     *
     * @param sound  Sound to create a channel for. Ownership is unaffected. As sounds
     * can only be mapped to a single channel at a time - if channel exists which is
     * already using this sound then it is returned instead.
     */
    Sound/*Channel*/ &add(Sound &sound);

    /**
     * Attempt to find a Channel suitable for playing a sample with the given format.
     *
     * @param use3D
     * @param bytes
     * @param rate
     * @param soundId
     */
    Sound/*Channel*/ *tryFindVacant(bool use3D, int bytes, int rate, int soundId) const;

    /**
     * Iterate through the Channels, executing @a callback for each.
     */
    de::LoopResult forAll(std::function<de::LoopResult (Sound/*Channel*/ &)> callback) const;

public:
    void refreshAll();

    /**
     * Enabling refresh is simple: the refresh thread is resumed. When disabling
     * refresh, first make sure a new refresh doesn't begin (using allowRefresh).
     * We still have to see if a refresh is being made and wait for it to stop.
     * Then we can suspend the refresh thread.
     */
    void allowRefresh(bool allow = true);
    void initRefresh();

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
