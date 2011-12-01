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

struct point2_s {
    /// Use a "raw" point natively so that we can return immutable references
    /// to this directly (no dynamic allocation).
    Point2Raw raw;
};

struct point2f_s {
    /// Use a "raw" point natively so that we can return immutable references
    /// to this directly (no dynamic allocation).
    Point2Rawf raw;
};

Point2* Point2_New(void)
{
    Point2* p = calloc(1, sizeof *p);
    return p;
}

Point2* Point2_NewWithCoords(int x, int y)
{
    Point2* p = Point2_New();
    Point2_SetXY(p, x, y);
    return p;
}

Point2* Point2_NewFromRaw(const Point2Raw* rawPoint)
{
    assert(rawPoint);
    return Point2_NewWithCoords(rawPoint->x, rawPoint->y);
}

void Point2_Delete(Point2* p)
{
    assert(p);
    free(p);
}

boolean Point2_IsNull(const Point2* p)
{
    assert(p);
    return p->raw.x == 0 && p->raw.y == 0;
}

const Point2Raw* Point2_ToRaw(const Point2* p)
{
    assert(p);
    return &p->raw;
}

int Point2_X(const Point2* p)
{
    assert(p);
    return p->raw.x;
}

int Point2_Y(const Point2* p)
{
    assert(p);
    return p->raw.y;
}

void Point2_SetX(Point2* p, int x)
{
    assert(p);
    p->raw.x = x;
}

void Point2_SetY(Point2* p, int y)
{
    assert(p);
    p->raw.y = y;
}

const int* Point2_XY(const Point2* p)
{
    assert(p);
    return p->raw.xy;
}

void Point2_SetXY(Point2* p, int x, int y)
{
    assert(p);
    p->raw.x = x;
    p->raw.y = y;
}

void Point2_Translate(Point2* p, int x, int y)
{
    assert(p);
    p->raw.x += x;
    p->raw.y += y;
}

void Point2_Sum(Point2* p, const Point2* other)
{
    assert(p && other);
    p->raw.x += Point2_X(other);
    p->raw.y += Point2_Y(other);
}

boolean Point2_Equality(const Point2* p, const Point2* other)
{
    assert(p && other);
    return p == other || p->raw.x == Point2_X(other) && p->raw.y == Point2_Y(other);
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

void Point2f_Translate(Point2* p, double x, double y)
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
