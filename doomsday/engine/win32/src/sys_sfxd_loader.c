/**\file
 *\section Copyright and License Summary
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2006 Jaakko Keränen <skyjake@dengine.net>
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

/*
 * sys_sfxd_loader.c: Sound Driver DLL Loader (Win32)
 *
 * Loader for ds*.dll
 */

// HEADER FILES ------------------------------------------------------------

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "de_console.h"
#include "sys_sfxd.h"
#include "sys_musd.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

sfxdriver_t sfxd_external;
musdriver_t musd_external;
musinterface_mus_t musd_external_imus;
musinterface_ext_t musd_external_iext;
musinterface_cd_t musd_external_icd;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static HINSTANCE hInstExt;
static void (*driverShutdown) (void);

// CODE --------------------------------------------------------------------

static void *Imp(const char *fn)
{
    return GetProcAddress(hInstExt, fn);
}

void DS_UnloadExternal(void)
{
    driverShutdown();
    FreeLibrary(hInstExt);
}

sfxdriver_t *DS_ImportExternal(void)
{
    sfxdriver_t *d = &sfxd_external;

    // Clear everything.
    memset(d, 0, sizeof(*d));

    d->Init = Imp("DS_Init");
    driverShutdown = Imp("DS_Shutdown");
    d->Create = Imp("DS_CreateBuffer");
    d->Destroy = Imp("DS_DestroyBuffer");
    d->Load = Imp("DS_Load");
    d->Reset = Imp("DS_Reset");
    d->Play = Imp("DS_Play");
    d->Stop = Imp("DS_Stop");
    d->Refresh = Imp("DS_Refresh");
    d->Event = Imp("DS_Event");
    d->Set = Imp("DS_Set");
    d->Setv = Imp("DS_Setv");
    d->Listener = Imp("DS_Listener");
    d->Listenerv = Imp("DS_Listenerv");
    d->Getv = Imp("DS_Getv");

    // The driver may also provide music playback functionality.
    // These additional init and shutdown functions are optional.
    musd_external.Init = Imp("DM_Init");
    musd_external.Shutdown = Imp("DM_Shutdown");

    if(Imp("DM_Mus_Init"))
    {
        // The driver also offers a Mus music playback interface.
        musinterface_mus_t* m = &musd_external_imus;
        m->gen.Init = Imp("DM_Mus_Init");
        m->gen.Update = Imp("DM_Mus_Update");
        m->gen.Get = Imp("DM_Mus_Get");
        m->gen.Set = Imp("DM_Mus_Set");
        m->gen.Pause = Imp("DM_Mus_Pause");
        m->gen.Stop = Imp("DM_Mus_Stop");
        m->Play = Imp("DM_Mus_Play");
        m->SongBuffer = Imp("DM_Mus_SongBuffer");
    }

    if(Imp("DM_Ext_Init"))
    {
        // The driver also offers an Ext music playback interface.
        musinterface_ext_t* m = &musd_external_iext;
        m->gen.Init = Imp("DM_Ext_Init");
        m->gen.Update = Imp("DM_Ext_Update");
        m->gen.Get = Imp("DM_Ext_Get");
        m->gen.Set = Imp("DM_Ext_Set");
        m->gen.Pause = Imp("DM_Ext_Pause");
        m->gen.Stop = Imp("DM_Ext_Stop");
        m->PlayFile = Imp("DM_Ext_PlayFile");
        m->PlayBuffer = Imp("DM_Ext_PlayBuffer");
        m->SongBuffer = Imp("DM_Ext_SongBuffer");
    }

    // We should free the DLL at shutdown.
    d->Shutdown = DS_UnloadExternal;
    return d;
}

/*
 * "A3D", "OpenAL" and "Compat" are supported.
 */
sfxdriver_t *DS_Load(const char *name)
{
    char    fn[256];

    // Compose the name, use the prefix "ds".
    sprintf(fn, "ds%s.dll", name);

    // Load the DLL.
    hInstExt = LoadLibrary(fn);
    if(!hInstExt)               // Load failed?
    {
        Con_Message("DS_Load: Loading of %s failed.\n", fn);
        return NULL;
    }
    return DS_ImportExternal();
}
