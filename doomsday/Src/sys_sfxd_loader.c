
//**************************************************************************
//**
//** SYS_SFXD_LOADER.C
//**
//** Loader for ds*.dll
//**
//**************************************************************************

// HEADER FILES ------------------------------------------------------------

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "de_console.h"
#include "sys_sfxd.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

sfxdriver_t sfxd_external;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static HINSTANCE	hInstExt;
static void			(*driverShutdown)(void);

// CODE --------------------------------------------------------------------

//===========================================================================
// Imp
//===========================================================================
static void *Imp(const char *fn)
{
	return GetProcAddress(hInstExt, fn);
}

//===========================================================================
// DS_UnloadExternal
//===========================================================================
void DS_UnloadExternal(void)
{
	driverShutdown();
	FreeLibrary(hInstExt);
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
	char fn[256];

	// Compose the name, use the prefix "ds".
	sprintf(fn, "ds%s.dll", name);

	// Load the DLL.
	hInstExt = LoadLibrary(fn);
	if(!hInstExt)  // Load failed?
	{
		Con_Message("DS_Load: Loading of %s failed.\n", fn);
		return NULL;
	}
	return DS_ImportExternal();
}
