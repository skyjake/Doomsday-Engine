/** @file sound.cpp  Interface for sound playback.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2015 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2006-2007 Jamie Jones <jamie_jones_au@yahoo.com.au>
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

#include "audio/sound.h"

#include "world/thinkers.h"
#include "def_main.h"        // SF_* flags, remove me
#include <de/Log>
#include <de/Observers>
#include <de/timer.h>        // TICSPERSEC

using namespace de;

namespace audio {

/**
 * @note Loading must be done prior to setting properties, because the driver might defer
 * creation of the actual data buffer.
 */
DENG2_PIMPL_NOREF(Sound)
, DENG2_OBSERVES(System, FrameEnds)
{
    dint flags = 0;                  ///< SFXCF_* flags.
    dfloat frequency = 0;            ///< Frequency adjustment: 1.0 is normal.
    dfloat volume = 0;               ///< Sound volume: 1.0 is max.

    mobj_t *emitter = nullptr;       ///< Mobj emitter for the sound, if any (not owned).
    Vector3d origin;                 ///< Emit from here (synced with emitter).

    sfxbuffer_t *buffer = nullptr;   ///< Assigned sound buffer, if any (not owned).
    dint startTime = 0;              ///< When the assigned sound sample was last started.

    ISoundPlayer *player = nullptr;  ///< Owning player (not owned).

    inline ISoundPlayer &getPlayer()
    {
        DENG2_ASSERT(player != 0);
        return *player;
    }
    
    void updateOriginIfNeeded()
    {
        // Updating is only necessary if we are tracking an emitter.
        if(!emitter) return;

        origin = Mobj_Origin(*emitter);
        // If this is a mobj, center the Z pos.
        if(Thinker_IsMobjFunc(emitter->thinker.function))
        {
            origin.z += emitter->height / 2;
        }
    }

    /**
     * Flushes property changes to the assigned data buffer.
     *
     * @param force  Usually updates are only necessary during playback. Use
     *               @c true= to override this check and write the properties
     *               regardless.
     */
    void updateBuffer(bool force = false)
    {
        DENG2_ASSERT(buffer);

        // Disabled?
        if(flags & SFXCF_NO_UPDATE) return;

        // Updates are only necessary during playback.
        if(!getPlayer().isPlaying(*buffer) && !force) return;

        // When tracking an emitter we need the latest origin coordinates.
        updateOriginIfNeeded();

        // Frequency is common to both 2D and 3D sounds.
        getPlayer().setFrequency(*buffer, frequency);

        if(buffer->flags & SFXBF_3D)
        {
            // Volume is affected only by maxvol.
            getPlayer().setVolume(*buffer, volume * System::get().soundVolume() / 255.0f);
            if(emitter && emitter == System::get().sfxListener())
            {
                // Emitted by the listener object. Go to relative position mode
                // and set the position to (0,0,0).
                getPlayer().setPositioning(*buffer, true/*head-relative*/);
                getPlayer().setOrigin(*buffer, Vector3d());
            }
            else
            {
                // Use the channel's map space origin.
                getPlayer().setPositioning(*buffer, false/*absolute*/);
                getPlayer().setOrigin(*buffer, origin);
            }

            // If the sound is emitted by the listener, speed is zero.
            if(emitter && emitter != System::get().sfxListener() &&
               Thinker_IsMobjFunc(emitter->thinker.function))
            {
                getPlayer().setVelocity(*buffer, Vector3d(emitter->mom)* TICSPERSEC);
            }
            else
            {
                // Not moving.
                getPlayer().setVelocity(*buffer, Vector3d());
            }
        }
        else
        {
            dfloat dist = 0;
            dfloat finalPan = 0;

            // This is a 2D buffer.
            if((flags & SFXCF_NO_ORIGIN) ||
               (emitter && emitter == System::get().sfxListener()))
            {
                dist = 1;
                finalPan = 0;
            }
            else
            {
                // Calculate roll-off attenuation. [.125/(.125+x), x=0..1]
                Ranged const attenRange = System::get().soundVolumeAttenuationRange();

                dist = System::get().distanceToListener(origin);

                if(dist < attenRange.start || (flags & SFXCF_NO_ATTENUATION))
                {
                    // No distance attenuation.
                    dist = 1;
                }
                else if(dist > attenRange.end)
                {
                    // Can't be heard.
                    dist = 0;
                }
                else
                {
                    ddouble const normdist = (dist - attenRange.start) / attenRange.size();

                    // Apply the linear factor so that at max distance there
                    // really is silence.
                    dist = .125f / (.125f + normdist) * (1 - normdist);
                }

                // And pan, too. Calculate angle from listener to emitter.
                if(mobj_t *listener = System::get().sfxListener())
                {
                    dfloat angle = (M_PointXYToAngle2(listener->origin[0], listener->origin[1],
                                                      origin.x, origin.y)
                                        - listener->angle)
                                 / (dfloat) ANGLE_MAX * 360;

                    // We want a signed angle.
                    if(angle > 180)
                        angle -= 360;

                    // Front half.
                    if(angle <= 90 && angle >= -90)
                    {
                        finalPan = -angle / 90;
                    }
                    else
                    {
                        // Back half.
                        finalPan = (angle + (angle > 0 ? -180 : 180)) / 90;
                        // Dampen sounds coming from behind.
                        dist *= (1 + (finalPan > 0 ? finalPan : -finalPan)) / 2;
                    }
                }
                else
                {
                    // No listener mobj? Can't pan, then.
                    finalPan = 0;
                }
            }

            getPlayer().setVolume(*buffer, volume * dist * System::get().soundVolume() / 255.0f);
            getPlayer().setPan(*buffer, finalPan);
        }
    }

    void systemFrameEnds(System &)
    {
        updateBuffer();
    }
};

