/** @file fixedpoint.h Fixed-point math.
 *
 * @par Build Options
 * Define DE_NO_FIXED_ASM to disable the assembler fixed-point routines.
 *
 * @author Copyright &copy; 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @author Copyright &copy; 2006-2013 Daniel Swanson <danij@dengine.net>
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
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#ifndef DE_FIXED_POINT_MATH_H
#define DE_FIXED_POINT_MATH_H

#include "../liblegacy.h"

/// @addtogroup legacyMath
/// @{

#define FRACBITS        16
#define FRACUNIT        (1 << FRACBITS)
#define FRACEPSILON     (1.0f / 65535.f) // ~ 1.5e-5

#define SLOPERANGE      2048
#define SLOPEBITS       11
#define DBITS           (FRACBITS-SLOPEBITS)

#define FIX2FLT(x)      ( (x) / (float) FRACUNIT )
#define FIX2DBL(x)      ( (double) FIX2FLT(x) )
#define Q_FIX2FLT(x)    ( (float) ((x) >> FRACBITS) )
#define FLT2FIX(x)      ( (fixed_t) ((x) * FRACUNIT) )
#define DBL2FIX(x)      ( (fixed_t) FLT2FIX((float)(x)) )

#ifdef __cplusplus
extern "C" {
#endif

#if !defined( DE_NO_FIXED_ASM ) && !defined( GNU_X86_FIXED_ASM )

__inline fixed_t FixedMul(fixed_t a, fixed_t b) {
    __asm {
        // The parameters in eax and ebx.
        mov eax, a
        mov ebx, b
        // The multiplying.
        imul ebx
        shrd eax, edx, 16
        // eax should hold the return value.
    }
    // A value is returned regardless of the compiler warning.
}
__inline fixed_t FixedDiv2(fixed_t a, fixed_t b) {
    __asm {
        // The parameters.
        mov eax, a
        mov ebx, b
        // The operation.
        cdq
        shld edx, eax, 16
        sal eax, 16
        idiv ebx
        // And the value returns in eax.
    }
    // A value is returned regardless of the compiler warning.
}

#else

DE_PUBLIC fixed_t FixedMul(fixed_t a, fixed_t b);
DE_PUBLIC fixed_t FixedDiv2(fixed_t a, fixed_t b);

#endif // DE_NO_FIXED_ASM

DE_PUBLIC fixed_t FixedDiv(fixed_t a, fixed_t b);

/// @}

#ifdef __cplusplus
} // extern "C"
#endif

#endif // DE_FIXED_POINT_MATH_H
