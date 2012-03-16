/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2007-2012 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 2004 Lukasz Stelmach
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
 * m_fixed.c: Fixed-point math.
 *
 * Define DENG_NO_FIXED_ASM to disable the assembler version.
 */

// HEADER FILES ------------------------------------------------------------

#include <string.h>

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

#ifdef DENG_NO_FIXED_ASM

fixed_t FixedMul(fixed_t a, fixed_t b)
{
    // Let's do this in an ultra-slow way.
    return (fixed_t) (((double) a * (double) b) / FRACUNIT);
}

fixed_t FixedDiv2(fixed_t a, fixed_t b)
{
    if(!b)
        return 0;
    // We've got cycles to spare!
    return (fixed_t) (((double) a / (double) b) * FRACUNIT);
}

#endif

#ifdef GNU_X86_FIXED_ASM
// Courtesy of Lukasz Stelmach.

fixed_t FixedMul(fixed_t a, fixed_t b)
{
    asm volatile (
        "imull %2\n\t"
        "shrdl $16, %%edx, %%eax"
        : "=a" (a)
        : "a" (a), "r" (b)
        : "edx"
    );
    return a;
}

fixed_t FixedDiv2(fixed_t a, fixed_t b)
{
    asm volatile (
        "cdq\n\t"
        "shldl $16, %%eax, %%edx\n\t"
        "sall $16 ,%%eax\n\t"
        "idivl %2"
        : "=a" (a)
        : "a" (a), "r" (b)
        : "edx"
    );
    return a;
}

#endif

fixed_t FixedDiv(fixed_t a, fixed_t b)
{
    if((abs(a) >> 14) >= abs(b))
    {
        return ((a ^ b) < 0 ? DDMININT : DDMAXINT);
    }
    return (FixedDiv2(a, b));
}

int16_t ShortSwap(int16_t n)
{
    return ((n & 0xff) << 8) | ((n & 0xff00) >> 8);
}

int32_t LongSwap(int32_t n)
{
    return (((n & 0xff) << 24) | ((n & 0xff00) << 8) |
            ((n & 0xff0000) >> 8) | ((n & 0xff000000) >> 24));
}

float FloatSwap(float f)
{
    int32_t temp = 0;
    float returnValue = 0;

    memcpy(&temp, &f, 4); // Must be 4.
    temp = LongSwap(temp);
    memcpy(&returnValue, &temp, 4); // Must be 4.
    return returnValue;
}
