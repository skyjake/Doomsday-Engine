
//**************************************************************************
//**
//** M_FIXED.C
//**
//** Fixed-point math routines.
//** Define NO_FIXED_ASM to disable the assembler version.
//**
//**************************************************************************

// HEADER FILES ------------------------------------------------------------

#include "dd_share.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

#ifdef NO_FIXED_ASM

fixed_t FixedMul(fixed_t a, fixed_t b)
{
	// Let's do this in an ultra-slow way.
	return (fixed_t) (((double)a * (double)b) / FRACUNIT);
}

fixed_t FixedDiv2(fixed_t a, fixed_t b)
{
	if(!b) return 0;
	// We've got cycles to spare!
	return (fixed_t) (((double)a / (double)b) * FRACUNIT);
}

#endif