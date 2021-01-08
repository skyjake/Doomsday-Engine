/** @file de/binangle.h Binary angle mathematics.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#ifndef LIBDENG_BAMS_MATH_H
#define LIBDENG_BAMS_MATH_H

#include <de/liblegacy.h>
#include <de/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/// @addtogroup legacyMath
/// @{

#define BAMS_BITS   16

#if BAMS_BITS == 32
typedef uint32_t binangle_t;
#  define BANG_TO_ANGLE(bang)     ((angle_t)(bang))
#elif BAMS_BITS == 16
typedef uint16_t binangle_t;
#  define BANG_TO_ANGLE(bang)     ((angle_t)(bang) << 16)
#else
typedef unsigned char binangle_t;
#  define BANG_TO_ANGLE(bang)     ((angle_t)(bang) << 24)
#endif

#define ANGLE_TO_BANG(angle)      ((binangle_t)((angle_t)(angle) >> BAMS_BITS))

#if BAMS_BITS == 32

// Some predefined angles.
#define BANG_0      0u             // To the east.
#define BANG_45     0x20000000u    // To the northeast.
#define BANG_90     0x40000000u    // To the north.
#define BANG_135    0x60000000u    // To the northwest.
#define BANG_180    0x80000000u    // To the west.
#define BANG_225    0xa0000000u    // To the southwest.
#define BANG_270    0xc0000000u    // To the south.
#define BANG_315    0xe0000000u    // To the southeast.
#define BANG_360    0x100000000u   // Actually the same as angle 0.
#define BANG_MAX    0xffffffffu    // The largest possible angle.

#elif BAMS_BITS == 16

// Some predefined angles.
#define BANG_0      ((binangle_t) 0)        // To the east.
#define BANG_45     ((binangle_t) 0x2000)   // To the northeast.
#define BANG_90     ((binangle_t) 0x4000)   // To the north.
#define BANG_135    ((binangle_t) 0x6000)   // To the northwest.
#define BANG_180    ((binangle_t) 0x8000)   // To the west.
#define BANG_225    ((binangle_t) 0xa000)   // To the southwest.
#define BANG_270    ((binangle_t) 0xc000)   // To the south.
#define BANG_315    ((binangle_t) 0xe000)   // To the southeast.
#define BANG_360    0x10000u
#define BANG_MAX    ((binangle_t) 0xffff)   // The largest possible angle.

#else                           // Byte-sized.

// Some predefined angles.
#define BANG_0      ((binangle_t) 0)        // To the east.
#define BANG_45     ((binangle_t) 0x20)     // To the northeast.
#define BANG_90     ((binangle_t) 0x40)     // To the north.
#define BANG_135    ((binangle_t) 0x60)     // To the northwest.
#define BANG_180    ((binangle_t) 0x80)     // To the west.
#define BANG_225    ((binangle_t) 0xa0)     // To the southwest.
#define BANG_270    ((binangle_t) 0xc0)     // To the south.
#define BANG_315    ((binangle_t) 0xe0)     // To the southeast.
#define BANG_360    ((binangle_t) 0x100)    // Actually the same than angle 0.
#define BANG_MAX    ((binangle_t) 0xff)     // The largest possible angle.

#endif

#define BAMS_PI             3.14159265359
#define RAD2BANG(x)         ((binangle_t)((int)((x)/BAMS_PI*BANG_180)))
#define BANG2RAD(x)         ((float)((x)/(float)BANG_180*BAMS_PI))
#define BANG2DEG(x)         ((float)((x)/(float)BANG_180*180))

// Compass directions, for convenience.
#define BANG_EAST       BANG_0
#define BANG_NORTHEAST  BANG_45
#define BANG_NORTH      BANG_90
#define BANG_NORTHWEST  BANG_135
#define BANG_WEST       BANG_180
#define BANG_SOUTHWEST  BANG_225
#define BANG_SOUTH      BANG_270
#define BANG_SOUTHEAST  BANG_315

void bamsInit(void);

DENG_PUBLIC binangle_t bamsAtan2(int y, int x);

/// @}

#ifdef __cplusplus
} // extern "C"
#endif

#endif
