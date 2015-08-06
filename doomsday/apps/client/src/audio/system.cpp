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

#define DENG_NO_API_MACROS_SOUND

#include "audio/system.h"

#include "api_sound.h"

#ifdef __CLIENT__
#  include "audio/audiodriver.h"
#endif
#include "audio/s_mus.h"
#include "audio/s_sfx.h"
#include <de/App>

using namespace de;

namespace audio {

static audio::System *theAudioSystem = nullptr;

DENG2_PIMPL(System)
, DENG2_OBSERVES(App, GameUnload)
{
    Instance(Public *i) : Base(i)
    {
        theAudioSystem = thisPublic;

        App::app().audienceForGameUnload() += this;
    }
    ~Instance()
    {
        App::app().audienceForGameUnload() -= this;

        theAudioSystem = nullptr;
    }

    void aboutToUnloadGame(game::Game const &)
    {
        self.reset();
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

void System::reset()
{
#ifdef __CLIENT__
    Sfx_Reset();
#endif
    _api_S.StopMusic();
}

}  // namespace audio
