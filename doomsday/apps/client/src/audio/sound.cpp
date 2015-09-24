/** @file sound.cpp  Interface for playing sounds.
 *
 * @authors Copyright © 2014-2015 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2015 Daniel Swanson <danij@dengine.net>
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

namespace audio {

DENG2_PIMPL_NOREF(Sound)
{
    DENG2_PIMPL_AUDIENCE(Deletion)
};

DENG2_AUDIENCE_METHOD(Sound, Deletion)

Sound::Sound() : d(new Instance)
{}

Sound::~Sound()
{
    // Notify interested parties.
    DENG2_FOR_AUDIENCE2(Deletion, i)
    {
        i->soundBeingDeleted(*this);
    }
}

}  // namespace audio
