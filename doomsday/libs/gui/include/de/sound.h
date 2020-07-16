/** @file sound.h  Base class for playable audio waveforms.
 *
 * @authors Copyright (c) 2014-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBGUI_SOUND_H
#define LIBGUI_SOUND_H

#include <de/id.h>
#include <de/observers.h>
#include <de/vector.h>

#include "de/libgui.h"

namespace de {

/**
 * Abstract base class for a playing sound. Provides methods for controlling how the
 * sound is played, and querying the status of the sound.
 *
 * After creation, sounds are in a paused state, after which they can be configured with
 * volume, frequency, etc. and started in either looping or non-looping mode.
 *
 * By default, sounds use 2D stereo positioning. 3D positioning is enabled when
 * calling Sound::setPosition().
 *
 * @ingroup audio
 */
class LIBGUI_PUBLIC Sound
{
public:
    Sound();

    /**
     * Sounds are automatically stopped when deleted.
     */
    virtual ~Sound() {}

    enum PlayingMode {
        NotPlaying,
        Once,           ///< Play once and then delete the sound.
        OnceDontDelete, ///< Play once then pause the sound.
        Looping         ///< Keep looping the sound.
    };

    enum Positioning {
        Stereo,         ///< Simple 2D stereo, not 3D.
        Absolute,
        HeadRelative
    };

    // Methods for controlling the sound:

    /**
     * Starts playing the sound. If the sound is already playing, does nothing:
     * to change the playing mode, one has to first stop the sound.
     *
     * The implementation must notify the Play audience.
     *
     * @param mode  Playing mode:
     *              - Sound::Once: Play once, after which the sound gets automatically
     *                deleted. The caller is expected to observe the Sound instance
     *                for deletion or not retain a reference to it.
     *              - Sound::OnceDontDelete: Play once, after which the sound remains
     *                in a paused state. The sound can then be restarted later.
     *              - Sound::Looping: Play and keep looping indefinitely.
     */
    virtual void play(PlayingMode mode = Once) = 0;

    virtual void stop() = 0;
    virtual void pause() = 0;
    virtual void resume() = 0;

    virtual Sound &setVolume(dfloat volume);
    virtual Sound &setPan(dfloat pan);
    virtual Sound &setFrequency(dfloat factor);
    virtual Sound &setPosition(const Vec3f &position, Positioning positioning = Absolute);
    virtual Sound &setVelocity(const Vec3f &velocity);
    virtual Sound &setMinDistance(dfloat minDistance);
    virtual Sound &setSpatialSpread(dfloat degrees);

    // Methods for querying the sound status:

    virtual PlayingMode mode() const = 0;
    virtual bool isPaused() const = 0;

    virtual bool isPlaying() const;
    virtual dfloat volume() const;
    virtual dfloat pan() const;
    virtual dfloat frequency() const;
    virtual Positioning positioning() const;
    virtual Vec3f position() const;
    virtual Vec3f velocity() const;
    virtual dfloat minDistance() const;
    virtual dfloat spatialSpread() const;

    DE_CAST_METHODS()

    /// Audience that is notified when the sound is played.
    DE_AUDIENCE(Play, void soundPlayed(const Sound &))

    /// Audience that is notified when the properties of the sound change.
    DE_AUDIENCE(Change, void soundPropertyChanged(const Sound &))

    /// Audience that is notified when the sound stops.
    DE_AUDIENCE(Stop, void soundStopped(Sound &))

    /// Audience that is notified when the sound instance is deleted.
    DE_AUDIENCE(Deletion, void soundBeingDeleted(Sound &))

protected:
    /// Called after a property value has been changed.
    virtual void update() = 0;

private:
    DE_PRIVATE(d)
};

} // namespace de

#endif // LIBGUI_SOUND_H
