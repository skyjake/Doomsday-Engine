/** @file sound.cpp  Logical Sound model for the audio::System.
 *
 * @authors Copyright © 2003-2014 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2015 Daniel Swanson <danij@dengine.net>
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

#include "audio/sound.h"

#include "world/p_object.h"
#include "world/thinkers.h"
#include <de/timer.h>

using namespace de;

namespace audio {

DENG2_PIMPL_NOREF(Sound)
{
    // Properties:
    SoundFlags flags;
    dint effectId;
    SoundEmitter *emitter;
    Vector3d origin;

    // State:
    duint endTime;

    Instance()
        : flags(DefaultSoundFlags)
        , effectId(0)
        , emitter (nullptr)
        , endTime (0)
    {}

    Instance(Instance const &other)
        : de::IPrivate()
        , flags   (other.flags)
        , effectId(other.effectId)
        , emitter (other.emitter)
        , origin  (other.origin)
        , endTime (other.endTime)
    {}

private:
    Instance &operator = (Instance const &); // no assignment
};

Sound::Sound() : d(new Instance)
{}

Sound::Sound(SoundFlags flags, dint effectId, Vector3d const &origin, duint endTime, SoundEmitter *emitter)
    : d(new Instance)
{
    d->flags    = flags;
    d->effectId = effectId;
    d->emitter  = emitter;
    if(!d->flags.testFlag(NoOrigin)) d->origin = origin;
    d->endTime  = endTime;
}

Sound::Sound(Sound const &other) : d(new Instance(*other.d))
{}

bool Sound::isPlaying(duint nowTime) const
{
    return (d->flags.testFlag(Repeat) || d->endTime > nowTime);
}

SoundFlags Sound::flags() const
{
    return d->flags;
}

dint Sound::effectId() const
{
    return d->effectId;
}

Vector3d Sound::origin() const
{
    return d->origin;
}

Vector3d Sound::velocity() const
{
    // We don't track velocity because it is meaningless to us.
    if(emitter() && Thinker_IsMobjFunc(emitter()->thinker.function))
    {
        return Vector3d(((mobj_t *)emitter())->mom) * TICSPERSEC;
    }
    return Vector3d();  // Not moving.
}

bool Sound::emitterIsMoving() const
{
    return emitter() && Thinker_IsMobjFunc(emitter()->thinker.function);
}

SoundEmitter *Sound::emitter() const
{
    return d->emitter;
}

void Sound::updateOriginFromEmitter()
{
    if(d->flags.testFlag(NoOrigin)) return;

    // Only if we are tracking an emitter.
    if(!emitter()) return;

    d->origin = Vector3d(emitter()->origin);

    // When tracking a map-object set the Z axis position to the object's center.
    if(Thinker_IsMobjFunc(emitter()->thinker.function))
    {
        d->origin.z += ((mobj_t *)emitter())->height / 2;
    }
}

}  // namespace audio
