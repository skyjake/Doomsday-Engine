
//**************************************************************************
//**
//** P_ACTOR.C
//**
//** Common code relating to actors, or "monsters".
//** Actor movement smoothing; the "Servo".
//**
//**************************************************************************

// HEADER FILES ------------------------------------------------------------

#if __JDOOM__
#include "doomdef.h"
#include "p_local.h"
#elif __JHERETIC__
#include "Doomdef.h"
#elif __JHEXEN__
#include "h2def.h"
#endif

// MACROS ------------------------------------------------------------------

#define MIN_STEP	((10 * ANGLE_1) >> 16) // degrees per tic
#define MAX_STEP	(ANG90 >> 16)

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

//==========================================================================
// P_SetThingSRVO
//	The actor has taken a step, set the corresponding short-range visual
//	offset.
//==========================================================================
void P_SetThingSRVO(mobj_t *mo, int stepx, int stepy)
{
	// Shift to 8.8 fixed point.
	mo->srvo[0] = (-stepx) >> 8;
	mo->srvo[1] = (-stepy) >> 8;
}

//==========================================================================
// P_SetThingSRVO
//	The actor has taken a step, set the corresponding short-range visual
//	offset.
//==========================================================================
void P_SetThingSRVOZ(mobj_t *mo, int stepz)
{
	// Shift to 8.8 fixed point.
	mo->srvo[2] = (-stepz) >> 8;
}

//==========================================================================
// P_SRVOAngleTicker
//	Turn visual angle towards real angle. An engine cvar controls whether
//	the visangle or the real angle is used in rendering.
//	Real-life analogy: angular momentum (you can't suddenly just take a
//	90 degree turn in zero time).
//==========================================================================
void P_SRVOAngleTicker(mobj_t *mo)
{
	short target, step, diff;
	int lstep, hgt;

	// Check requirements.
	if(mo->flags & MF_MISSILE || !(mo->flags & MF_COUNTKILL))
	{
		mo->visangle = mo->angle >> 16;
		return;	// This is not for us.
	}

	target = mo->angle >> 16;
	diff = target - mo->visangle;

	if(mo->turntime)
	{
		if(mo->tics)
			step = abs(diff) / mo->tics;
		else
			step = abs(diff);
		if(!step) step = 1;
	}
	else
	{
		// Calculate a good step size. Thing height and diff taken into account.
		hgt = mo->height >> 16;
		if(hgt < 30) hgt = 30;
		if(hgt > 60) hgt = 60;
		lstep = abs(diff) * 8 / hgt;
		if(lstep < MIN_STEP) lstep = MIN_STEP;
		if(lstep > MAX_STEP) lstep = MAX_STEP;
		step = lstep;
	}
	
	// Do the step.
	if(abs(diff) <= step)
		mo->visangle = target;
	else if(diff > 0)
		mo->visangle += step;
	else if(diff < 0)
		mo->visangle -= step;
}

//==========================================================================
// P_ClearThingSRVO
//	The thing's timer has run out, which means the thing has completed its
//	step. Or there has been a teleport.
//==========================================================================
void P_ClearThingSRVO(mobj_t *mo)
{
	memset(mo->srvo, 0, sizeof(mo->srvo));
}

/*
//===========================================================================
// P_UpdateAlpha
//	Sets the mobj's selector's alpha bits according to the alpha level.
//===========================================================================
void P_UpdateAlpha(mobj_t *mo)
{
	// Clear the highest three bits (the alpha bits).
	mo->selector &= 0x1fffffff;
	mo->selector |= (mo->alpha >> 5) << 29;
}
*/

//===========================================================================
// P_UpdateHealthBits
//	The first three bits of the selector special byte contain a
//	relative health level.
//===========================================================================
void P_UpdateHealthBits(mobj_t *mobj)
{
	int i;

	if(mobj->info && mobj->info->spawnhealth > 0)
	{
		mobj->selector &= DDMOBJ_SELECTOR_MASK; // Clear high byte.
		i = (mobj->health << 3) / mobj->info->spawnhealth;
		if(i > 7) i = 7;
		if(i < 0) i = 0;
		mobj->selector |= i << DDMOBJ_SELECTOR_SHIFT;
	}
}

//===========================================================================
// P_UpdateMobjFlags
//	Update gravity-related flags for Doomsday.
//===========================================================================
void P_UpdateMobjFlags(mobj_t *mobj)
{
/*	if(mobj->flags & MF_NOGRAVITY || mobj->flags2 & MF2_FLOATBOB)
		mobj->ddflags |= DDMF_NOGRAVITY;
	else
		mobj->ddflags &= ~DDMF_NOGRAVITY;

#ifndef __JDOOM__
	if(mobj->flags2 & MF2_LOGRAV)
		mobj->ddflags |= DDMF_LOWGRAVITY;
	else
		mobj->ddflags &= ~DDMF_LOWGRAVITY;
#endif*/
}
