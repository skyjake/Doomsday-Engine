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

//#define DENG_NO_API_MACROS_SOUND

#include "audio/system.h"

#include "api_sound.h"

#ifdef __CLIENT__
#  include "audio/audiodriver.h"
#endif
#include "audio/s_main.h"
#include "audio/s_mus.h"
#include "audio/s_sfx.h"
#include <doomsday/console/cmd.h>
#include <doomsday/console/var.h>
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

/**
 * Console command for playing a (local) sound effect.
 */
D_CMD(PlaySound)
{
    DENG2_UNUSED(src);

    if(argc < 2)
    {
        LOG_SCR_NOTE("Usage: %s (id) (volume) at (x) (y) (z)") << argv[0];
        LOG_SCR_MSG("(volume) must be in 0..1, but may be omitted.");
        LOG_SCR_MSG("'at (x) (y) (z)' may also be omitted.");
        LOG_SCR_MSG("The sound is always played locally.");
        return true;
    }
    dint p = 0;

    // The sound ID is always first.
    dint const id = ::defs.getSoundNum(argv[1]);

    // The second argument may be a volume.
    dfloat volume = 1;
    if(argc >= 3 && String(argv[2]).compareWithoutCase("at"))
    {
        volume = String(argv[2]).toFloat();
        p = 3;
    }
    else
    {
        p = 2;
    }

    bool useFixedPos = false;
    coord_t fixedPos[3];
    if(argc >= p + 4 && !String(argv[p]).compareWithoutCase("at"))
    {
        useFixedPos = true;
        fixedPos[0] = String(argv[p + 1]).toDouble();
        fixedPos[1] = String(argv[p + 2]).toDouble();
        fixedPos[2] = String(argv[p + 3]).toDouble();
    }

    // Check that the volume is valid.
    volume = de::clamp(0.f, volume, 1.f);
    if(de::fequal(volume, 0)) return true;

    if(useFixedPos)
    {
        S_LocalSoundAtVolumeFrom(id, nullptr, fixedPos, volume);
    }
    else
    {
        S_LocalSoundAtVolume(id, nullptr, volume);
    }

    return true;
}

#ifdef __CLIENT__
static void reverbVolumeChanged()
{
    Sfx_UpdateReverb();
}
#endif

void System::consoleRegister()  // static
{
    C_VAR_BYTE  ("sound-overlap-stop",  &sfxOneSoundPerEmitter, 0, 0, 1);

#ifdef __CLIENT__
    C_VAR_INT   ("sound-volume",        &sfxVolume,             0, 0, 255);
    C_VAR_INT   ("sound-info",          &showSoundInfo,         0, 0, 1);
    C_VAR_INT   ("sound-rate",          &sfxSampleRate,         0, 11025, 44100);
    C_VAR_INT   ("sound-16bit",         &sfx16Bit,              0, 0, 1);
    C_VAR_INT   ("sound-3d",            &sfx3D,                 0, 0, 1);
    C_VAR_FLOAT2("sound-reverb-volume", &sfxReverbStrength,     0, 0, 1.5f, reverbVolumeChanged);

    C_CMD_FLAGS("playsound", nullptr, PlaySound, CMDF_NO_DEDICATED);

    Mus_Register();
#endif
}

}  // namespace audio
