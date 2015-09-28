/** @file logicsound.h  Logical sound management.
 * @ingroup audio
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2013-2015 Daniel Swanson <danij@dengine.net>
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

#ifndef AUDIO_LOGICSOUND_H
#define AUDIO_LOGICSOUND_H

namespace audio {

/**
 * LogicSounds are used to track currently playing sounds on a logical level (irrespective
 * of whether playback is available, or if the sounds are actually audible to anyone).
 */
struct LogicSound
{
    struct mobj_s *emitter = nullptr;
    uint endTime     = 0;
    bool isRepeating = false;

    bool inline isPlaying(uint nowTime) const {
        return (isRepeating || endTime > nowTime);
    }
};

}  // namespace audio

#endif  // AUDIO_LOGICSOUND_H
