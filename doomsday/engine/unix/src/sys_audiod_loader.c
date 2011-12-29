/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2004-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2007-2011 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 2006 Jamie Jones <jamie_jones_au@yahoo.com.au>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

/**
 * sys_audiod_loader.c: Loader for ds*.so
 */

// HEADER FILES ------------------------------------------------------------

#include <string.h>

#ifdef UNIX
#  include "sys_dylib.h"
#endif

#include "de_console.h"

#include "s_main.h"
#include "sys_audiod.h"
#include "sys_audiod_sfx.h"
#include "sys_audiod_mus.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

audiodriver_t audiodExternal;

audiointerface_sfx_t audiodExternalISFX;
audiointerface_music_t audiodExternalIMusic;
audiointerface_cd_t audiodExternalICD;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static lt_dlhandle handle = NULL;

// CODE --------------------------------------------------------------------

static void* Imp(const char* fn)
{
    return lt_dlsym(handle, fn);
}

void Sys_ShutdownAudioDriver(void)
{
    if(!audioDriver)
        return;

    if(audioDriver->Shutdown)
        audioDriver->Shutdown();

    if(audioDriver == &audiodExternal)
    {
        lt_dlclose(handle);
        handle = NULL;
    }
}

static audiodriver_t* importExternal(void)
{
    audiodriver_t*        d = &audiodExternal;

    // Clear everything.
    memset(d, 0, sizeof(*d));

    d->Init = Imp("DS_Init");
    d->Shutdown = Imp("DS_Shutdown");
    d->Event = Imp("DS_Event");
    d->Set = Imp("DS_Set");

    // The driver may provide SFX playback functionality.
    if(Imp("DS_SFX_Init"))
    {   // The driver offers a SFX playback interface.
        audiointerface_sfx_t* i = &audiodExternalISFX;

        i->gen.Init = Imp("DS_SFX_Init");
        i->gen.Create = Imp("DS_SFX_CreateBuffer");
        i->gen.Destroy = Imp("DS_SFX_DestroyBuffer");
        i->gen.Load = Imp("DS_SFX_Load");
        i->gen.Reset = Imp("DS_SFX_Reset");
        i->gen.Play = Imp("DS_SFX_Play");
        i->gen.Stop = Imp("DS_SFX_Stop");
        i->gen.Refresh = Imp("DS_SFX_Refresh");

        i->gen.Set = Imp("DS_SFX_Set");
        i->gen.Setv = Imp("DS_SFX_Setv");
        i->gen.Listener = Imp("DS_SFX_Listener");
        i->gen.Listenerv = Imp("DS_SFX_Listenerv");
        i->gen.Getv = Imp("DS_SFX_Getv");
    }

    // The driver may provide music playback functionality.
    if(Imp("DM_Music_Init"))
    {   // The driver also offers a music playback interface.
        audiointerface_music_t* i = &audiodExternalIMusic;

        i->gen.Init = Imp("DM_Music_Init");
        i->gen.Update = Imp("DM_Music_Update");
        i->gen.Get = Imp("DM_Music_Get");
        i->gen.Set = Imp("DM_Music_Set");
        i->gen.Pause = Imp("DM_Music_Pause");
        i->gen.Stop = Imp("DM_Music_Stop");
        i->SongBuffer = Imp("DM_Music_SongBuffer");
        i->Play = Imp("DM_Music_Play");
        i->PlayFile = Imp("DM_Music_PlayFile");
    }

    if(Imp("DM_CDAudio_Init"))
    {   // The driver also offers a CD audio (redbook) playback interface.
        audiointerface_cd_t*  i = &audiodExternalICD;

        i->gen.Init = Imp("DM_CDAudio_Init");
        i->gen.Update = Imp("DM_CDAudio_Update");
        i->gen.Set = Imp("DM_CDAudio_Set");
        i->gen.Get = Imp("DM_CDAudio_Get");
        i->gen.Pause = Imp("DM_CDAudio_Pause");
        i->gen.Stop = Imp("DM_CDAudio_Stop");
        i->Play = Imp("DM_CDAudio_Play");
    }

    return d;
}

audiodriver_t* Sys_LoadAudioDriver(const char* name)
{
    audiodriver_t* ad = NULL;
    if(NULL != name && name[0])
    {
        ddstring_t libPath;
        // Compose the name using the prefix "ds".
        Str_Init(&libPath);
#ifdef MACOSX
        Str_Appendf(&libPath, "ds%s.bundle", name);
#else
        Str_Appendf(&libPath, "libds%s.so", name);
#endif

        // Load the audio driver library and import symbols.
        handle = lt_dlopenext(Str_Text(&libPath));
        if(NULL != handle)
        {
            ad = importExternal();
        }
        if(NULL == ad)
        {
            Con_Message("Warning: Sys_LoadAudioDriver: Loading of \"%s\" failed.\n", Str_Text(&libPath));
        }
        Str_Free(&libPath);
    }
    return ad;
}
