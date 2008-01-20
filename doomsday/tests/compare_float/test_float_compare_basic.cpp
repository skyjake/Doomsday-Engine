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
 * test_float_compare_basic.cpp: Basic Funcionality Unit Testing of the
 * Almost_Equal_Float function
 */

// HEADER FILES ------------------------------------------------------------
#include "compare_float.h"
#include <iostream>

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------
const float Zero = 0.0f;
const float Negative_Zero = -0.0f;
const float Two = 2.0;
const float Almost_Two = 1.9999999;
const float Close_to_Two = 1.9999995;
const float Not_Two = 1.9999993;

// CODE --------------------------------------------------------------------
void Test_Almost_Equal_Float(float A, float B, int Expected_Result, int Maximum_Float_Range_Acceptable)
{
int result = Almost_Equal_Float(A, B, Maximum_Float_Range_Acceptable);
if (result != Expected_Result)
   {
   printf("TEST FAILED: received - %1.9f, %1.9f, %d, instead of expected %s\n", A, B, Maximum_Float_Range_Acceptable, Expected_Result ? "true" : "false");
   }
else
   {
   std::cout << "TEST PASSED\n";
   }
}

int main(int argc, char* argv[])
{
std::cout << "        Almost_Equal_Float Essential Functionality Tests\n";

std::cout << "        Zero and Negative_Zero compare as equal. ";
Test_Almost_Equal_Float( Zero, Negative_Zero, true, MAX_FLOAT_FUZZ );

std::cout << "        Nearby numbers compare as equal. ";
Test_Almost_Equal_Float ( Two, Almost_Two, true, MAX_FLOAT_FUZZ );

std::cout << "        Slightly more distant numbers compare as equal. ";
Test_Almost_Equal_Float ( Two, Close_to_Two, true, MAX_FLOAT_FUZZ );

std::cout << "        Parameters reversed. ";
Test_Almost_Equal_Float (Close_to_Two, Two, true, MAX_FLOAT_FUZZ );

std::cout << "        Even more distant numbers don't compare as equal. ";
Test_Almost_Equal_Float ( Two, Not_Two, false, MAX_FLOAT_FUZZ );

return 0;
}
