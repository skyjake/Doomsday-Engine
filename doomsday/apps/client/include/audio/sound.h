/** @file sound.h  POD structure for logical sound.
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

#ifndef CLIENT_AUDIO_SOUND_H
#define CLIENT_AUDIO_SOUND_H

#include "dd_share.h"    // SoundEmitter
#include <de/libcore.h>

namespace audio {

class Listener;

/**
 * POD structure for representing a logical sound.
 * @ingroup audio
 */
struct Sound
{
    // Properties:
    bool looping          = false;
    de::dint id           = 0;        ///< Not a valid id.
    SoundEmitter *emitter = nullptr;

    // State:
    de::duint endTime = 0;

    /**
     * Returns @c true if the sound is currently playing relative to @a nowTime.
     */
    inline bool isPlaying(de::duint nowTime) const
    {
        return (looping || endTime > nowTime);
    }

    /**
     * The priority of a sound is affected by distance, volume and age. These points are
     * used to determine which currently playing Channel(s) can be overridden with new
     * sounds. Zero is the lowest priority.
     */
    static de::dfloat ratePriority(de::dfloat volume, SoundEmitter const *emitter,
        de::ddouble const *origin, de::dint startTime, Listener *listener = nullptr);
};

}  // namespace audio

#endif  // CLIENT_AUDIO_SOUND_H
