
// P_maputl.c

#include "Doomdef.h"
#include "P_local.h"


/*
===============================================================================

						THING POSITION SETTING

===============================================================================
*/

//===========================================================================
// P_UnsetThingPosition
//	Unlinks a thing from everything it has been linked to.
//===========================================================================
void P_UnsetThingPosition (mobj_t *thing)
{
	P_UnlinkThing(thing);
}

//===========================================================================
// P_SetThingPosition
//	Links a thing into both a block and a subsector based on it's x,y.
//	Sets thing->subsector properly.
//===========================================================================
void P_SetThingPosition (mobj_t *thing)
{
	P_LinkThing(thing, 
		(!(thing->flags & MF_NOSECTOR)? DDLINK_SECTOR : 0) | 
		(!(thing->flags & MF_NOBLOCKMAP)? DDLINK_BLOCKMAP : 0));
}




