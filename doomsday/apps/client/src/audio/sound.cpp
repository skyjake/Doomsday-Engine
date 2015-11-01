/** @file sound.cpp  POD structure for a logical sound.
 *
 * @authors Copyright © 2014-2015 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "audio/sound.h"

#include "audio/stage.h"
#include "world/p_object.h"
#include <de/timer.h>  // TICSPERSEC

using namespace de;

namespace audio {

dfloat Sound::ratePriority(dfloat volume, SoundEmitter const *emitter, ddouble const *origin,
    dint startTime, Listener *listener)  // static
{
    mobj_t const *tracking = listener ? listener->trackedMapObject() : nullptr;

    // Deminish the rating over five seconds from the start time until zero.
    dfloat const timeoff = 1000 * (Timer_Ticks() - startTime) / (5.0f * TICSPERSEC);

    // Rate sounds without an origin simply by playback volume.
    if(!tracking || (!emitter && !origin))
    {
        return 1000 * volume - timeoff;
    }
    // Rate sounds with an origin by both distance and playback volume.
    else
    {
        ddouble const distFromListener
            = Mobj_ApproxPointDistance(*tracking, emitter ? emitter->origin : origin);

        return 1000 * volume - distFromListener / 2 - timeoff;
    }
}

}  // namespace audio
