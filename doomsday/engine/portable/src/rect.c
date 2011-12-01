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

struct recti_s {
    Point2i* origin;
    Size2i* size;
};

struct rectf_s {
    Point2f* origin;
    Size2f* size;
};

Recti* Recti_New(void)
{
    Recti* r = malloc(sizeof *r);
    r->origin = Point2i_New();
    r->size = Size2i_New();
    return r;
}

Recti* Recti_NewWithOriginSize(const Point2i* origin, const Size2i* size)
{
    Recti* r = Recti_New();
    Recti_SetOrigin(r, origin);
    Recti_SetSize(r, size);
    return r;
}

Recti* Recti_NewWithOriginSize2(int x, int y, int width, int height)
{
    Recti* r = Recti_New();
    Recti_SetOrigin2(r, x, y);
    Recti_SetSize2(r, width, height);
    return r;
}

Recti* Recti_NewFromRaw(const RectRawi* rawRect)
{
    assert(rawRect);
    return Recti_NewWithOriginSize2(rawRect->origin.x, rawRect->origin.y,
                                    rawRect->size.width, rawRect->size.height);
}

void Recti_Delete(Recti* r)
{
    assert(r);
    Point2i_Delete(r->origin);
    Size2i_Delete(r->size);
    free(r);
}

boolean Recti_IsNull(const Recti* r)
{
    assert(r);
    return Size2i_IsNull(r->size);
}

int Recti_Width(const Recti* r)
{
    assert(r);
    return Size2i_Width(r->size);
}

int Recti_Height(const Recti* r)
{
    assert(r);
    return Size2i_Height(r->size);
}

void Recti_SetWidth(Recti* r, int width)
{
    assert(r);
    Size2i_SetWidth(r->size, width);
}

void Recti_SetHeight(Recti* r, int height)
{
    assert(r);
    Size2i_SetHeight(r->size, height);
}

const Point2i* Recti_Origin(const Recti* r)
{
    assert(r);
    return r->origin;
}

void Recti_SetOrigin(Recti* r, const Point2i* origin)
{
    assert(r);
    Point2i_SetXY(r->origin, Point2i_X(origin), Point2i_Y(origin));
}

void Recti_SetOrigin2(Recti* r, int x, int y)
{
    assert(r);
    Point2i_SetXY(r->origin, x, y);
}

const Size2i* Recti_Size(const Recti* r)
{
    assert(r);
    return r->size;
}

void Recti_SetSize(Recti* r, const Size2i* size)
{
    assert(r);
    Size2i_SetWidthHeight(r->size, Size2i_Width(size), Size2i_Height(size));
}

void Recti_SetSize2(Recti* r, int width, int height)
{
    assert(r);
    Size2i_SetWidthHeight(r->size, width, height);
}

boolean Recti_Equality(const Recti* r, const Recti* other)
{
    assert(r && other);
    return r == other || Point2i_Equality(r->origin, Recti_Origin(other)) &&
                         Size2i_Equality(r->size, Recti_Size(other));
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
