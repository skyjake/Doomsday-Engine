/** @file channel.h  Logical sound playback channel.
 * @ingroup audio
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2007-2015 Daniel Swanson <danij@dengine.net>
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
     * Construct a new (empty) sound channel set.
     */
    Channels();

    /**
     * Returns the total number of sound channels.
     */
    de::dint count() const;

    /**
     * Returns the total number of channels currently playing a/the sound sample
     * associated with the given @a soundId.
     */
    de::dint countPlaying(de::dint soundId) const;

    /**
     * Returns @a true if at least one channel is currently playing a/the sound sample
     * associated with the given @a soundId.
     */
    inline bool isPlaying(de::dint soundId) const { return countPlaying(soundId) > 0; }

    /**
     * Add a new channel using the given @a sound.
     *
     * @param sound  Sound to create a channel for. Ownership is unaffected. As sounds
     * can only be mapped to a single channel at a time - if channel exists which is
     * already using this sound then it is returned instead.
     */
    Sound/*Channel*/ &add(Sound &sound);

    /**
     * Attempt to find a channel suitable for playing a sample with the given format.
     *
     * @param use3D
     * @param bytes
     * @param rate
     * @param soundId
     */
    Sound/*Channel*/ *tryFindVacant(bool use3D, de::dint bytes, de::dint rate, de::dint soundId) const;

    /**
     * Iterate through the channels, executing @a callback for each.
     */
    de::LoopResult forAll(std::function<de::LoopResult (Sound/*Channel*/ &)> callback) const;

private:
    DENG2_PRIVATE(d)
};

}  // namespace audio

// Debug visual: -----------------------------------------------------------------

extern int showSoundInfo;
//extern byte refMonitor;

/**
 * Draws debug information on-screen.
 */
void UI_AudioChannelDrawer();

#endif  // CLIENT_AUDIO_CHANNEL_H
