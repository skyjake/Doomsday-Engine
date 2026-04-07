/** @file audio.h
 *
 * @authors Copyright (c) 2020 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#pragma once

#include "../libdoomsday.h"
#include <de/system.h>

struct mobj_s;

namespace audio {

class LIBDOOMSDAY_PUBLIC Audio : public de::System
{
public:
    Audio();
    virtual ~Audio() override;

    static Audio &get();

    /**
     * @param soundId  @c 0: stops all sounds originating from the given @a emitter.
     * @param emitter  @c nullptr: stops all sounds with the ID. Otherwise both @a soundId
     *                 and @a emitter must match.
     * @param flags    @ref soundStopFlags.
     */
    virtual void stopSound(int soundId, const struct mobj_s *emitter, int flags = 0) = 0;
};

} // namespace audio
