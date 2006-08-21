/* DE1: $Id: def_data.c 3285 2006-06-11 08:01:30Z skyjake $
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
 * Compiles for jDoom/jHeretic/jHexen/WolfTC
 *
 * TODO: Replace me with a standard ADT, data structure.
 */

// HEADER FILES ------------------------------------------------------------

#if  __DOOM64TC__
#  include "doom64tc.h"
#elif __WOLFTC__
#  include "wolftc.h"
#elif __JDOOM__
#  include "jdoom.h"
#elif __JHERETIC__
#  include "jheretic.h"
#elif __JHEXEN__
#  include "jhexen.h"
#elif __JSTRIFE__
#  include "jstrife.h"
#endif

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// keep track of special lines as they are hit,
// but don't process them until the move is proven valid
static line_t **spechit;
static int spechit_max = 0;
static int numspechit = 0;

static int specrover = 0;

// CODE --------------------------------------------------------------------

/*
 * Add the given line to spechit list.
 *
 * @param ld        The line_t to be added to the list.
 * @return          The index of the line within the list else -1.
 */
int P_AddLineToSpecHit(line_t *ld)
{
    if(!ld)
        return -1;

    if(++numspechit > spechit_max)
    {
         spechit_max = (spechit_max? spechit_max * 2 : 8);
         spechit = realloc(spechit, sizeof(line_t*) * spechit_max);
    }

    spechit[numspechit - 1] = ld;

    return numspechit - 1;
}

/*
 * Pop the top of the spechit list and return the next element.
 *
 * @return          Ptr to the next line in the list.
 */
line_t* P_PopSpecHit(void)
{
    if(numspechit > 0)
        return spechit[--numspechit];
    else
        return NULL;
}

/*
 * Returns the next element in the spechit list.
 *
 * @return          The next line_t in the spechit list.
 */
line_t* P_SpecHitIterator(void)
{
    if(numspechit > 0 && specrover > 0)
        return spechit[--specrover];
    else
        return NULL;
}

/*
 * Returns the spechit iterator to the beginning (the end).
 */
void P_SpecHitResetIterator(void)
{
    specrover = numspechit;
}

/*
 * Empty the spechit list.
 */
void P_EmptySpecHit(void)
{
    numspechit = spechit_max = specrover = 0;
}

/*
 * Return the size of the spechit list.
 *
 * @return          The size of the spechit list.
 */
int P_SpecHitSize(void)
{
    return numspechit;
}

/*
 * Free any memory used by the spechit array. Called before
 * starting a new level and befor shutdown.
 */
void P_FreeSpecHit(void)
{
    if(numspechit > 0)
    {
        free(spechit);
        spechit = NULL;
    }

    numspechit = spechit_max = specrover = 0;
}
