/**
 * @file size.h
 * 2D sizes. @ingroup math
 *
 * @authors Copyright © 2012 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG_DATA_SIZE_H
#define LIBDENG_DATA_SIZE_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Size2Raw. Size 2 Raw (integer). Is intended as a handy POD structure
 * for easy manipulation of two dimensional sizes with integer precison.
 */
typedef struct Size2Raw_s {
    union {
        struct {
            int width;
            int height;
        };
        int widthHeight[2];
    };
#ifdef __cplusplus
    Size2Raw_s(int w = 0, int h = 0) : width(w), height(h) {}
#endif
} Size2Raw;

/**
 * Size2. Class for manipulating two dimensional sizes using integer
 * precision.
 */
struct size2_s; // The Size2 instance (opaque).
typedef struct size2_s Size2;

Size2* Size2_New(void);
Size2* Size2_NewWithDimensions(int width, int height);
Size2* Size2_NewFromRaw(const Size2Raw* rawSize);
void Size2_Delete(Size2* size);

Size2Raw* Size2_Raw(const Size2* size, Size2Raw* rawSize);

boolean Size2_IsNull(const Size2* size);

int Size2_Width(const Size2* size);
int Size2_Height(const Size2* size);

void Size2_SetWidth(Size2* size, int width);
void Size2_SetHeight(Size2* size, int height);

const int* Size2_WidthHeight(const Size2* size);
void Size2_SetWidthHeight(Size2* size, int width, int height);

void Size2_Sum(Size2* size, const Size2* other);

boolean Size2_Equality(const Size2* point, const Size2* other);

/**
 * Size2Rawf. Size 2 Raw (f)loating point. Is intended as a handy POD
 * structure for easy manipulation of two dimensional sizes with floating
 * point precison.
 */
typedef struct Size2Rawf_s {
    union {
        struct {
            double width;
            double height;
        };
        double widthHeight[2];
    };
} Size2Rawf;

/**
 * Size2f. Class for manipulating two dimensional sizes using floating
 * point precision.
 */
struct size2f_s; // The Size2f instance (opaque).
typedef struct size2f_s Size2f;

Size2f* Size2f_New(void);
Size2f* Size2f_NewWithDimensions(double width, double height);
Size2f* Size2f_NewFromRaw(const Size2Rawf* rawSize);
void Size2f_Delete(Size2f* size);

Size2Rawf* Size2f_Raw(const Size2f* size, Size2Rawf* rawSize);

boolean Size2f_IsNull(const Size2f* size);

double Size2f_Width(const Size2f* size);
double Size2f_Height(const Size2f* size);

void Size2f_SetWidth(Size2f* size, double width);
void Size2f_SetHeight(Size2f* size, double height);

const double* Size2f_WidthHeight(const Size2f* size);
void Size2f_SetWidthHeight(Size2f* size, double width, double height);

void Size2f_Sum(Size2f* size, const Size2f* other);

boolean Size2f_Equality(const Size2f* size, const Size2f* other);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* LIBDENG_DATA_SIZE_H */