Sound::Sound(ISoundPlayer &player) : d(new Instance)
{
    d->player = &player;
}

Sound::~Sound()
{
    releaseBuffer();
}

bool Sound::hasBuffer() const
{
    return d->buffer != nullptr;
}

sfxbuffer_t const &Sound::buffer() const
{
    if(d->buffer) return *d->buffer;
    /// @throw MissingBufferError  No sound buffer is currently assigned.
    throw MissingBufferError("audio::Sound::buffer", "No data buffer is assigned");
}

void Sound::releaseBuffer()
{
    stop();
    if(!hasBuffer()) return;

    // Cancel frame notifications - we'll soon have no buffer to update.
    System::get().audienceForFrameEnds() -= d;

    d->getPlayer().destroy(*d->buffer);
    d->buffer = nullptr;
}

void Sound::setBuffer(sfxbuffer_t *newBuffer)
{
    releaseBuffer();
    d->buffer = newBuffer;

    if(d->buffer)
    {
        // We want notification when the frame ends in order to flush deferred property writes.
        System::get().audienceForFrameEnds() += d;
    }
}

dint Sound::flags() const
{
    return d->flags;
}

/// @todo Use QFlags -ds
void Sound::setFlags(dint newFlags)
{
    d->flags = newFlags;
}

mobj_t *Sound::emitter() const
{
    return d->emitter;
}

void Sound::setEmitter(mobj_t *newEmitter)
{
    d->emitter = newEmitter;
}

void Sound::setFixedOrigin(Vector3d const &newOrigin)
{
    d->origin = newOrigin;
}

dfloat Sound::priority() const
{
    if(!isPlaying())
        return SFX_LOWEST_PRIORITY;

    if(d->flags & SFXCF_NO_ORIGIN)
        return System::get().rateSoundPriority(0, 0, d->volume, d->startTime);

    // d->origin is set to emitter->xyz during updates.
    ddouble origin[3]; d->origin.decompose(origin);
    return System::get().rateSoundPriority(0, origin, d->volume, d->startTime);
}

void Sound::load(sfxsample_t &sample)
{
    if(!d->buffer)
    {
        /// @throw MissingBufferError  Loading is impossible without a buffer...
        throw MissingBufferError("Sound::load", "Attempted with no data buffer assigned");
    }

    // Don't reload if a sample with the same sound ID is already loaded.
    if(!d->buffer->sample || d->buffer->sample->soundId != sample.soundId)
    {
        d->getPlayer().load(*d->buffer, sample);
    }
}

void Sound::stop()
{
    d->getPlayer().stop(*d->buffer);
}

void Sound::reset()
{
    d->getPlayer().reset(*d->buffer);
}

void Sound::play()
{
    if(!d->buffer)
    {
        /// @throw MissingBufferError  Playing is obviously impossible without data to play...
        throw MissingBufferError("Sound::play", "Attempted with no data buffer assigned");
    }

    // Flush deferred property value changes to the assigned data buffer.
    d->updateBuffer(true/*force*/);

    // 3D sounds need a few extra properties set up.
    if(d->buffer->flags & SFXBF_3D)
    {
        // Configure the attentuation distances.
        // This is only done once, when the sound is first played (i.e., here).
        if(d->flags & SFXCF_NO_ATTENUATION)
        {
            d->getPlayer().setVolumeAttenuationRange(*d->buffer, Ranged(10000, 20000));
        }
        else
        {
            d->getPlayer().setVolumeAttenuationRange(*d->buffer, System::get().soundVolumeAttenuationRange());
        }
    }

    d->getPlayer().play(*d->buffer);
    d->startTime = Timer_Ticks();  // Note the current time.
}

void Sound::setPlayingMode(dint sfFlags)
{
    if(d->buffer)
    {
        d->buffer->flags &= ~(SFXBF_REPEAT | SFXBF_DONT_STOP);
        if(sfFlags & SF_REPEAT)    d->buffer->flags |= SFXBF_REPEAT;
        if(sfFlags & SF_DONT_STOP) d->buffer->flags |= SFXBF_DONT_STOP;
    }
}

dint Sound::startTime() const
{
    return d->startTime;
}

Sound &Sound::setFrequency(dfloat newFrequency)
{
    d->frequency = newFrequency;
    return *this;
}

Sound &Sound::setVolume(dfloat newVolume)
{
    d->volume = newVolume;
    return *this;
}

bool Sound::isPlaying() const
{
    return d->buffer && d->getPlayer().isPlaying(*d->buffer);
}

dfloat Sound::frequency() const
{
    return d->frequency;
}

dfloat Sound::volume() const
{
    return d->volume;
}

void Sound::refresh()
{
    d->getPlayer().refresh(*d->buffer);
}

}  // namespace audio
