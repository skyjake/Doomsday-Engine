/* DE1: $Id$
 * Copyright (C) 2003 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * m_fixed.c: Naive Fixed-Point Math
 *
 * Define NO_FIXED_ASM to disable the assembler version.
 */

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

fixed_t FixedDiv(fixed_t a, fixed_t b)
{
	if((abs(a)>>14) >= abs(b))
	{
		return((a^b)<0 ? DDMININT : DDMAXINT);
	}
	return(FixedDiv2(a, b));
}
