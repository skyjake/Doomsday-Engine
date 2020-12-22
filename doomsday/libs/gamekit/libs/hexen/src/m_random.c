/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 1999 Activision
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
 * m_random.c: Random number LUT.
 */

// HEADER FILES ------------------------------------------------------------

#include "jhexen.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// This is the new flat distribution table
static unsigned char rndTable[256] = {
    201, 1, 243, 19, 18, 42, 183, 203, 101, 123, 154, 137, 34, 118, 10, 216,
    135, 246, 0, 107, 133, 229, 35, 113, 177, 211, 110, 17, 139, 84, 251, 235,
    182, 166, 161, 230, 143, 91, 24, 81, 22, 94, 7, 51, 232, 104, 122, 248,
    175, 138, 127, 171, 222, 213, 44, 16, 9, 33, 88, 102, 170, 150, 136, 114,
    62, 3, 142, 237, 6, 252, 249, 56, 74, 30, 13, 21, 180, 199, 32, 132,
    187, 234, 78, 210, 46, 131, 197, 8, 206, 244, 73, 4, 236, 178, 195, 70,
    121, 97, 167, 217, 103, 40, 247, 186, 105, 39, 95, 163, 99, 149, 253, 29,
    119, 83, 254, 26, 202, 65, 130, 155, 60, 64, 184, 106, 221, 93, 164, 196,
    112, 108, 179, 141, 54, 109, 11, 126, 75, 165, 191, 227, 87, 225, 156, 15,
    98, 162, 116, 79, 169, 140, 190, 205, 168, 194, 41, 250, 27, 20, 14, 241,
    50, 214, 72, 192, 220, 233, 67, 148, 96, 185, 176, 181, 215, 207, 172, 85,
    89, 90, 209, 128, 124, 2, 55, 173, 66, 152, 47, 129, 59, 43, 159, 240,
    239, 12, 189, 212, 144, 28, 200, 77, 219, 198, 134, 228, 45, 92, 125, 151,
    5, 53, 255, 52, 68, 245, 160, 158, 61, 86, 58, 82, 117, 37, 242, 145,
    69, 188, 115, 76, 63, 100, 49, 111, 153, 80, 38, 57, 174, 224, 71, 231,
    23, 25, 48, 218, 120, 147, 208, 36, 226, 223, 193, 238, 157, 204, 146, 31
};

static int rndIndex = 0, prndIndex = 0;

// CODE --------------------------------------------------------------------

/**
 * \note The returned value is calculated deterministically.
 *
 * @return                  A pseudo-random number from the table.
 */
byte P_Random(void)
{
    return rndTable[(++prndIndex) & 0xff];
}

/**
 * \note The returned value is NOT calculated deterministically.
 *
 * @return                  A pseudo-random number from the table.
 */
byte M_Random (void)
{
    rndIndex = (rndIndex+1)&0xff;
    return rndTable[rndIndex];
}

/**
 * Resets the seed for the random number generator.
 */
void M_ResetRandom(void)
{
    rndIndex = prndIndex = 0;
}
