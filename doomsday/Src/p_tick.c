
//**************************************************************************
//**
//** P_TICK.C
//**
//**************************************************************************

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_render.h"
#include "de_play.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

//===========================================================================
// P_MobjTicker
//===========================================================================
void P_MobjTicker(mobj_t *mo)
{
	int i = mo->halofactor & 0x7f;

	// Handle halofactor.
	if(mo->halofactor & 0x80)
	{
		// Going up.
		i += haloOccludeSpeed;
		if(i > 127) i = 127;
	}
	else
	{
		// Going down.
		i -= haloOccludeSpeed;
		if(i < 0) i = 0;
	}
	mo->halofactor &= ~0x7f;
	mo->halofactor |= i;
}

//===========================================================================
// P_Ticker
//	Doomsday's own play-ticker.
//===========================================================================
void P_Ticker(void)
{
	thinker_t *th;

	if(!thinkercap.next) return; // Not initialized yet.

	// New ptcgens for planes?
	P_CheckPtcPlanes();	
	R_AnimateAnimGroups();
	R_SkyTicker();

	// Check all mobjs.
	for(th = thinkercap.next; th != &thinkercap; th = th->next)
	{
		// FIXME: Client mobjs!
		if(!P_IsMobjThinker(th->function)) continue;
		P_MobjTicker( (mobj_t*) th);
	}
}