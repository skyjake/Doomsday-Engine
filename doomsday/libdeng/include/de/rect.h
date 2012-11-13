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

DENG_PUBLIC Rect* Rect_New(void);
DENG_PUBLIC Rect* Rect_NewWithOriginSize(const Point2* origin, const Size2* size);
DENG_PUBLIC Rect* Rect_NewWithOriginSize2(int x, int y, int width, int height);
DENG_PUBLIC Rect* Rect_NewFromRaw(const RectRaw* rawRect);

DENG_PUBLIC void Rect_Delete(Rect* rect);

DENG_PUBLIC void Rect_Copy(Rect* rect, const Rect* other);
DENG_PUBLIC Rect* Rect_CopyRaw(Rect* rect, const RectRaw* rawRect);

DENG_PUBLIC RectRaw* Rect_Raw(const Rect* rect, RectRaw* rawRect);

DENG_PUBLIC boolean Rect_IsNull(const Rect* rect);

DENG_PUBLIC const Point2* Rect_Origin(const Rect* rect);
DENG_PUBLIC int Rect_X(const Rect* rect);
DENG_PUBLIC int Rect_Y(const Rect* rect);

DENG_PUBLIC void Rect_SetOrigin(Rect* rect, const Point2* origin);
DENG_PUBLIC void Rect_SetX(Rect* rect, int x);
DENG_PUBLIC void Rect_SetY(Rect* rect, int y);
DENG_PUBLIC void Rect_SetXY(Rect* rect, int x, int y);

DENG_PUBLIC void Rect_Translate(Rect* r, const Point2Raw* delta);
DENG_PUBLIC void Rect_TranslateX(Rect* rect, int x);
DENG_PUBLIC void Rect_TranslateY(Rect* rect, int y);
DENG_PUBLIC void Rect_TranslateXY(Rect* rect, int x, int y);

DENG_PUBLIC const Size2* Rect_Size(const Rect* rect);
DENG_PUBLIC int Rect_Width(const Rect* rect);
DENG_PUBLIC int Rect_Height(const Rect* rect);

DENG_PUBLIC void Rect_SetSize(Rect* rect, const Size2* size);
DENG_PUBLIC void Rect_SetWidth(Rect* rect, int width);
DENG_PUBLIC void Rect_SetHeight(Rect* rect, int height);

DENG_PUBLIC void Rect_SetWidthHeight(Rect* rect, int width, int height);

DENG_PUBLIC Point2Raw* Rect_TopLeft(const Rect* rect, Point2Raw* point);
DENG_PUBLIC Point2Raw* Rect_TopRight(const Rect* rect, Point2Raw* point);
DENG_PUBLIC Point2Raw* Rect_BottomLeft(const Rect* rect, Point2Raw* point);
DENG_PUBLIC Point2Raw* Rect_BottomRight(const Rect* rect, Point2Raw* point);

DENG_PUBLIC Rect* Rect_Normalize(Rect* rect);
DENG_PUBLIC Rect* Rect_Unite(Rect* rect, const Rect* other);
DENG_PUBLIC Rect* Rect_UniteRaw(Rect* rect, const RectRaw* other);

DENG_PUBLIC boolean Rect_Equality(const Rect* rect, const Rect* other);

/**
 * Static non-members.
 */
DENG_PUBLIC RectRaw* Rect_Normalized(const Rect* rect, RectRaw* normalized);
DENG_PUBLIC RectRaw* Rect_United(const Rect* rect, const Rect* other, RectRaw* united);

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

DENG_PUBLIC Rectf* Rectf_New(void);
DENG_PUBLIC Rectf* Rectf_NewWithOriginSize(const Point2f* origin, const Size2f* size);
DENG_PUBLIC Rectf* Rectf_NewFromRaw(const RectRawf* rawRect);

DENG_PUBLIC void Rectf_Delete(Rectf* rect);

DENG_PUBLIC void Rectf_Copy(Rectf* rect, const Rectf* other);
DENG_PUBLIC Rectf* Rectf_CopyRaw(Rectf* r, const RectRawf* rawRect);

DENG_PUBLIC RectRawf* Rectf_Raw(const Rectf* rect, RectRawf* rawRect);

DENG_PUBLIC boolean Rectf_IsNull(const Rectf* rect);

DENG_PUBLIC const Point2f* Rectf_Origin(const Rectf* rect);
DENG_PUBLIC double Rectf_X(const Rectf* rect);
DENG_PUBLIC double Rectf_Y(const Rectf* rect);

DENG_PUBLIC void Rectf_SetOrigin(Rectf* rect, const Point2f* origin);
DENG_PUBLIC void Rectf_SetX(Rectf* rect, double x);
DENG_PUBLIC void Rectf_SetY(Rectf* rect, double y);
DENG_PUBLIC void Rectf_SetXY(Rectf* rect, double x, double y);

DENG_PUBLIC void Rectf_TranslateX(Rectf* rect, double x);
DENG_PUBLIC void Rectf_TranslateY(Rectf* rect, double y);
DENG_PUBLIC void Rectf_TranslateXY(Rectf* rect, double x, double y);

DENG_PUBLIC const Size2f* Rectf_Size(const Rectf* rect);
DENG_PUBLIC double Rectf_Width(const Rectf* rect);
DENG_PUBLIC double Rectf_Height(const Rectf* rect);

DENG_PUBLIC void Rectf_SetSize(Rectf* rect, const Size2f* size);
DENG_PUBLIC void Rectf_SetWidth(Rectf* rect, double width);
DENG_PUBLIC void Rectf_SetHeight(Rectf* rect, double height);

DENG_PUBLIC void Rectf_SetWidthHeight(Rectf* rect, double width, double height);

DENG_PUBLIC Point2Rawf* Rectf_TopLeft(const Rectf* rect, Point2Rawf* point);
DENG_PUBLIC Point2Rawf* Rectf_TopRight(const Rectf* rect, Point2Rawf* point);
DENG_PUBLIC Point2Rawf* Rectf_BottomLeft(const Rectf* rect, Point2Rawf* point);
DENG_PUBLIC Point2Rawf* Rectf_BottomRight(const Rectf* rect, Point2Rawf* point);

DENG_PUBLIC Rectf* Rectf_Normalize(Rectf* rect);
DENG_PUBLIC Rectf* Rectf_Unite(Rectf* rect, const Rectf* other);
DENG_PUBLIC Rectf* Rectf_UniteRaw(Rectf* rect, const RectRawf* other);

DENG_PUBLIC boolean Rectf_Equality(const Rectf* rect, const Rectf* other);

/**
 * Static non-members:
 */
DENG_PUBLIC RectRawf* Rectf_Normalized(const Rectf* rect, RectRawf* normalized);
DENG_PUBLIC RectRawf* Rectf_United(const Rectf* rect, const Rectf* other, RectRawf* united);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* LIBDENG_DATA_RECT_H */
