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

#include "audio/system.h"
#include "world/thinkers.h"
#include "def_main.h"        // SF-* flags, remove me
#include <de/Log>
#include <de/timer.h>        // TICSPERSEC
#include <de/vector1.h>      // remove me

using namespace de;

namespace audio {

DENG2_PIMPL_NOREF(Sound)
{
    dint flags = 0;                 ///< SFXCF_* flags.
    dfloat frequency = 0;           ///< Frequency adjustment: 1.0 is normal.
    dfloat volume = 0;              ///< Sound volume: 1.0 is max.

    mobj_t *emitter = nullptr;      ///< Mobj emitter for the sound, if any (not owned).
    coord_t origin[3];              ///< Emit from here (synced with emitter).

    sfxbuffer_t *buffer = nullptr;  ///< Assigned sound buffer, if any (not owned).
    dint startTime = 0;             ///< When the assigned sound sample was last started.

    Instance() { zap(origin); }

    /// @todo Sounds should be constructed by an OO interface wrapper class, which can
    /// associate the managed buffer with a reference to the Driver that manages it -ds
    audiointerface_sfx_t &sfx() const
    {
        DENG2_ASSERT(System::get().sfx() != nullptr);
        return *(audiointerface_sfx_t *) System::get().sfx();
    }

    /**
     * @param property  Buffer property:
     *              - SFXBP_VOLUME (if negative, interpreted as attenuation)
     *              - SFXBP_FREQUENCY
     *              - SFXBP_PAN (-1..1)
     *              - SFXBP_MIN_DISTANCE
     *              - SFXBP_MAX_DISTANCE
     *              - SFXBP_RELATIVE_MODE
     * @param value Value for the property.
     */
    void setBufferProperty(dint prop, dfloat value)
    {
        if(!buffer) return;
        sfx().gen.Set(buffer, prop, value);
    }

    /**
     * Coordinates specified in world coordinate system, converted to DSound's:
     * +X to the right, +Y up and +Z away (Y and Z swapped, i.e.).
     *
     * @param property  - SFXBP_POSITION
     *                  - SFXBP_VELOCITY
     */
    void setBufferPropertyv(dint prop, dfloat *values)
    {
        if(!buffer) return;
        sfx().gen.Setv(buffer, prop, values);
    }
};

Sound::Sound() : d(new Instance)
{}

Sound::~Sound()
{}

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

void Sound::setBuffer(sfxbuffer_t *newBuffer)
{
    releaseBuffer();
    d->buffer = newBuffer;
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

dfloat Sound::frequency() const
{
    return d->frequency;
}

void Sound::setFrequency(dfloat newFrequency)
{
    d->frequency = newFrequency;
}

dfloat Sound::volume() const
{
    return d->volume;
}

void Sound::setVolume(dfloat newVolume)
{
    d->volume = newVolume;
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
    d->origin[0] = newOrigin.x;
    d->origin[1] = newOrigin.y;
    d->origin[2] = newOrigin.z;
}

dfloat Sound::priority() const
{
    if(!d->buffer || !(d->buffer->flags & SFXBF_PLAYING))
        return SFX_LOWEST_PRIORITY;

    if(d->flags & SFXCF_NO_ORIGIN)
        return System::get().rateSoundPriority(0, 0, d->volume, d->startTime);

    // d->origin is set to emitter->xyz during updates.
    return System::get().rateSoundPriority(0, d->origin, d->volume, d->startTime);
}

void Sound::updateBuffer()
{
    // If no sound buffer is assigned we've no need to update.
    sfxbuffer_t *sbuf = d->buffer;
    if(!sbuf) return;

    // Disabled?
    if(d->flags & SFXCF_NO_UPDATE) return;

    // If we know the emitter update our origin info.
    if(mobj_t *emitter = d->emitter)
    {
        d->origin[0] = emitter->origin[0];
        d->origin[1] = emitter->origin[1];
        d->origin[2] = emitter->origin[2];

        // If this is a mobj, center the Z pos.
        if(Thinker_IsMobjFunc(emitter->thinker.function))
        {
            // Sounds originate from the center.
            d->origin[2] += emitter->height / 2;
        }
    }

    // Frequency is common to both 2D and 3D sounds.
    d->setBufferProperty(SFXBP_FREQUENCY, d->frequency);

    if(sbuf->flags & SFXBF_3D)
    {
        // Volume is affected only by maxvol.
        d->setBufferProperty(SFXBP_VOLUME, d->volume * System::get().soundVolume() / 255.0f);
        if(d->emitter && d->emitter == System::get().sfxListener())
        {
            // Emitted by the listener object. Go to relative position mode
            // and set the position to (0,0,0).
            dfloat vec[3]; vec[0] = vec[1] = vec[2] = 0;
            d->setBufferProperty(SFXBP_RELATIVE_MODE, true);
            d->setBufferPropertyv(SFXBP_POSITION, vec);
        }
        else
        {
            // Use the channel's map space origin.
            dfloat origin[3];
            V3f_Copyd(origin, d->origin);
            d->setBufferProperty(SFXBP_RELATIVE_MODE, false);
            d->setBufferPropertyv(SFXBP_POSITION, origin);
        }

        // If the sound is emitted by the listener, speed is zero.
        if(d->emitter && d->emitter != System::get().sfxListener() &&
           Thinker_IsMobjFunc(d->emitter->thinker.function))
        {
            dfloat vec[3];
            vec[0] = d->emitter->mom[0] * TICSPERSEC;
            vec[1] = d->emitter->mom[1] * TICSPERSEC;
            vec[2] = d->emitter->mom[2] * TICSPERSEC;
            d->setBufferPropertyv(SFXBP_VELOCITY, vec);
        }
        else
        {
            // Not moving.
            dfloat vec[3]; vec[0] = vec[1] = vec[2] = 0;
            d->setBufferPropertyv(SFXBP_VELOCITY, vec);
        }
    }
    else
    {
        dfloat dist = 0;
        dfloat pan  = 0;

        // This is a 2D buffer.
        if((d->flags & SFXCF_NO_ORIGIN) ||
           (d->emitter && d->emitter == System::get().sfxListener()))
        {
            dist = 1;
            pan = 0;
        }
        else
        {
            // Calculate roll-off attenuation. [.125/(.125+x), x=0..1]
            Ranged const attenRange = System::get().soundVolumeAttenuationRange();

            dist = Mobj_ApproxPointDistance(System::get().sfxListener(), d->origin);

            if(dist < attenRange.start || (d->flags & SFXCF_NO_ATTENUATION))
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
                dfloat angle = (M_PointToAngle2(listener->origin, d->origin) - listener->angle) / (dfloat) ANGLE_MAX * 360;

                // We want a signed angle.
                if(angle > 180)
                    angle -= 360;

                // Front half.
                if(angle <= 90 && angle >= -90)
                {
                    pan = -angle / 90;
                }
                else
                {
                    // Back half.
                    pan = (angle + (angle > 0 ? -180 : 180)) / 90;
                    // Dampen sounds coming from behind.
                    dist *= (1 + (pan > 0 ? pan : -pan)) / 2;
                }
            }
            else
            {
                // No listener mobj? Can't pan, then.
                pan = 0;
            }
        }

        d->setBufferProperty(SFXBP_VOLUME, d->volume * dist * System::get().soundVolume() / 255.0f);
        d->setBufferProperty(SFXBP_PAN, pan);
    }
}

dint Sound::startTime() const
{
    return d->startTime;
}

void Sound::releaseBuffer()
{
    stop();
    if(!hasBuffer()) return;

    d->sfx().gen.Destroy(d->buffer);
    setBuffer(nullptr);
}

void Sound::load(sfxsample_t *sample)
{
    d->sfx().gen.Load(d->buffer, sample);
}

void Sound::reset()
{
    d->sfx().gen.Reset(d->buffer);
}

void Sound::play()
{
    if(!d->buffer)
    {
        /// @throw MissingBufferError  Playing is obviously impossible without data to play...
        throw MissingBufferError("Sound::play", "Attempted with no data buffer assigned");
    }

    // Flush deferred property value changes to the assigned data buffer.
    updateBuffer();

    // 3D sounds need a few extra properties set up.
    if(d->buffer->flags & SFXBF_3D)
    {
        // Configure the attentuation distances.
        // This is only done once, when the sound is first played (i.e., here).
        Ranged const attenRange = System::get().soundVolumeAttenuationRange();
        d->setBufferProperty(SFXBP_MIN_DISTANCE, (d->flags & SFXCF_NO_ATTENUATION) ? 10000 : attenRange.start);
        d->setBufferProperty(SFXBP_MAX_DISTANCE, (d->flags & SFXCF_NO_ATTENUATION) ? 20000 : attenRange.end);
    }

    d->sfx().gen.Play(d->buffer);
    d->startTime = Timer_Ticks();  // Note the current time.
}

void Sound::setPlayingMode(de::dint sfFlags)
{
    if(d->buffer)
    {
        d->buffer->flags &= ~(SFXBF_REPEAT | SFXBF_DONT_STOP);
        if(sfFlags & SF_REPEAT)    d->buffer->flags |= SFXBF_REPEAT;
        if(sfFlags & SF_DONT_STOP) d->buffer->flags |= SFXBF_DONT_STOP;
    }
}

void Sound::stop()
{
    d->sfx().gen.Stop(d->buffer);
}

void Sound::refresh()
{
    d->sfx().gen.Refresh(d->buffer);
}

}  // namespace audio
