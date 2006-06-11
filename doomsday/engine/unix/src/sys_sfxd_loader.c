/* DE1: $Id$
 * Copyright (C) 2003, 2004 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * along with this program; if not: http://www.opensource.org/
 */

/*
 * sys_sfxd_loader.c: Loader for ds*.so
 *
 * Probably won't be needed because the OpenAL sound code can be
 * staticly linked.
 */

// HEADER FILES ------------------------------------------------------------

#include <string.h>

#ifdef MACOSX
#  include "sys_dylib.h"
#else
#  include <ltdl.h>
#endif

#include "de_console.h"
#include "sys_sfxd.h"
#include "sys_musd.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern musdriver_t musd_loaded;
extern musinterface_mus_t musd_loaded_imus;
extern musinterface_ext_t musd_loaded_iext;
extern musinterface_cd_t musd_loaded_icd;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

sfxdriver_t sfxd_external;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static lt_dlhandle handle;
static void (*driverShutdown) (void);

// CODE --------------------------------------------------------------------

static void dummyVoid(void)
{
}

//===========================================================================
// Imp
//===========================================================================
static void *Imp(const char *fn)
{
	return lt_dlsym(handle, fn);
}

//===========================================================================
// DS_UnloadExternal
//===========================================================================
void DS_UnloadExternal(void)
{
	driverShutdown();
	lt_dlclose(handle);
	handle = NULL;
}

//===========================================================================
// DS_ImportExternal
//===========================================================================
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

	// The driver may also offer an Ext music interface.
	if(Imp("DM_Ext_Init"))
	{
		musdriver_t *m = &musd_loaded;
		musinterface_ext_t *i = &musd_loaded_iext;

		m->Init = Imp("DS_Init");
		m->Shutdown = dummyVoid;

		i->gen.Init = Imp("DM_Ext_Init");
		i->gen.Update = Imp("DM_Ext_Update");
		i->gen.Set = Imp("DM_Ext_Set");
		i->gen.Get = Imp("DM_Ext_Get");
		i->gen.Pause = Imp("DM_Ext_Pause");
		i->gen.Stop = Imp("DM_Ext_Stop");

		i->SongBuffer = Imp("DM_Ext_SongBuffer");
		i->PlayFile = Imp("DM_Ext_PlayFile");
		i->PlayBuffer = Imp("DM_Ext_PlayBuffer");
	}

	// The driver may also offer a MUS music interface.
	if(Imp("DM_Mus_Init"))
	{
		musdriver_t *m = &musd_loaded;
		musinterface_mus_t *i = &musd_loaded_imus;

		m->Init = Imp("DS_Init");
		m->Shutdown = dummyVoid;

		i->gen.Init = Imp("DM_Mus_Init");
		i->gen.Update = Imp("DM_Mus_Update");
		i->gen.Set = Imp("DM_Mus_Set");
		i->gen.Get = Imp("DM_Mus_Get");
		i->gen.Pause = Imp("DM_Mus_Pause");
		i->gen.Stop = Imp("DM_Mus_Stop");

		i->SongBuffer = Imp("DM_Mus_SongBuffer");
		i->Play = Imp("DM_Mus_Play");
	}

	// We should free the DLL at shutdown.
	d->Shutdown = DS_UnloadExternal;
	return d;
}

//===========================================================================
// DS_Load
//  "A3D", "OpenAL" and "Compat" are supported.
//===========================================================================
sfxdriver_t *DS_Load(const char *name)
{
	filename_t fn;

#ifdef MACOSX
	sprintf(fn, "ds%s.bundle", name);
#else
	// Compose the name, use the prefix "ds".
	sprintf(fn, "libds%s", name);
#endif

	if((handle = lt_dlopenext(fn)) == NULL)
	{
		Con_Message("DS_Load: Loading of %s failed.\n", fn);
		return NULL;
	}

	return DS_ImportExternal();
}
