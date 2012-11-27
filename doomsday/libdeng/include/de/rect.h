/**
 * @file rect.h
 * Rectangles. @ingroup math
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

#ifndef LIBDENG_DATA_RECT_H
#define LIBDENG_DATA_RECT_H

#include "point.h"
#include "size.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Integer rectangle. Intended as a handy POD structure for manipulation
 * of rectangles using integer precison.
 */
typedef struct RectRaw_s {
    Point2Raw origin;
    Size2Raw size;
#ifdef __cplusplus
    RectRaw_s(int x = 0, int y = 0, int width = 0, int height = 0)
        : origin(x, y), size(width, height) {}
#endif
} RectRaw;

/**
 * Rect instance.
 */
struct rect_s; // The Rect instance (opaque).
typedef struct rect_s Rect;

DENG_PUBLIC Rect *Rect_New(void);
DENG_PUBLIC Rect *Rect_NewWithOriginSize(Point2 const *origin, Size2 const *size);
DENG_PUBLIC Rect *Rect_NewWithOriginSize2(int x, int y, int width, int height);
DENG_PUBLIC Rect *Rect_NewFromRaw(RectRaw const *rawRect);

DENG_PUBLIC void Rect_Delete(Rect *rect);

DENG_PUBLIC void Rect_Copy(Rect *rect, Rect const *other);
DENG_PUBLIC Rect *Rect_CopyRaw(Rect *rect, RectRaw const *rawRect);

DENG_PUBLIC RectRaw *Rect_Raw(Rect const *rect, RectRaw *rawRect);

DENG_PUBLIC boolean Rect_IsNull(Rect const *rect);

DENG_PUBLIC Point2 const *Rect_Origin(Rect const *rect);
DENG_PUBLIC int Rect_X(Rect const *rect);
DENG_PUBLIC int Rect_Y(Rect const *rect);

DENG_PUBLIC void Rect_SetOrigin(Rect *rect, Point2 const *origin);
DENG_PUBLIC void Rect_SetX(Rect *rect, int x);
DENG_PUBLIC void Rect_SetY(Rect *rect, int y);
DENG_PUBLIC void Rect_SetXY(Rect *rect, int x, int y);

DENG_PUBLIC void Rect_Translate(Rect *r, Point2Raw const *delta);
DENG_PUBLIC void Rect_TranslateX(Rect *rect, int x);
DENG_PUBLIC void Rect_TranslateY(Rect *rect, int y);
DENG_PUBLIC void Rect_TranslateXY(Rect *rect, int x, int y);

DENG_PUBLIC Size2 const *Rect_Size(Rect const *rect);
DENG_PUBLIC int Rect_Width(Rect const *rect);
DENG_PUBLIC int Rect_Height(Rect const *rect);

DENG_PUBLIC void Rect_SetSize(Rect *rect, Size2 const *size);
DENG_PUBLIC void Rect_SetWidth(Rect *rect, int width);
DENG_PUBLIC void Rect_SetHeight(Rect *rect, int height);

DENG_PUBLIC void Rect_SetWidthHeight(Rect *rect, int width, int height);

DENG_PUBLIC Point2Raw *Rect_TopLeft(Rect const *rect, Point2Raw *point);
DENG_PUBLIC Point2Raw *Rect_TopRight(Rect const *rect, Point2Raw *point);
DENG_PUBLIC Point2Raw *Rect_BottomLeft(Rect const *rect, Point2Raw *point);
DENG_PUBLIC Point2Raw *Rect_BottomRight(Rect const *rect, Point2Raw *point);

DENG_PUBLIC Rect *Rect_Normalize(Rect *rect);
DENG_PUBLIC Rect *Rect_Unite(Rect *rect, Rect const *other);
DENG_PUBLIC Rect *Rect_UniteRaw(Rect *rect, RectRaw const *other);

DENG_PUBLIC boolean Rect_Equality(Rect const *rect, Rect const *other);

/**
 * Static non-members.
 */
