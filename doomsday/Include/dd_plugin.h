#ifndef __DOOMSDAY_PLUGIN_H__
#define __DOOMSDAY_PLUGIN_H__

#define MAX_HOOKS			8
#define HOOKF_EXCLUSIVE		0x01000000

typedef int (*hookfunc_t)(int type, int parm, void *data);

enum // Hook types. 
{
	HOOK_STARTUP	= 0,	// Called ASAP after startup.
	HOOK_INIT		= 1,	// Called after engine has been initialized.
	HOOK_DEFS		= 2,	// Called after DEDs have been loaded.
	NUM_HOOK_TYPES
};

int Plug_AddHook(int hook_type, hookfunc_t hook);
int Plug_RemoveHook(int hook_type, hookfunc_t hook);

// Plug_DoHook is used by the engine to call all functions 
// registered to a hook.
int Plug_DoHook(int hook_type);

#endif