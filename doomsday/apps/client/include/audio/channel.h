/** @file channel.h  Audio playback channels.
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
 * Implements high-level logic for managing a set of audio playback channels.
 */
class Channels
{
public:
    /// Audience to be notified when a channel mapping change occurs.
    DENG2_DEFINE_AUDIENCE2(Remapped, void channelsRemapped(Channels &))

public:
    /**
     * Construct a new (empty) playback channel set.
     */
    Channels();

    /**
     * Returns the total number of playback channels.
     */
    de::dint count() const;

    /**
     * Returns the total number of playback channels currently playing a/the sound associated
     * with the given @a soundId and/or @a emitter.
     */
    de::dint countPlaying(de::dint soundId, SoundEmitter *emitter = nullptr) const;

    /**
     * Returns @a true if one or more playback channels are currently playing a/the sound
     * associated with the given @a soundId.
     */
    inline bool isPlaying(de::dint soundId, SoundEmitter *emitter = nullptr) const {
        return countPlaying(soundId, emitter) > 0;
    }

    /**
     * Add a new playback channel and configure with the given @a sound.
     *
     * @param sound  Sound to create a playback channel for. Ownership is unaffected. As
     * sounds can only be mapped to a single channel at a time - if a channel exists which
     * is already using this sound then @em it is returned instead.
     */
    Sound/*Channel*/ &add(Sound &sound);

    /**
     * Attempt to find a playback channel suitable for a sound with the given format.
     *
     * @param stereoPositioning  @c true= suitable for stereo sound positioning; otherwise
     * suitable for 3D sound positioning.
     *
     * @param bytesPer
     * @param rate
     *
     * @param soundId   If > 0, the channel must currently be loaded with a/the sound
     * associated with this identifier.
     */
    Sound/*Channel*/ *tryFindVacant(bool stereoPositioning, de::dint bytesPer, de::dint rate,
        de::dint soundId) const;

    /**
     * Stop all playback channels currently playing a/the sound associated with the given
     * sound @a group. If an @a emitter is specified, only stop sounds emitted by it.
     *
     * @param group    Sound group identifier.
     * @param emitter  If not @c nullptr the referenced sound's emitter must match.
     *
     * @return  Number of channels stopped.
     */
    de::dint stopGroup(de::dint group, SoundEmitter *emitter = nullptr);

    /**
     * Stop all playback channels currently playing a sound using the specified @a emitter.
     *
     * @param emitter            If not @c nullptr the sound's emitter must match; otherwise
     * stop @em all sounds using @em any emitter.
     *
     * @param clearSoundEmitter  If @c true, clear the sound -> emitter association for
     * any matching Sounds that are stopped.
     *
     * @return  Number of channels stopped.
     */
    de::dint stopWithEmitter(SoundEmitter *emitter = nullptr, bool clearEmitter = true);

    /**
     * Stop all playback channels currently playing a/the sound with a lower priority rating.
     *
     * @param soundId      If > 0, the currently playing sound must be associated with
     * this identifier; otherwise @em all sounds are stopped.
     *
     * @param emitter      If not @c nullptr the referenced sound's emitter must match.
     *
     * @param defPriority  If >= 0, the currently playing sound must have a lower priority
     * than this to be stopped. Returns -1 if the sound @a id has a lower priority than
     * a currently playing sound.
     *
     * @return  Number of channels stopped.
     */
    de::dint stopWithLowerPriority(de::dint soundId, SoundEmitter *emitter, de::dint defPriority);

    /**
     * Iterate through the playback channels, executing @a callback for each.
     */
    de::LoopResult forAll(std::function<de::LoopResult (Sound/*Channel*/ &)> callback) const;

private:
    DENG2_PRIVATE(d)
};

}  // namespace audio

// Debug visual: ------------------------------------------------------------------------

extern int showSoundInfo;
//extern byte refMonitor;

/**
 * Draws debug information on-screen.
 */
void UI_AudioChannelDrawer();

#endif  // CLIENT_AUDIO_CHANNEL_H
