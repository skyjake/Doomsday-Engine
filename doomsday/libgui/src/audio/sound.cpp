/** @file sound.cpp  Interface for playing sounds.
 *
 * @authors Copyright (c) 2014 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/Sound"

namespace de {

DENG2_PIMPL_NOREF(Sound)
{
    dfloat volume;
    dfloat pan;
    dfloat frequency;
    Vector3f position;
    Vector3f velocity;
    Positioning positioning;
    dfloat minDistance;
    dfloat spread;

    Instance()
        : volume(1.f)
        , pan(0.f)
        , frequency(1.f)
        , positioning(Stereo)
        , minDistance(1.f)
        , spread(0)
    {}

    DENG2_PIMPL_AUDIENCE(Stop)
    DENG2_PIMPL_AUDIENCE(Deletion)
};

DENG2_AUDIENCE_METHOD(Sound, Stop)
DENG2_AUDIENCE_METHOD(Sound, Deletion)

Sound::Sound() : d(new Instance)
{}

Sound &Sound::setVolume(dfloat volume)
{
    d->volume = volume;
    update();
    return *this;
}

Sound &Sound::setPan(dfloat pan)
{
    d->pan = pan;
    update();
    return *this;
}

Sound &Sound::setFrequency(dfloat factor)
{
    d->frequency = factor;
    update();
    return *this;
}

Sound &Sound::setPosition(Vector3f const &position, Positioning positioning)
{
    d->position = position;
    d->positioning = positioning;
    update();
    return *this;
}

Sound &Sound::setVelocity(Vector3f const &velocity)
{
    d->velocity = velocity;
    update();
    return *this;
}

Sound &Sound::setMinDistance(dfloat minDistance)
{
    d->minDistance = minDistance;
    update();
    return *this;
}

Sound &Sound::setSpatialSpread(dfloat degrees)
{
    d->spread = degrees;
    update();
    return *this;
}

bool Sound::isPlaying() const
{
    return mode() != NotPlaying;
}

dfloat Sound::volume() const
{
    return d->volume;
}

dfloat Sound::pan() const
{
    return d->pan;
}

dfloat Sound::frequency() const
{
    return d->frequency;
}

Vector3f Sound::position() const
{
    return d->position;
}

Sound::Positioning Sound::positioning() const
{
    return d->positioning;
}

Vector3f Sound::velocity() const
{
    return d->velocity;
}

dfloat Sound::minDistance() const
{
    return d->minDistance;
}

dfloat Sound::spatialSpread() const
{
    return d->spread;
}

} // namespace de
