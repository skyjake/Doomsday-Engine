/** @file worldstage.h  audio::Stage specialized for the world context.
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

#ifndef CLIENT_AUDIO_WORLDSTAGE_H
#define CLIENT_AUDIO_WORLDSTAGE_H

#include "audio/stage.h"

namespace audio {

/**
 * Specialized audio::Stage (soundstage) for the "world" playback context.
 *
 * Automatically clears all logical Sounds from the stage when the current map changes.
 *
 * @ingroup audio
 */
class WorldStage : public Stage
{
public:
    WorldStage(Exclusion exclusion = DontExclude);

private:
    DENG2_PRIVATE(d)
};

}  // namespace audio

#endif  // CLIENT_AUDIO_WORLDSTAGE_H
