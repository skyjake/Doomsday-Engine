/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2008 Jamie Jones <jamie_jones_au@yahoo.com.au>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
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
 * compare_float.cpp: Floating Point Approximate Comparison Routines.
 * These rely on the IEEE floating point representation which is true
 * on x86 and amd64 hardware but may not be on others.
 *
 * Due to all floating point numbers being approximate, we can't
 * actually compare them like integers, often the least significant
 * bits are different as a result of truncations involved in
 * approximating the numbers, so while mathematically we would check
 * for equality, practically, we should check to see we are close
 * enough, and are within our allowed margin of error.
 *
 * For an easy example of how the same floating point equations
 * can produce different results, do the same equation using the SSE
 * unit, and the x87 units of modern x86 or amd64 systems, and compare.
 * You'll find they don't match - the x87 contains a higher internal
 * accuracy although the both comply with the IEEE standards.
 *
 * Based on the very clever paper by Bruce Dawson located at
 * http://www.cygnus-software.com/papers/comparingfloats/comparingfloats.htm
 */

// HEADER FILES ------------------------------------------------------------
#include <complex>
#include "dd_types.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------
int32_t A_Float_As_Int;
int32_t B_Float_As_Int;
int32_t Float_Integer_Difference;
const int32_t Float_Magic_Number = 0x80000000;


// CODE --------------------------------------------------------------------
/**
 * Compare Single Precision floating point numbers for equality
 * Suggested Maximum_Float_Range_Acceptable is 4 to 10 
 *
 * @return              @c true, if equal within the maximum error allowed
 */
extern "C" int Almost_Equal_Float(float A, float B, int Maximum_Float_Range_Acceptable)
{
/* \todo If not "fast math" check for infinities here
 * please note - additional checks for rare conditions
 * and otherwise not-expected conditions will slow down
 * this code - and this should be a hot spot in our code!
 */

/* \todo If not "fast math" check for NaNs here
 * please note - additional checks for rare conditions
 * and otherwise not-expected conditions will slow down
 * this code - and this should be a hot spot in our code!
 */

/* \todo If not "fast math" check for tiny numbers (denormals)
 * of opposite signs, and make sure they don't "equal"
 * please note - additional checks for rare conditions
 * and otherwise not-expected conditions will slow down
 * this code - and this should be a hot spot in our code!
 */

/* Make A_Float_As_Int lexicographically ordered as a
 * twos-complement integer. We are no longer in IEEE
 * representation now, but its *much* easier to compare
 * as integers now.
 */
int32_t A_Float_As_Int = *(int*)&A;
if (A_Float_As_Int < 0)
   {
   A_Float_As_Int = Float_Magic_Number - A_Float_As_Int;
   }

/* Make B_Float_As_Int lexicographically ordered as a
 * twos-complement integer. We are no longer in IEEE
 * representation now, but its *much* easier to compare
 * as integers now.
 */
int32_t B_Float_As_Int = *(int*)&B;
if (B_Float_As_Int < 0)
   {
   B_Float_As_Int = Float_Magic_Number - B_Float_As_Int;
   }

/* Now we can compare A_Float_As_Int and B_Float_As_Int
 * to find out how far apart A and B are.
 */
int32_t Float_Integer_Difference = abs(A_Float_As_Int - B_Float_As_Int);

if (Float_Integer_Difference <= Maximum_Float_Range_Acceptable)
   {
   return true;
   }
return false;
}

