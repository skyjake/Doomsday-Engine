/**
 * @file rect.c
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

#include "de/rect.h"
#include "de/memory.h"

struct rect_s {
    Point2* origin;
    Size2* size;
};

struct rectf_s {
    Point2f* origin;
    Size2f* size;
};

Rect* Rect_New(void)
{
    Rect* r = M_Malloc(sizeof *r);
    r->origin = Point2_New();
    r->size = Size2_New();
    return r;
}

Rect* Rect_NewWithOriginSize(const Point2* origin, const Size2* size)
{
    Rect* r = Rect_New();
    Rect_SetOrigin(r, origin);
    Rect_SetSize(r, size);
    return r;
}

Rect* Rect_NewWithOriginSize2(int x, int y, int width, int height)
{
    Rect* r = Rect_New();
    Rect_SetXY(r, x, y);
    Rect_SetWidthHeight(r, width, height);
    return r;
}

Rect* Rect_NewFromRaw(const RectRaw* rawRect)
{
    DENG_ASSERT(rawRect);
    return Rect_NewWithOriginSize2(rawRect->origin.x, rawRect->origin.y,
                                   rawRect->size.width, rawRect->size.height);
}

void Rect_Delete(Rect* r)
{
    DENG_ASSERT(r);
    Point2_Delete(r->origin);
    Size2_Delete(r->size);
    M_Free(r);
}

void Rect_Copy(Rect* r, const Rect* other)
{
    DENG_ASSERT(r);
    if(!other) return;
    Point2_SetXY(r->origin, Rect_X(other), Rect_Y(other));
    Size2_SetWidthHeight(r->size, Rect_Width(other), Rect_Height(other));
}

Rect* Rect_CopyRaw(Rect* r, const RectRaw* rawRect)
{
    DENG_ASSERT(r);
    if(rawRect)
    {
        Point2_SetXY(r->origin, rawRect->origin.x, rawRect->origin.y);
        Size2_SetWidthHeight(r->size, rawRect->size.width, rawRect->size.height);
    }
    return r;
}

RectRaw* Rect_Raw(const Rect* r, RectRaw* rawRect)
{
    DENG_ASSERT(r);
    if(!rawRect) return NULL;
    Point2_Raw(r->origin, &rawRect->origin);
    Size2_Raw(r->size, &rawRect->size);
    return rawRect;
}

boolean Rect_IsNull(const Rect* r)
{
    DENG_ASSERT(r);
    return Size2_IsNull(r->size);
}

int Rect_Width(const Rect* r)
{
    DENG_ASSERT(r);
    return Size2_Width(r->size);
}

int Rect_Height(const Rect* r)
{
    DENG_ASSERT(r);
    return Size2_Height(r->size);
}

void Rect_SetWidth(Rect* r, int width)
{
    DENG_ASSERT(r);
    Size2_SetWidth(r->size, width);
}

void Rect_SetHeight(Rect* r, int height)
{
    DENG_ASSERT(r);
    Size2_SetHeight(r->size, height);
}

const Point2* Rect_Origin(const Rect* r)
{
    DENG_ASSERT(r);
    return r->origin;
}

int Rect_X(const Rect* r)
{
    DENG_ASSERT(r);
    return Point2_X(r->origin);
}

int Rect_Y(const Rect* r)
{
    DENG_ASSERT(r);
    return Point2_Y(r->origin);
}

void Rect_SetOrigin(Rect* r, const Point2* origin)
{
    DENG_ASSERT(r);
    Point2_SetXY(r->origin, Point2_X(origin), Point2_Y(origin));
}

void Rect_SetX(Rect* r, int x)
{
    DENG_ASSERT(r);
    Point2_SetX(r->origin, x);
}

void Rect_SetY(Rect* r, int y)
{
    DENG_ASSERT(r);
    Point2_SetY(r->origin, y);
}

void Rect_SetXY(Rect* r, int x, int y)
{
    DENG_ASSERT(r);
    Point2_SetXY(r->origin, x, y);
}

void Rect_TranslateX(Rect* r, int x)
{
    DENG_ASSERT(r);
    Point2_TranslateX(r->origin, x);
}

void Rect_TranslateY(Rect* r, int y)
{
    DENG_ASSERT(r);
    Point2_TranslateY(r->origin, y);
}

void Rect_TranslateXY(Rect* r, int x, int y)
{
    DENG_ASSERT(r);
    Point2_TranslateXY(r->origin, x, y);
}

void Rect_Translate(Rect* r, const Point2Raw* delta)
{
    DENG_ASSERT(r);
    Point2_Translate(r->origin, delta);
}

const Size2* Rect_Size(const Rect* r)
{
    DENG_ASSERT(r);
    return r->size;
}

void Rect_SetSize(Rect* r, const Size2* size)
{
    DENG_ASSERT(r);
    Size2_SetWidthHeight(r->size, Size2_Width(size), Size2_Height(size));
}

void Rect_SetWidthHeight(Rect* r, int width, int height)
{
    DENG_ASSERT(r);
    Size2_SetWidthHeight(r->size, width, height);
}

Point2Raw* Rect_TopLeft(const Rect* r, Point2Raw* point)
{
    DENG_ASSERT(r);
    if(!point) return NULL;
    point->x = Point2_X(r->origin);
    point->y = Point2_Y(r->origin);
    return point;
}

Point2Raw* Rect_TopRight(const Rect* r, Point2Raw* point)
{
    DENG_ASSERT(r);
    if(!point) return NULL;
    point->x = Point2_X(r->origin) + Size2_Width(r->size);
    point->y = Point2_Y(r->origin);
    return point;
}

Point2Raw* Rect_BottomLeft(const Rect* r, Point2Raw* point)
{
    DENG_ASSERT(r);
    if(!point) return NULL;
    point->x = Point2_X(r->origin);
    point->y = Point2_Y(r->origin) + Size2_Height(r->size);
    return point;
}

Point2Raw* Rect_BottomRight(const Rect* r, Point2Raw* point)
{
    DENG_ASSERT(r);
    if(!point) return NULL;
    point->x = Point2_X(r->origin) + Size2_Width(r->size);
    point->y = Point2_Y(r->origin) + Size2_Height(r->size);
    return point;
}

Rect* Rect_Normalize(Rect* r)
{
    DENG_ASSERT(r);
    if(Size2_Width(r->size) < 0)
        Point2_TranslateX(r->origin, -Size2_Width(r->size));
    if(Size2_Height(r->size) < 0)
        Point2_TranslateY(r->origin, -Size2_Height(r->size));
    return r;
}

RectRaw* Rect_Normalized(const Rect* rect, RectRaw* normalized)
{
    if(!normalized) return NULL;

    if(!rect)
    {
        memset(normalized, 0, sizeof(*normalized));
        return normalized;
    }

    Rect_Raw(rect, normalized);
    if(normalized->size.width < 0)
        normalized->origin.x -= normalized->size.width;
    if(normalized->size.height < 0)
        normalized->origin.y -= normalized->size.height;

    return normalized;
}

/// @pre  This and @a other have been normalized.
static Rect* Rect_UniteRaw2(Rect* r, const RectRaw* other)
{
    Point2Raw oldOrigin;
    DENG_ASSERT(r && other);

    Point2_Raw(r->origin, &oldOrigin);

    Rect_SetXY(r, MIN_OF(Point2_X(r->origin), other->origin.x),
                  MIN_OF(Point2_Y(r->origin), other->origin.y));

    Rect_SetWidthHeight(r, MAX_OF(oldOrigin.x + Size2_Width(r->size),
                                  other->origin.x + other->size.width)  - Point2_X(r->origin),
                           MAX_OF(oldOrigin.y + Size2_Height(r->size),
                                  other->origin.y + other->size.height) - Point2_Y(r->origin));

    return r;
}

Rect* Rect_UniteRaw(Rect* r, const RectRaw* other)
{
    RectRaw otherNormalized;

    if(!other) return r;

    memcpy(&otherNormalized, other, sizeof(otherNormalized));
    if(otherNormalized.size.width < 0)
        otherNormalized.origin.x -= otherNormalized.size.width;
    if(otherNormalized.size.height < 0)
        otherNormalized.origin.y -= otherNormalized.size.height;

    return Rect_UniteRaw2(Rect_Normalize(r), &otherNormalized);
}

Rect* Rect_Unite(Rect* r, const Rect* other)
{
    RectRaw rawOther;
    return Rect_UniteRaw2(Rect_Normalize(r), Rect_Normalized(other, &rawOther));
}

RectRaw* Rect_United(const Rect* rect, const Rect* other, RectRaw* united)
{
    RectRaw normA, normB;

    if(!united) return NULL;

    if(!other)
    {
        united->origin.x = Point2_X(rect->origin);
        united->origin.y = Point2_Y(rect->origin);
        united->size.width  = Size2_Width(rect->size);
        united->size.height = Size2_Height(rect->size);
        return united;
    }

    Rect_Normalized(rect, &normA);
    Rect_Normalized(other, &normB);

    united->origin.x = MIN_OF(normA.origin.x, normB.origin.x);
    united->origin.y = MIN_OF(normA.origin.y, normB.origin.y);

    united->size.width  = MAX_OF(normA.origin.x + normA.size.width,
                                 normB.origin.x + normB.size.width)  - united->origin.x;
    united->size.height = MAX_OF(normA.origin.y + normA.size.height,
                                 normB.origin.y + normB.size.height) - united->origin.y;

    return united;
}

boolean Rect_Equality(const Rect* r, const Rect* other)
{
    DENG_ASSERT(r);
    if(!other) return false;
    return r == other || (Point2_Equality(r->origin, Rect_Origin(other)) &&
                          Size2_Equality(r->size, Rect_Size(other)));
}

Rectf* Rectf_New(void)
{
    Rectf* r = M_Calloc(sizeof *r);
    r->origin = Point2f_New();
    r->size = Size2f_New();
    return r;
}

Rectf* Rectf_NewWithOriginSize(const Point2f* origin, const Size2f* size)
{
    Rectf* r = Rectf_New();
    Rectf_SetOrigin(r, origin);
    Rectf_SetSize(r, size);
    return r;
}

Rectf* Rectf_NewWithOriginSize2(double x, double y, double width, double height)
{
    Rectf* r = Rectf_New();
    Rectf_SetXY(r, x, y);
    Rectf_SetWidthHeight(r, width, height);
    return r;
}

Rectf* Rectf_NewFromRaw(const RectRawf* rawRect)
{
    DENG_ASSERT(rawRect);
    return Rectf_NewWithOriginSize2(rawRect->origin.x, rawRect->origin.y,
                                    rawRect->size.width, rawRect->size.height);
}

void Rectf_Delete(Rectf* r)
{
    DENG_ASSERT(r);
    Point2f_Delete(r->origin);
    Size2f_Delete(r->size);
    M_Free(r);
}

void Rectf_Copy(Rectf* r, const Rectf* other)
{
    DENG_ASSERT(r);
    if(!other) return;
    Point2f_SetXY(r->origin, Rectf_X(other), Rectf_Y(other));
    Size2f_SetWidthHeight(r->size, Rectf_Width(other), Rectf_Height(other));
}

Rectf* Rectf_CopyRaw(Rectf* r, const RectRawf* rawRect)
{
    DENG_ASSERT(r);
    if(rawRect)
    {
        Point2f_SetXY(r->origin, rawRect->origin.x, rawRect->origin.y);
        Size2f_SetWidthHeight(r->size, rawRect->size.width, rawRect->size.height);
    }
    return r;
}

RectRawf* Rectf_Raw(const Rectf* r, RectRawf* rawRect)
{
    DENG_ASSERT(r);
    if(!rawRect) return NULL;
    Point2f_Raw(r->origin, &rawRect->origin);
    Size2f_Raw(r->size, &rawRect->size);
    return rawRect;
}

boolean Rectf_IsNull(const Rectf* r)
{
    DENG_ASSERT(r);
    return Size2f_IsNull(r->size);
}

double Rectf_Width(const Rectf* r)
{
    DENG_ASSERT(r);
    return Size2f_Width(r->size);
}

double Rectf_Height(const Rectf* r)
{
    DENG_ASSERT(r);
    return Size2f_Height(r->size);
}

const Point2f* Rectf_Origin(const Rectf* r)
{
    DENG_ASSERT(r);
    return r->origin;
}

double Rectf_X(const Rectf* r)
{
    DENG_ASSERT(r);
    return Point2f_X(r->origin);
}

double Rectf_Y(const Rectf* r)
{
    DENG_ASSERT(r);
    return Point2f_Y(r->origin);
}

void Rectf_SetOrigin(Rectf* r, const Point2f* origin)
{
    DENG_ASSERT(r);
    Point2f_SetXY(r->origin, Point2f_X(origin), Point2f_Y(origin));
}

void Rectf_SetX(Rectf* r, double x)
{
    DENG_ASSERT(r);
    Point2f_SetX(r->origin, x);
}

void Rectf_SetY(Rectf* r, double y)
{
    DENG_ASSERT(r);
    Point2f_SetY(r->origin, y);
}

void Rectf_SetXY(Rectf* r, double x, double y)
{
    DENG_ASSERT(r);
    Point2f_SetXY(r->origin, x, y);
}

void Rectf_Translate(Rectf* r, const Point2Rawf* delta)
{
    DENG_ASSERT(r);
    Point2f_Translate(r->origin, delta);
}

void Rectf_TranslateX(Rectf* r, double x)
{
    DENG_ASSERT(r);
    Point2f_TranslateX(r->origin, x);
}

void Rectf_TranslateY(Rectf* r, double y)
{
    DENG_ASSERT(r);
    Point2f_TranslateY(r->origin, y);
}

void Rectf_TranslateXY(Rectf* r, double x, double y)
{
    DENG_ASSERT(r);
    Point2f_TranslateXY(r->origin, x, y);
}

void Rectf_SetWidth(Rectf* r, double width)
{
    DENG_ASSERT(r);
    Size2f_SetWidth(r->size, width);
}

void Rectf_SetHeight(Rectf* r, double height)
{
    DENG_ASSERT(r);
    Size2f_SetHeight(r->size, height);
}

const Size2f* Rectf_Size(const Rectf* r)
{
    DENG_ASSERT(r);
    return r->size;
}

void Rectf_SetSize(Rectf* r, const Size2f* size)
{
    DENG_ASSERT(r);
    Size2f_SetWidthHeight(r->size, Size2f_Width(size), Size2f_Height(size));
}

void Rectf_SetWidthHeight(Rectf* r, double width, double height)
{
    DENG_ASSERT(r);
    Size2f_SetWidthHeight(r->size, width, height);
}

Point2Rawf* Rectf_TopLeft(const Rectf* r, Point2Rawf* point)
{
    DENG_ASSERT(r);
    if(!point) return NULL;
    point->x = Point2f_X(r->origin);
    point->y = Point2f_Y(r->origin);
    return point;
}

Point2Rawf* Rectf_TopRight(const Rectf* r, Point2Rawf* point)
{
    DENG_ASSERT(r);
    if(!point) return NULL;
    point->x = Point2f_X(r->origin) + Size2f_Width(r->size);
    point->y = Point2f_Y(r->origin);
    return point;
}

Point2Rawf* Rectf_BottomLeft(const Rectf* r, Point2Rawf* point)
{
    DENG_ASSERT(r);
    if(!point) return NULL;
    point->x = Point2f_X(r->origin);
    point->y = Point2f_Y(r->origin) + Size2f_Height(r->size);
    return point;
}

Point2Rawf* Rectf_BottomRight(const Rectf* r, Point2Rawf* point)
{
    DENG_ASSERT(r);
    if(!point) return NULL;
    point->x = Point2f_X(r->origin) + Size2f_Width(r->size);
    point->y = Point2f_Y(r->origin) + Size2f_Height(r->size);
    return point;
}

Rectf* Rectf_Normalize(Rectf* r)
{
    DENG_ASSERT(r);
    if(Size2f_Width(r->size) < 0)
        Point2f_TranslateX(r->origin, -Size2f_Width(r->size));
    if(Size2f_Height(r->size) < 0)
        Point2f_TranslateY(r->origin, -Size2f_Height(r->size));
    return r;
}

RectRawf* Rectf_Normalized(const Rectf* rect, RectRawf* normalized)
{
    if(!normalized) return NULL;

    if(!rect)
    {
        memset(normalized, 0, sizeof(*normalized));
        return normalized;
    }

    Rectf_Raw(rect, normalized);
    if(normalized->size.width < 0)
        normalized->origin.x -= normalized->size.width;
    if(normalized->size.height < 0)
        normalized->origin.y -= normalized->size.height;

    return normalized;
}

/// @pre  This and @a other have been normalized.
static Rectf* Rectf_UniteRaw2(Rectf* r, const RectRawf* other)
{
    Point2Rawf oldOrigin;
    DENG_ASSERT(r && other);

    Point2f_Raw(r->origin, &oldOrigin);

    Rectf_SetXY(r, MIN_OF(Point2f_X(r->origin), other->origin.x),
                   MIN_OF(Point2f_Y(r->origin), other->origin.y));

    Rectf_SetWidthHeight(r, MAX_OF(oldOrigin.x + Size2f_Width(r->size),
                                   other->origin.x + other->size.width)  - Point2f_X(r->origin),
                            MAX_OF(oldOrigin.y + Size2f_Height(r->size),
                                   other->origin.y + other->size.height) - Point2f_Y(r->origin));

    return r;
}

Rectf* Rectf_UniteRaw(Rectf* r, const RectRawf* other)
{
    RectRawf otherNormalized;

    if(!other) return r;

    memcpy(&otherNormalized, other, sizeof(otherNormalized));
    if(otherNormalized.size.width < 0)
        otherNormalized.origin.x -= otherNormalized.size.width;
    if(otherNormalized.size.height < 0)
        otherNormalized.origin.y -= otherNormalized.size.height;

    return Rectf_UniteRaw2(Rectf_Normalize(r), &otherNormalized);
}

Rectf* Rectf_Unite(Rectf* r, const Rectf* other)
{
    RectRawf rawOther;
    return Rectf_UniteRaw2(Rectf_Normalize(r), Rectf_Normalized(other, &rawOther));
}

RectRawf* Rectf_United(const Rectf* rect, const Rectf* other, RectRawf* united)
{
    RectRawf normA, normB;

    if(!united) return NULL;

    if(!other)
    {
        united->origin.x = Point2f_X(rect->origin);
        united->origin.y = Point2f_Y(rect->origin);
        united->size.width  = Size2f_Width(rect->size);
        united->size.height = Size2f_Height(rect->size);
        return united;
    }

    Rectf_Normalized(rect, &normA);
    Rectf_Normalized(other, &normB);

    united->origin.x = MIN_OF(normA.origin.x, normB.origin.x);
    united->origin.y = MIN_OF(normA.origin.y, normB.origin.y);

    united->size.width  = MAX_OF(normA.origin.x + normA.size.width,
                                 normB.origin.x + normB.size.width)  - united->origin.x;
    united->size.height = MAX_OF(normA.origin.y + normA.size.height,
                                 normB.origin.y + normB.size.height) - united->origin.y;

    return united;
}

boolean Rectf_Equality(const Rectf* r, const Rectf* other)
{
    DENG_ASSERT(r);
    if(!other) return false;
    return r == other || (Point2f_Equality(r->origin, Rectf_Origin(other)) &&
                          Size2f_Equality(r->size, Rectf_Size(other)));
}
