
//**************************************************************************
//**
//** SYS_SFXD_LOADER.C
//**
//** Loader for ds*.so
//**
//** Probably won't be needed because the OpenAL sound code can be
//** staticly linked.
//**
//**************************************************************************

// HEADER FILES ------------------------------------------------------------

#include <string.h>
#include <ltdl.h>

#include "de_console.h"
#include "sys_sfxd.h"
#include "sys_musd.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern musdriver_t         musd_loaded;
extern musinterface_mus_t  musd_loaded_imus;
extern musinterface_ext_t  musd_loaded_iext;
extern musinterface_cd_t   musd_loaded_icd;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

sfxdriver_t sfxd_external;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static lt_dlhandle handle;
static void	(*driverShutdown)(void);

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
	
	// We should free the DLL at shutdown.
	d->Shutdown = DS_UnloadExternal;
	return d;
}

//===========================================================================
// DS_Load
//	"A3D", "OpenAL" and "Compat" are supported.
//===========================================================================
sfxdriver_t *DS_Load(const char *name)
{
	filename_t fn;

	// Compose the name, use the prefix "ds".
	sprintf(fn, "libds%s", name);

	if((handle = lt_dlopenext(fn)) == NULL)
	{
		Con_Message("DS_Load: Loading of %s failed.\n", fn);
		return NULL;
	}
	
	return DS_ImportExternal();
}

