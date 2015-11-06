/** @file stage.h  Logically independent audio context (a sound "stage").
 * @ingroup audio
 *
 * @authors Copyright © 2015 Daniel Swanson <danij@dengine.net>
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

#ifndef CLIENT_AUDIO_STAGE_H
#define CLIENT_AUDIO_STAGE_H

#include "audio/sound.h"
#include <de/Observers>

namespace audio {

class Listener;

/**
 * Model of a logically independent audio context (or sound "stage").
 *
 * Marshalls concurrent playback on a purely logical level, providing tracking of all the
 * currently playing sounds and enforcing start/stop/etc.. behaviors and polices.
 *
 * @ingroup audio
 */
class Stage
{
public:
    /**
     * Concurrent Sound exclusion policy:
     */
    enum Exclusion
    {
        DontExclude,   ///< All are welcome.
        OnePerEmitter  ///< Only one per SoundEmitter (others will be removed).
    };

    /// Audience that is notified when a new sound is added/started.
    DENG2_DEFINE_AUDIENCE2(Addition, void stageSoundAdded(Stage &, Sound &))

public:
    Stage(Exclusion exclusion = DontExclude);
    virtual ~Stage();

    /**
     * Returns the current Sound exclusion policy.
     *
     * @see setExclusion()
     */
    Exclusion exclusion() const;

    /**
     * Change the Sound exclusion policy to @a newBehavior. Note that calling this will
     * @em not affect any currently playing sounds, which persist until they are purged
     * (because they've stopped) or manually removed.
     *
     * @see exclusion()
     */
    void setExclusion(Exclusion newBehavior);

    /**
     * Provides access to the sound stage Listener.
     */
    Listener       &listener();
    Listener const &listener() const;

    /**
     * Returns true if the referenced sound is currently playing somewhere in the stage.
     * (irrespective of whether it is actually audible, or not).
     *
     * @param effectId  @c 0= true if sounds are playing using the specified @a emitter.
     * @param emitter   Soundstage Emitter (originator). May be @c nullptr.
     */
    bool soundIsPlaying(de::dint effectId, SoundEmitter *emitter) const;

    /**
     * Start playing a Sound in the soundstage. The Addition audience is notified whenever
     * a Sound is added to the stage.
     *
     * @param params   Parameters of the sound-effect to play.
     * @param emitter  Emitter to attribute the sound to, if any.
     *
     * @see soundIsPlaying()
     */
    void playSound(SoundParams params, SoundEmitter *emitter = nullptr);

    /**
     * @see removeSoundsById(), removeSoundsWithEmitter()
     */
    void removeAllSounds();

    /**
     * @param effectId
     *
     * @see removeSoundsWithEmitter(), removeAllSounds()
     */
    void removeSoundsById(de::dint effectId);

    /**
     * Remove all Sounds originating from the given @a emitter. Does absolutely nothing
     * about playback (or refresh), as that is handled at another (lower) level.
     *
     * @param emitter
     *
     * @see removeSoundsById(), removeAllSounds()
     * @note Performance: O(1)
     */
    void removeSoundsWithEmitter(SoundEmitter const &emitter);

    /**
     * Remove stopped (logical) Sounds @em if a purge is due.
     * @todo make private. -ds
     */
    void maybeRunSoundPurge();

private:
    DENG2_PRIVATE(d)
};

}  // namespace audio

#endif  // CLIENT_AUDIO_STAGE_H
