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
    Rect_SetOrigin2(r, x, y);
    Rect_SetSize2(r, width, height);
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

void Rect_SetOrigin2(Rect* r, int x, int y)
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

void Rect_SetSize2(Rect* r, int width, int height)
{
    assert(r);
    Size2_SetWidthHeight(r->size, width, height);
}

boolean Rect_Equality(const Rect* r, const Rect* other)
{
    assert(r && other);
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
    Rectf_SetOrigin2(r, x, y);
    Rectf_SetSize2(r, width, height);
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

void Rectf_SetOrigin2(Rectf* r, double x, double y)
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

void Rectf_SetSize2(Rectf* r, double width, double height)
{
    assert(r);
    Size2f_SetWidthHeight(r->size, width, height);
}

boolean Rectf_Equality(const Rectf* r, const Rectf* other)
{
    assert(r && other);
    return r == other || Point2f_Equality(r->origin, Rectf_Origin(other)) &&
                         Size2f_Equality(r->size, Rectf_Size(other));
}
