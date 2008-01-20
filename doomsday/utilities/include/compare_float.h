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
 * compare_float.h: Floating Point Approximate Comparison Routines.
 * These rely on the IEEE floating point representation which is true
 * on x86 and amd64 hardware but may not be on others.
 */


#ifndef __DENGNG_COMPARE_FLOAT_
#define __DENGNG_COMPARE_FLOAT_


#ifdef __cplusplus
extern "C" {
#endif
/** Unless you have a compelling reason, stick with MAX_FLOAT_FUZZ as default
 */
#define MAX_FLOAT_FUZZ 5
int Almost_Equal_Float (float A, float B, int Maximum_Float_Range_Acceptable);
#ifdef __cplusplus
}
#endif

#endif // __DENGNG_COMPARE_FLOAT_
