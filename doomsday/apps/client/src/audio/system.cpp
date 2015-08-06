/** @file system.cpp  Audio subsystem module.
 *
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

#include "audio/system.h"

#include <de/App>

using namespace de;

namespace audio {

static audio::System *theAudioSystem = nullptr;

DENG2_PIMPL(System)
{
    Instance(Public *i) : Base(i)
    {
        theAudioSystem = thisPublic;
    }
    ~Instance()
    {
        theAudioSystem = nullptr;
    }
};

System::System() : d(new Instance(this))
{}

void System::timeChanged(Clock const &)
{
    // Nothing to do.
}

audio::System &System::get()
{
    DENG2_ASSERT(theAudioSystem);
    return *theAudioSystem;
}

}  // namespace audio
