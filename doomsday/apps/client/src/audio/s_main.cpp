/** @file s_main.cpp  Audio Subsystem.
 *
 * Interface to the Sfx and Mus modules. High-level (and exported) audio control.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2014 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2007 Jamie Jones <jamie_jones_au@yahoo.com.au>
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

#include "audio/s_main.h"

#ifdef __CLIENT__
#  include <de/concurrency.h>
#endif
#include <doomsday/doomsdayapp.h>
#include <doomsday/audio/logical.h>
#include <doomsday/filesys/fs_main.h>
#include <doomsday/filesys/fs_util.h>

#ifdef __CLIENT__
#  include "audio/audiodriver.h"
#endif
#include "audio/s_cache.h"
#include "audio/s_mus.h"
#include "audio/s_sfx.h"
#include "audio/sys_audio.h"

#include "world/p_players.h"
#include "Sector"

#include "dd_main.h" // isDedicated
#include "m_profiler.h"

#ifdef __CLIENT__
#  include "gl/gl_main.h"
#  include "ui/clientwindow.h"
#endif
#ifdef __SERVER__
#  include "server/sv_sound.h"
#endif

using namespace de;

int showSoundInfo;

dd_bool S_Init()
{
#ifdef __CLIENT__
    dd_bool sfxOK, musOK;
#endif

    Sfx_Logical_SetSampleLengthCallback(Sfx_GetSoundLength);

    if(CommandLine_Exists("-nosound") || CommandLine_Exists("-noaudio"))
        return true;

    // Disable random pitch changes?
    ::noRndPitch = CommandLine_Exists("-norndpitch");

#ifdef __CLIENT__
    // Try to load the audio driver plugin(s).
    if(!AudioDriver_Init())
    {
        LOG_AUDIO_NOTE("Music and sound effects are disabled");
        return false;
    }

    sfxOK = Sfx_Init();
    musOK = Mus_Init();

    if(!sfxOK || !musOK)
    {
        LOG_AUDIO_NOTE("Errors during audio subsystem initialization");
        return false;
    }
#endif

    return true;
}

void S_Shutdown()
{
#ifdef __CLIENT__
    Sfx_Shutdown();
    Mus_Shutdown();

    // Finally, close the audio driver.
    AudioDriver_Shutdown();
#endif
}

void S_MapChange()
{
    // Stop everything in the LSM.    
    Sfx_InitLogical();

#ifdef __CLIENT__
    Sfx_MapChange();
#endif
}

void S_SetupForChangedMap()
{
#ifdef __CLIENT__
    // Update who is listening now.
    Sfx_SetListener(S_GetListenerMobj());
#endif
}

void S_Drawer()
{
#ifdef __CLIENT__
    if(!::showSoundInfo) return;

    DENG_ASSERT_IN_MAIN_THREAD();
    DENG_ASSERT_GL_CONTEXT_ACTIVE();

    // Go into screen projection mode.
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, DENG_GAMEVIEW_WIDTH, DENG_GAMEVIEW_HEIGHT, 0, -1, 1);

    Sfx_DebugInfo();

    // Back to the original.
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
#endif  // __CLIENT__
}
