/**
 * @file size.h
 * 2D sizes.
 *
 * @authors Copyright © 2013 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBLEGACY_SIZE_H
#define LIBLEGACY_SIZE_H

#include "liblegacy.h"

#ifdef __cplusplus
extern "C" {
#endif

/// @addtogroup legacyMath
/// @{

/**
 * Integer width and height. Intended as a handy POD structure for easy
 * manipulation of two dimensional sizes with integer precison.
 */
typedef struct Size2Raw_s {
    union {
        struct {
            int width;
            int height;
        };
        int widthHeight[2];
    };
//#ifdef __cplusplus
//    Size2Raw_s(int w = 0, int h = 0) : width(w), height(h) {}
//#endif
} Size2Raw;

struct size2_s; // The Size2 instance (opaque).
typedef struct size2_s Size2;

DE_PUBLIC Size2 *Size2_New(void);
DE_PUBLIC Size2 *Size2_NewWithDimensions(int width, int height);
DE_PUBLIC Size2 *Size2_NewFromRaw(Size2Raw const *rawSize);
DE_PUBLIC void Size2_Delete(Size2 *size);

DE_PUBLIC Size2Raw *Size2_Raw(Size2 const *size, Size2Raw *rawSize);

DE_PUBLIC dd_bool Size2_IsNull(Size2 const *size);

DE_PUBLIC int Size2_Width(Size2 const *size);
DE_PUBLIC int Size2_Height(Size2 const *size);

DE_PUBLIC void Size2_SetWidth(Size2 *size, int width);
DE_PUBLIC void Size2_SetHeight(Size2 *size, int height);

DE_PUBLIC int const *Size2_WidthHeight(Size2 const *size);
DE_PUBLIC void Size2_SetWidthHeight(Size2 *size, int width, int height);

DE_PUBLIC void Size2_Sum(Size2 *size, Size2 const *other);

DE_PUBLIC dd_bool Size2_Equality(Size2 const *point, Size2 const *other);

/**
 * Floating-point width and height. Intended as a handy POD structure for easy
 * manipulation of two dimensional sizes with floating point precison.
 */
typedef struct Size2Rawf_s {
    union {
        struct {
            double width;
            double height;
        };
        double widthHeight[2];
    };
#ifdef __cplusplus
    Size2Rawf_s(double w = 0, double h = 0) : width(w), height(h) {}
#endif
} Size2Rawf;

struct size2f_s; // The Size2f instance (opaque).
typedef struct size2f_s Size2f;

DE_PUBLIC Size2f *Size2f_New(void);
DE_PUBLIC Size2f *Size2f_NewWithDimensions(double width, double height);
DE_PUBLIC Size2f *Size2f_NewFromRaw(Size2Rawf const *rawSize);
DE_PUBLIC void Size2f_Delete(Size2f *size);

DE_PUBLIC Size2Rawf *Size2f_Raw(Size2f const *size, Size2Rawf *rawSize);

DE_PUBLIC dd_bool Size2f_IsNull(Size2f const *size);

DE_PUBLIC double Size2f_Width(Size2f const *size);
DE_PUBLIC double Size2f_Height(Size2f const *size);

DE_PUBLIC void Size2f_SetWidth(Size2f *size, double width);
DE_PUBLIC void Size2f_SetHeight(Size2f *size, double height);

DE_PUBLIC double const *Size2f_WidthHeight(Size2f const *size);
DE_PUBLIC void Size2f_SetWidthHeight(Size2f *size, double width, double height);

DE_PUBLIC void Size2f_Sum(Size2f *size, Size2f const *other);

DE_PUBLIC dd_bool Size2f_Equality(Size2f const *size, Size2f const *other);

/// @}

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* LIBLEGACY_SIZE_H */
