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
 *
 * Based on Hexen by Raven Software.
 */

/*
 * p_intercept.c: Line/Object Interception
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_play.h"

// MACROS ------------------------------------------------------------------

#define MININTERCEPTS	128

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

intercept_t		*intercepts = 0, *intercept_p;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static int		maxIntercepts = 0;

// CODE --------------------------------------------------------------------

/*
 * P_ClearIntercepts
 *	Empties the intercepts array and makes sure it has been allocated.
 */
void P_ClearIntercepts(void)
{
	if(!intercepts) 
	{
		intercepts = Z_Malloc(sizeof(*intercepts) 
			* (maxIntercepts = MININTERCEPTS), 
			PU_STATIC, 0);
	}
	intercept_p = intercepts;
}

/*
 * P_AddIntercept
 *	You must clear intercepts before the first time this is called.
 *	The intercepts array grows if necessary. Returns a pointer to the new 
 *	intercept.
 */
intercept_t *P_AddIntercept(fixed_t frac, boolean isaline, void *ptr)
{
	int count = intercept_p - intercepts;

	if(count == maxIntercepts)
	{
		// Allocate more memory.
		intercepts = Z_Realloc(intercepts, maxIntercepts *= 2, PU_STATIC);
		intercept_p = intercepts + count;
	}
	
	// Fill in the data that has been provided.
	intercept_p->frac = frac;
	intercept_p->isaline = isaline;
	intercept_p->d.thing = ptr;
	return intercept_p++;
}

/*
 * P_TraverseIntercepts
 *	Reformatted.
 *	Returns true if the traverser function returns true for all lines.
 */
boolean P_TraverseIntercepts(traverser_t func, fixed_t maxfrac)
{
	intercept_t *in = NULL;
	int count = intercept_p - intercepts;
	while(count--)
    {
		fixed_t dist = DDMAXINT;
		intercept_t *scan;
		for(scan = intercepts; scan < intercept_p; scan++)
			if(scan->frac < dist)
				dist = (in=scan)->frac;
		if(dist > maxfrac) return true;	// checked everything in range
		if(!func(in)) return false;		// don't bother going farther
		in->frac = DDMAXINT;
    }
	return true;                  // everything was traversed
}

/*
 * P_SightTraverseIntercepts 
 *	Returns true if the traverser function returns true for all lines.
 */
boolean P_SightTraverseIntercepts
	(divline_t *strace, boolean (*func)(intercept_t*))
{
	int			count;
	fixed_t		dist;
	intercept_t	*scan, *in;
	divline_t	dl;

	count = intercept_p - intercepts;

	// Calculate intercept distance.
	for(scan = intercepts; scan < intercept_p; scan++)
	{
		P_MakeDivline(scan->d.line, &dl);
		scan->frac = P_InterceptVector(strace, &dl);
	}

	// Go through in order.
	in = 0;                 // shut up compiler warning
	while (count--)
	{
		dist = DDMAXINT;
		for(scan = intercepts; scan < intercept_p; scan++)
			if(scan->frac < dist)
			{
				dist = scan->frac;
				in = scan;
			}

		if(!func(in)) return false; // don't bother going farther
		in->frac = DDMAXINT;
	}
	return true;            // everything was traversed
}

