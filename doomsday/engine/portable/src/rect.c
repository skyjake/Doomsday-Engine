/**\file rect.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2011 Daniel Swanson <danij@dengine.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
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

#include <de_base.h>
#include <de_console.h>

#include "point.h"
#include "size.h"
#include "rect.h"

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
    Rect* r = malloc(sizeof *r);
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
    assert(rawRect);
    return Rect_NewWithOriginSize2(rawRect->origin.x, rawRect->origin.y,
                                   rawRect->size.width, rawRect->size.height);
}

void Rect_Delete(Rect* r)
{
    assert(r);
    Point2_Delete(r->origin);
    Size2_Delete(r->size);
    free(r);
}

boolean Rect_IsNull(const Rect* r)
{
    assert(r);
    return Size2_IsNull(r->size);
}

int Rect_Width(const Rect* r)
{
    assert(r);
    return Size2_Width(r->size);
}

int Rect_Height(const Rect* r)
{
    assert(r);
    return Size2_Height(r->size);
}

void Rect_SetWidth(Rect* r, int width)
{
    assert(r);
    Size2_SetWidth(r->size, width);
}

void Rect_SetHeight(Rect* r, int height)
{
    assert(r);
    Size2_SetHeight(r->size, height);
}

const Point2* Rect_Origin(const Rect* r)
{
    assert(r);
    return r->origin;
}

void Rect_SetOrigin(Rect* r, const Point2* origin)
{
    assert(r);
    Point2_SetXY(r->origin, Point2_X(origin), Point2_Y(origin));
}

void Rect_SetXY(Rect* r, int x, int y)
{
    assert(r);
    Point2_SetXY(r->origin, x, y);
}

const Size2* Rect_Size(const Rect* r)
{
    assert(r);
    return r->size;
}

void Rect_SetSize(Rect* r, const Size2* size)
{
    assert(r);
    Size2_SetWidthHeight(r->size, Size2_Width(size), Size2_Height(size));
}

void Rect_SetWidthHeight(Rect* r, int width, int height)
{
    assert(r);
    Size2_SetWidthHeight(r->size, width, height);
}

Point2Raw* Rect_TopLeft(const Rect* r, Point2Raw* point)
{
    assert(r);
    if(!point) return NULL;
    point->x = Point2_X(r->origin);
    point->y = Point2_Y(r->origin);
    return point;
}

Point2Raw* Rect_TopRight(const Rect* r, Point2Raw* point)
{
    assert(r);
    if(!point) return NULL;
    point->x = Point2_X(r->origin) + Size2_Width(r->size);
    point->y = Point2_Y(r->origin);
    return point;
}

Point2Raw* Rect_BottomLeft(const Rect* r, Point2Raw* point)
{
    assert(r);
    if(!point) return NULL;
    point->x = Point2_X(r->origin);
    point->y = Point2_Y(r->origin) + Size2_Height(r->size);
    return point;
}

Point2Raw* Rect_BottomRight(const Rect* r, Point2Raw* point)
{
    assert(r);
    if(!point) return NULL;
    point->x = Point2_X(r->origin) + Size2_Width(r->size);
    point->y = Point2_Y(r->origin) + Size2_Height(r->size);
    return point;
}

Rect* Rect_Normalize(Rect* r)
{
    assert(r);
    if(Size2_Width(r->size) < 0)
        Point2_TranslateX(r->origin, -Size2_Width(r->size));
    if(Size2_Height(r->size) < 0)
        Point2_TranslateY(r->origin, -Size2_Height(r->size));
    return r;
}

RectRaw* Rect_Normalized(const Rect* r, RectRaw* normalized)
{
    assert(r);
    if(!normalized) return NULL;

    memcpy(&normalized->origin, Point2_ToRaw(Rect_Origin(r)), sizeof(normalized->origin));
    memcpy(&normalized->size,   Size2_ToRaw(Rect_Size(r)), sizeof(normalized->size));

    if(normalized->size.width < 0)
        normalized->origin.x -= normalized->size.width;
    if(normalized->size.height < 0)
        normalized->origin.y -= normalized->size.height;

    return normalized;
}

Rect* Rect_Unite(Rect* r, const Rect* other)
{
    RectRaw otherNormalized;
    assert(r);

    if(!other) return r;

    Rect_Normalize(r);
    Rect_Normalized(other, &otherNormalized);

    Rect_SetXY(r, MIN_OF(Point2_X(r->origin), otherNormalized.origin.x),
                  MIN_OF(Point2_Y(r->origin), otherNormalized.origin.y));

    Rect_SetWidthHeight(r, MAX_OF(Point2_X(r->origin) + Size2_Width(r->size),
                                  otherNormalized.origin.x + otherNormalized.size.width)  - Point2_X(r->origin),
                           MAX_OF(Point2_Y(r->origin) + Size2_Height(r->size),
                                  otherNormalized.origin.y + otherNormalized.size.height) - Point2_Y(r->origin));

    return r;
}

RectRaw* Rect_United(const Rect* r, const Rect* other, RectRaw* united)
{
    RectRaw normA, normB;
    assert(r);

    if(!united) return NULL;

    if(!other)
    {
        united->origin.x = Point2_X(r->origin);
        united->origin.y = Point2_Y(r->origin);
        united->size.width  = Size2_Width(r->size);
        united->size.height = Size2_Height(r->size);
        return united;
    }

    Rect_Normalized(r, &normA);
    Rect_Normalized(r, &normB);

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
    assert(r);
    if(!other) return false;
    return r == other || Point2_Equality(r->origin, Rect_Origin(other)) &&
                         Size2_Equality(r->size, Rect_Size(other));
}

Rectf* Rectf_New(void)
{
    Rectf* r = calloc(1, sizeof *r);
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
    assert(rawRect);
    return Rectf_NewWithOriginSize2(rawRect->origin.x, rawRect->origin.y,
                                    rawRect->size.width, rawRect->size.height);
}

void Rectf_Delete(Rectf* r)
{
    assert(r);
    Point2f_Delete(r->origin);
    Size2f_Delete(r->size);
    free(r);
}

boolean Rectf_IsNull(const Rectf* r)
{
    assert(r);
    return Size2f_IsNull(r->size);
}

double Rectf_Width(const Rectf* r)
{
    assert(r);
    return Size2f_Width(r->size);
}

double Rectf_Height(const Rectf* r)
{
    assert(r);
    return Size2f_Height(r->size);
}

const Point2f* Rectf_Origin(const Rectf* r)
{
    assert(r);
    return r->origin;
}

void Rectf_SetOrigin(Rectf* r, const Point2f* origin)
{
    assert(r);
    Point2f_SetXY(r->origin, Point2f_X(origin), Point2f_Y(origin));
}

void Rectf_SetXY(Rectf* r, double x, double y)
{
    assert(r);
    Point2f_SetXY(r->origin, x, y);
}

void Rectf_SetWidth(Rectf* r, double width)
{
    assert(r);
    Size2f_SetWidth(r->size, width);
}

void Rectf_SetHeight(Rectf* r, double height)
{
    assert(r);
    Size2f_SetHeight(r->size, height);
}

const Size2f* Rectf_Size(const Rectf* r)
{
    assert(r);
    return r->size;
}

void Rectf_SetSize(Rectf* r, const Size2f* size)
{
    assert(r);
    Size2f_SetWidthHeight(r->size, Size2f_Width(size), Size2f_Height(size));
}

void Rectf_SetWidthHeight(Rectf* r, double width, double height)
{
    assert(r);
    Size2f_SetWidthHeight(r->size, width, height);
}

Point2Rawf* Rectf_TopLeft(const Rectf* r, Point2Rawf* point)
{
    assert(r);
    if(!point) return NULL;
    point->x = Point2f_X(r->origin);
    point->y = Point2f_Y(r->origin);
    return point;
}

Point2Rawf* Rectf_TopRight(const Rectf* r, Point2Rawf* point)
{
    assert(r);
    if(!point) return NULL;
    point->x = Point2f_X(r->origin) + Size2f_Width(r->size);
    point->y = Point2f_Y(r->origin);
    return point;
}

Point2Rawf* Rectf_BottomLeft(const Rectf* r, Point2Rawf* point)
{
    assert(r);
    if(!point) return NULL;
    point->x = Point2f_X(r->origin);
    point->y = Point2f_Y(r->origin) + Size2f_Height(r->size);
    return point;
}

Point2Rawf* Rectf_BottomRight(const Rectf* r, Point2Rawf* point)
{
    assert(r);
    if(!point) return NULL;
    point->x = Point2f_X(r->origin) + Size2f_Width(r->size);
    point->y = Point2f_Y(r->origin) + Size2f_Height(r->size);
    return point;
}

Rectf* Rectf_Normalize(Rectf* r)
{
    assert(r);
    if(Size2f_Width(r->size) < 0)
        Point2f_TranslateX(r->origin, -Size2f_Width(r->size));
    if(Size2f_Height(r->size) < 0)
        Point2f_TranslateY(r->origin, -Size2f_Height(r->size));
    return r;
}

RectRawf* Rectf_Normalized(const Rectf* r, RectRawf* normalized)
{
    assert(r);
    if(!normalized) return NULL;

    memcpy(&normalized->origin, Point2f_ToRaw(Rectf_Origin(r)), sizeof(normalized->origin));
    memcpy(&normalized->size,   Size2f_ToRaw(Rectf_Size(r)), sizeof(normalized->size));

    if(normalized->size.width < 0)
        normalized->origin.x -= normalized->size.width;
    if(normalized->size.height < 0)
        normalized->origin.y -= normalized->size.height;

    return normalized;
}

Rectf* Rectf_Unite(Rectf* r, const Rectf* other)
{
    RectRawf otherNormalized;
    assert(r);

    if(!other) return r;

    Rectf_Normalize(r);
    Rectf_Normalized(other, &otherNormalized);

    Rectf_SetXY(r, MIN_OF(Point2f_X(r->origin), otherNormalized.origin.x),
                   MIN_OF(Point2f_Y(r->origin), otherNormalized.origin.y));

    Rectf_SetWidthHeight(r, MAX_OF(Point2f_X(r->origin) + Size2f_Width(r->size),
                                   otherNormalized.origin.x + otherNormalized.size.width)  - Point2f_X(r->origin),
                            MAX_OF(Point2f_Y(r->origin) + Size2f_Height(r->size),
                                   otherNormalized.origin.y + otherNormalized.size.height) - Point2f_Y(r->origin));

    return r;
}

RectRawf* Rectf_United(const Rectf* r, const Rectf* other, RectRawf* united)
{
    RectRawf normA, normB;
    assert(r);

    if(!united) return NULL;

    if(!other)
    {
        united->origin.x = Point2f_X(r->origin);
        united->origin.y = Point2f_Y(r->origin);
        united->size.width  = Size2f_Width(r->size);
        united->size.height = Size2f_Height(r->size);
        return united;
    }

    Rectf_Normalized(r, &normA);
    Rectf_Normalized(r, &normB);

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
    assert(r);
    if(!other) return false;
    return r == other || Point2f_Equality(r->origin, Rectf_Origin(other)) &&
                         Size2f_Equality(r->size, Rectf_Size(other));
}
