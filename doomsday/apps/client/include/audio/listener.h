/** @file listener.h  Logical model of "listener" in an audio sound "stage".
 * @ingroup audio
 *
 * @authors Copyright © 2015 Daniel Swanson <danij@dengine.net>
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

#ifndef CLIENT_AUDIO_LISTENER_H
#define CLIENT_AUDIO_LISTENER_H

#include "audio/environment.h"

#include "world/p_object.h"  // mobj_s
#include <de/Observers>
#include <de/Range>
#include <de/Vector>

namespace audio {

/**
 * @ingroup audio
 */
class Listener
{
public:
    /// Audience to be notified when the listener is about to be deleted.
    DENG2_DEFINE_AUDIENCE2(Deletion, void listenerBeingDeleted(Listener const &))

    /// Audience to be notified whenever the effective Environment changes.
    DENG2_DEFINE_AUDIENCE2(EnvironmentChange, void listenerEnvironmentChanged(Listener &));

public:
    Listener();
    virtual ~Listener();

    /**
     * Register the console commands and variables of this module.
     */
    static void consoleRegister();

    /**
     * Toggle the use of audio::Environment characteristics for the listener according to
     * @a enableOrDisable, notifying the EnvironmentChange audience if changed.
     */
    void useEnvironment(bool enableOrDisable);

    /**
     * Returns the effective audio::Environment at the listener's current position. Whenever
     * an environment change occurs the EnvironmentChange audience is notified.
     */
    Environment environment();

    /**
     * Determines the world space angle in degrees [0..360) between the listener and the
     * given @a point.
     *
     * If no map-object is currently being tracked the listener is understood to be at a
     * fixed (x:0, y:0) origin for the purposes of this calculation.
     *
     * @see distanceFrom()
     */
    de::dfloat angleFrom(de::Vector3d const &point) const;

    /**
     * Determines the world space distance (in map units) from the given @a point to the
     * listener. If no map-object is currently being tracked then @c 0 is returned instead.
     *
     * @see position(), inAudibleRangeOf(), angleFrom()
     */
    de::ddouble distanceFrom(de::Vector3d const &point) const;

    /**
     * Convenient method for determining whether the given @a point is within the audible
     * range of the listener (@ref volumeAttenuationRange()). If no map-object is currently
     * being tracked then @c true is always returned.
     *
     * @see distanceFrom()
     */
    inline bool inAudibleRangeOf(de::Vector3d const &point) const {
        return distanceFrom(point) < volumeAttenuationRange().end;
    };

    /**
     * Returns the orientation of the listener in world space as a 2D vector (0:yaw, 1:pitch),
     * where (0, 0) yields front=(1, 0, 0) and up=(0, 0, 1).
     *
     * @see position(), velocity()
     */
    de::Vector2d orientation() const;

    /**
     * Returns the 3D world space position (@em eye-level) of the listener.
     *
     * @see orientation(), velocity()
     */
    de::Vector3d position() const;

    /**
     * Returns the 3D world space velocity (world units per second) of the listener.
     *
     * @see orientation(), position()
     */
    de::Vector3d velocity() const;

    /**
     * Convenient method returning the current sound volume attenuation range. Note that
     * this is the @em attenuated subsection of the audible range (which actually starts
     * at zero). Any sounds nearer than the start of the attenuated range will be played
     * at full volume.
     *
     * @see inVolumeAttenuationRangeOf()
     */
    de::Ranged volumeAttenuationRange() const;

public:
    /**
     * Returns the world map-object being tracked, if any (may return @c nullptr).
     *
     * @see distanceFrom()
     */
    struct mobj_s const *trackedMapObject() const;

    /**
     * Change the world map-object being tracked to @a mobToTrack. Use @c nullptr to clear.
     */
    void setTrackedMapObject(struct mobj_s *mobToTrack);

    inline void stopTracking() { setTrackedMapObject(nullptr); }

    /// @todo make private. -ds
    void requestEnvironmentUpdate();

private:
    DENG2_PRIVATE(d)
};

}  // namespace audio

#endif  // CLIENT_AUDIO_LISTENER_H
