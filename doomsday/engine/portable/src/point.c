/**\file point.c
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

struct point2i_s {
    /// Use a "raw" point natively so that we can return immutable references
    /// to this directly (no dynamic allocation).
    Point2Rawi raw;
};

struct point2f_s {
    /// Use a "raw" point natively so that we can return immutable references
    /// to this directly (no dynamic allocation).
    Point2Rawf raw;
};

Point2i* Point2i_New(void)
{
    Point2i* p = calloc(1, sizeof *p);
    return p;
}

Point2i* Point2i_NewWithCoords(int x, int y)
{
    Point2i* p = Point2i_New();
    Point2i_SetXY(p, x, y);
    return p;
}

Point2i* Point2i_NewFromRaw(const Point2Rawi* rawPoint)
{
    assert(rawPoint);
    return Point2i_NewWithCoords(rawPoint->x, rawPoint->y);
}

void Point2i_Delete(Point2i* p)
{
    assert(p);
    free(p);
}

boolean Point2i_IsNull(const Point2i* p)
{
    assert(p);
    return p->raw.x == 0 && p->raw.y == 0;
}

const Point2Rawi* Point2i_ToRaw(const Point2i* p)
{
    assert(p);
    return &p->raw;
}

int Point2i_X(const Point2i* p)
{
    assert(p);
    return p->raw.x;
}

int Point2i_Y(const Point2i* p)
{
    assert(p);
    return p->raw.y;
}

void Point2i_SetX(Point2i* p, int x)
{
    assert(p);
    p->raw.x = x;
}

void Point2i_SetY(Point2i* p, int y)
{
    assert(p);
    p->raw.y = y;
}

const int* Point2i_XY(const Point2i* p)
{
    assert(p);
    return p->raw.xy;
}

void Point2i_SetXY(Point2i* p, int x, int y)
{
    assert(p);
    p->raw.x = x;
    p->raw.y = y;
}

void Point2i_Translate(Point2i* p, int x, int y)
{
    assert(p);
    p->raw.x += x;
    p->raw.y += y;
}

void Point2i_Sum(Point2i* p, const Point2i* other)
{
    assert(p && other);
    p->raw.x += Point2i_X(other);
    p->raw.y += Point2i_Y(other);
}

boolean Point2i_Equality(const Point2i* p, const Point2i* other)
{
    assert(p && other);
    return p == other || p->raw.x == Point2i_X(other) && p->raw.y == Point2i_Y(other);
}

Point2f* Point2f_New(void)
{
    Point2f* p = calloc(1, sizeof *p);
    return p;
}

Point2f* Point2f_NewWithCoords(double x, double y)
{
    Point2f* p = Point2f_New();
    Point2f_SetXY(p, x, y);
    return p;
}

Point2f* Point2f_NewFromRaw(const Point2Rawf* rawPoint)
{
    assert(rawPoint);
    return Point2f_NewWithCoords(rawPoint->x, rawPoint->y);
}

void Point2f_Delete(Point2f* p)
{
    assert(p);
    free(p);
}

boolean Point2f_IsNull(const Point2f* p)
{
    assert(p);
    return p->raw.x == 0 && p->raw.y == 0;
}

const Point2Rawf* Point2f_ToRaw(const Point2f* p)
{
    return &p->raw;
}

double Point2f_X(const Point2f* p)
{
    assert(p);
    return p->raw.x;
}

double Point2f_Y(const Point2f* p)
{
    assert(p);
    return p->raw.y;
}

void Point2f_SetX(Point2f* p, double x)
{
    assert(p);
    p->raw.x = x;
}

void Point2f_SetY(Point2f* p, double y)
{
    assert(p);
    p->raw.y = y;
}

const double* Point2f_XY(const Point2f* p)
{
    assert(p);
    return p->raw.xy;
}

void Point2f_SetXY(Point2f* p, double x, double y)
{
    assert(p);
    p->raw.x = x;
    p->raw.y = y;
}

void Point2f_Translate(Point2i* p, double x, double y)
{
    assert(p);
    p->raw.x += x;
    p->raw.y += y;
}

void Point2f_Sum(Point2f* p, const Point2f* other)
{
    assert(p && other);
    p->raw.x += Point2f_X(other);
    p->raw.y += Point2f_Y(other);
}

boolean Point2f_Equality(const Point2f* p, const Point2f* other)
{
    assert(p && other);
    return p == other || p->raw.x == Point2f_X(other) && p->raw.y == Point2f_Y(other);
}
