/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2009 Daniel Swanson <danij@dengine.net>
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

/**
 * p_intercept.c: Line/Object Interception
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_play.h"

// MACROS ------------------------------------------------------------------

#define MININTERCEPTS   128

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// Must be static so these are not confused with intercepts in game libs.
static intercept_t *intercepts = 0;
static intercept_t *intercept_p = 0;
static int maxIntercepts = 0;

// CODE --------------------------------------------------------------------

/**
 * Empties the intercepts array and makes sure it has been allocated.
 */
void P_ClearIntercepts(void)
{
    if(!intercepts)
    {
        intercepts =
            Z_Malloc(sizeof(*intercepts) * (maxIntercepts = MININTERCEPTS),
                     PU_STATIC, 0);
    }
    intercept_p = intercepts;
}

/**
 * You must clear intercepts before the first time this is called.
 * The intercepts array grows if necessary.
 *
 * @return              Ptr to the new intercept.
 */
intercept_t *P_AddIntercept(float frac, intercepttype_t type, void *ptr)
{
    int     count = intercept_p - intercepts;

    if(count == maxIntercepts)
    {
        // Allocate more memory.
        intercepts = Z_Realloc(intercepts, maxIntercepts *= 2, PU_STATIC);
        intercept_p = intercepts + count;
    }

    if(type == ICPT_LINE && P_ToIndex(ptr) >= numLineDefs)
    {
        count = count;
    }

    // Fill in the data that has been provided.
    intercept_p->frac = frac;
    intercept_p->type = type;
    intercept_p->d.mo = ptr;
    return intercept_p++;
}

/**
 * @return              @c true, if the traverser function returns @c true,
 *                      for all lines.
 */
boolean P_TraverseIntercepts(traverser_t func, float maxfrac)
{
    intercept_t *in = NULL;
    int         count = intercept_p - intercepts;

    while(count--)
    {
        float       dist = DDMAXFLOAT;
        intercept_t *scan;

        for(scan = intercepts; scan < intercept_p; scan++)
            if(scan->frac < dist)
                dist = (in = scan)->frac;

        if(dist > maxfrac)
            return true; // Checked everything in range.
        if(!func(in))
            return false; // Don't bother going farther.

        if(in)
            in->frac = DDMAXFLOAT;
    }

    return true; // Everything was traversed.
}

/**
 * @return              @c true, if the traverser function returns @c true,
 *                      for all lines.
 */
boolean P_SightTraverseIntercepts(divline_t *strace,
                                  boolean (*func) (intercept_t *))
{
    int         count;
    float       dist;
    intercept_t *scan, *in;
    divline_t   dl;

    count = intercept_p - intercepts;

    // Calculate intercept distance.
    for(scan = intercepts; scan < intercept_p; scan++)
    {
        P_MakeDivline(scan->d.lineDef, &dl);
        scan->frac = P_InterceptVector(strace, &dl);
    }

    // Go through in order.
    in = 0; // Shut up compiler warning.
    while(count--)
    {
        dist = DDMAXFLOAT;
        for(scan = intercepts; scan < intercept_p; scan++)
            if(scan->frac < dist)
            {
                dist = scan->frac;
                in = scan;
            }

        if(!in)
            continue; // Huh?
        if(!func(in))
            return false; // Don't bother going farther.

        in->frac = DDMAXFLOAT;
    }

    return true; // Everything was traversed.
}