DENG_PUBLIC RectRaw *Rect_Normalized(Rect const *rect, RectRaw *normalized);
DENG_PUBLIC RectRaw *Rect_United(Rect const *rect, Rect const *other, RectRaw *united);

/**
 * Floating-point rectangle. Intended as a handy POD structure for
 * manipulation of rectangles using floating point precison.
 */
typedef struct RectRawf_s {
    Point2Rawf origin;
    Size2Rawf size;
} RectRawf;

/**
 * Rectf instance.
 */
struct rectf_s; // The Rectf instance (opaque).
typedef struct rectf_s Rectf;

DENG_PUBLIC Rectf *Rectf_New(void);
DENG_PUBLIC Rectf *Rectf_NewWithOriginSize(Point2f const *origin, Size2f const *size);
DENG_PUBLIC Rectf *Rectf_NewFromRaw(RectRawf const *rawRect);

DENG_PUBLIC void Rectf_Delete(Rectf *rect);

DENG_PUBLIC void Rectf_Copy(Rectf *rect, Rectf const *other);
DENG_PUBLIC Rectf *Rectf_CopyRaw(Rectf *r, RectRawf const *rawRect);

DENG_PUBLIC RectRawf *Rectf_Raw(Rectf const *rect, RectRawf *rawRect);

DENG_PUBLIC boolean Rectf_IsNull(Rectf const *rect);

DENG_PUBLIC Point2f const *Rectf_Origin(Rectf const *rect);
DENG_PUBLIC double Rectf_X(Rectf const *rect);
DENG_PUBLIC double Rectf_Y(Rectf const *rect);

DENG_PUBLIC void Rectf_SetOrigin(Rectf *rect, Point2f const *origin);
DENG_PUBLIC void Rectf_SetX(Rectf *rect, double x);
DENG_PUBLIC void Rectf_SetY(Rectf *rect, double y);
DENG_PUBLIC void Rectf_SetXY(Rectf *rect, double x, double y);

DENG_PUBLIC void Rectf_TranslateX(Rectf *rect, double x);
DENG_PUBLIC void Rectf_TranslateY(Rectf *rect, double y);
DENG_PUBLIC void Rectf_TranslateXY(Rectf *rect, double x, double y);

DENG_PUBLIC Size2f const *Rectf_Size(Rectf const *rect);
DENG_PUBLIC double Rectf_Width(Rectf const *rect);
DENG_PUBLIC double Rectf_Height(Rectf const *rect);

DENG_PUBLIC void Rectf_SetSize(Rectf *rect, Size2f const *size);
DENG_PUBLIC void Rectf_SetWidth(Rectf *rect, double width);
DENG_PUBLIC void Rectf_SetHeight(Rectf *rect, double height);

DENG_PUBLIC void Rectf_SetWidthHeight(Rectf *rect, double width, double height);

DENG_PUBLIC Point2Rawf *Rectf_TopLeft(Rectf const *rect, Point2Rawf *point);
DENG_PUBLIC Point2Rawf *Rectf_TopRight(Rectf const *rect, Point2Rawf *point);
DENG_PUBLIC Point2Rawf *Rectf_BottomLeft(Rectf const *rect, Point2Rawf *point);
DENG_PUBLIC Point2Rawf *Rectf_BottomRight(Rectf const *rect, Point2Rawf *point);

DENG_PUBLIC Rectf *Rectf_Normalize(Rectf *rect);
DENG_PUBLIC Rectf *Rectf_Unite(Rectf *rect, Rectf const *other);
DENG_PUBLIC Rectf *Rectf_UniteRaw(Rectf *rect, RectRawf const *other);

DENG_PUBLIC boolean Rectf_Equality(Rectf const *rect, Rectf const *other);

/**
 * Static non-members:
 */
DENG_PUBLIC RectRawf *Rectf_Normalized(Rectf const *rect, RectRawf *normalized);
DENG_PUBLIC RectRawf *Rectf_United(Rectf const *rect, Rectf const *other, RectRawf *united);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* LIBDENG_DATA_RECT_H */
