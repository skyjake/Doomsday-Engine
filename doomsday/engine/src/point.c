/**\file point.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2012 Daniel Swanson <danij@dengine.net>
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
    /// Use a "raw" Point natively so that we can simply copy when called upon.
    Point2Raw raw;
};

struct point2f_s {
    /// Use a "raw" Point natively so that we can simply copy when called upon.
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

Point2Raw* Point2_Raw(const Point2* p, Point2Raw* rawPoint)
{
    assert(p);
    if(!rawPoint) return NULL;
    memcpy(rawPoint, &p->raw, sizeof(*rawPoint));
    return rawPoint;
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

void Point2_Translate(Point2* p, const Point2Raw* delta)
{
    assert(p);
    if(!delta) return;
    p->raw.x += delta->x;
    p->raw.y += delta->y;
}

void Point2_TranslateXY(Point2* p, int x, int y)
{
    assert(p);
    p->raw.x += x;
    p->raw.y += y;
}

void Point2_TranslateX(Point2* p, int x)
{
    assert(p);
    p->raw.x += x;
}

void Point2_TranslateY(Point2* p, int y)
{
    assert(p);
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
    return p == other || (p->raw.x == Point2_X(other) && p->raw.y == Point2_Y(other));
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

Point2Rawf* Point2f_Raw(const Point2f* p, Point2Rawf* rawPoint)
{
    assert(p);
    if(!rawPoint) return NULL;
    memcpy(rawPoint, &p->raw, sizeof(*rawPoint));
    return rawPoint;
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

void Point2f_Translate(Point2f* p, const Point2Rawf* delta)
{
    assert(p);
    if(!delta) return;
    p->raw.x += delta->x;
    p->raw.y += delta->y;
}

void Point2f_TranslateXY(Point2f* p, double x, double y)
{
    assert(p);
    p->raw.x += x;
    p->raw.y += y;
}

void Point2f_TranslateX(Point2f* p, double x)
{
    assert(p);
    p->raw.x += x;
}

void Point2f_TranslateY(Point2f* p, double y)
{
    assert(p);
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
    return p == other || (p->raw.x == Point2f_X(other) && p->raw.y == Point2f_Y(other));
}

struct point3_s {
    /// Use a "raw" Point natively so that we can simply copy when called upon.
    Point3Raw raw;
};

struct point3f_s {
    /// Use a "raw" Point natively so that we can simply copy when called upon.
    Point3Rawf raw;
};

Point3* Point3_New(void)
{
    Point3* p = calloc(1, sizeof *p);
    return p;
}

Point3* Point3_NewWithCoords(int x, int y, int z)
{
    Point3* p = Point3_New();
    Point3_SetXYZ(p, x, y, z);
    return p;
}

Point3* Point3_NewFromRaw(const Point3Raw* rawPoint)
{
    assert(rawPoint);
    return Point3_NewWithCoords(rawPoint->x, rawPoint->y, rawPoint->z);
}

void Point3_Delete(Point3* p)
{
    assert(p);
    free(p);
}

boolean Point3_IsNull(const Point3* p)
{
    assert(p);
    return p->raw.x == 0 && p->raw.y == 0 && p->raw.z == 0;
}

Point3Raw* Point3_Raw(const Point3* p, Point3Raw* rawPoint)
{
    assert(p);
    if(!rawPoint) return NULL;
    memcpy(rawPoint, &p->raw, sizeof(*rawPoint));
    return rawPoint;
}

int Point3_X(const Point3* p)
{
    assert(p);
    return p->raw.x;
}

int Point3_Y(const Point3* p)
{
    assert(p);
    return p->raw.y;
}

int Point3_Z(const Point3* p)
{
    assert(p);
    return p->raw.z;
}

void Point3_SetX(Point3* p, int x)
{
    assert(p);
    p->raw.x = x;
}

void Point3_SetY(Point3* p, int y)
{
    assert(p);
    p->raw.y = y;
}

void Point3_SetZ(Point3* p, int z)
{
    assert(p);
    p->raw.z = z;
}

const int* Point3_XYZ(const Point3* p)
{
    assert(p);
    return p->raw.xyz;
}

void Point3_SetXYZ(Point3* p, int x, int y, int z)
{
    assert(p);
    p->raw.x = x;
    p->raw.y = y;
    p->raw.z = z;
}

void Point3_Translate(Point3* p, int x, int y, int z)
{
    assert(p);
    p->raw.x += x;
    p->raw.y += y;
    p->raw.z += z;
}

void Point3_TranslateX(Point3* p, int x)
{
    assert(p);
    p->raw.x += x;
}

void Point3_TranslateY(Point3* p, int y)
{
    assert(p);
    p->raw.y += y;
}

void Point3_TranslateZ(Point3* p, int z)
{
    assert(p);
    p->raw.z += z;
}

void Point3_Sum(Point3* p, const Point3* other)
{
    assert(p && other);
    p->raw.x += Point3_X(other);
    p->raw.y += Point3_Y(other);
    p->raw.z += Point3_Z(other);
}

boolean Point3_Equality(const Point3* p, const Point3* other)
{
    assert(p && other);
    return p == other || (p->raw.x == Point3_X(other) &&
                          p->raw.y == Point3_Y(other) &&
                          p->raw.z == Point3_Z(other));
}

Point3f* Point3f_New(void)
{
    Point3f* p = calloc(1, sizeof *p);
    return p;
}

Point3f* Point3f_NewWithCoords(double x, double y, double z)
{
    Point3f* p = Point3f_New();
    Point3f_SetXYZ(p, x, y, z);
    return p;
}

Point3f* Point3f_NewFromRaw(const Point3Rawf* rawPoint)
{
    assert(rawPoint);
    return Point3f_NewWithCoords(rawPoint->x, rawPoint->y, rawPoint->z);
}

void Point3f_Delete(Point3f* p)
{
    assert(p);
    free(p);
}

boolean Point3f_IsNull(const Point3f* p)
{
    assert(p);
    return p->raw.x == 0 && p->raw.y == 0 && p->raw.z == 0;
}

Point3Rawf* Point3f_Raw(const Point3f* p, Point3Rawf* rawPoint)
{
    assert(p);
    if(!rawPoint) return NULL;
    memcpy(rawPoint, &p->raw, sizeof(*rawPoint));
    return rawPoint;
}

double Point3f_X(const Point3f* p)
{
    assert(p);
    return p->raw.x;
}

double Point3f_Y(const Point3f* p)
{
    assert(p);
    return p->raw.y;
}

double Point3f_Z(const Point3f* p)
{
    assert(p);
    return p->raw.z;
}

void Point3f_SetX(Point3f* p, double x)
{
    assert(p);
    p->raw.x = x;
}

void Point3f_SetY(Point3f* p, double y)
{
    assert(p);
    p->raw.y = y;
}

void Point3f_SetZ(Point3f* p, double z)
{
    assert(p);
    p->raw.z = z;
}

const double* Point3f_XYZ(const Point3f* p)
{
    assert(p);
    return p->raw.xyz;
}

void Point3f_SetXYZ(Point3f* p, double x, double y, double z)
{
    assert(p);
    p->raw.x = x;
    p->raw.y = y;
    p->raw.z = z;
}

void Point3f_Translate(Point3f* p, double x, double y, double z)
{
    assert(p);
    p->raw.x += x;
    p->raw.y += y;
    p->raw.z += z;
}

void Point3f_TranslateX(Point3f* p, double x)
{
    assert(p);
    p->raw.x += x;
}

void Point3f_TranslateY(Point3f* p, double y)
{
    assert(p);
    p->raw.y += y;
}

void Point3f_TranslateZ(Point3f* p, double z)
{
    assert(p);
    p->raw.z += z;
}

void Point3f_Sum(Point3f* p, const Point3f* other)
{
    assert(p && other);
    p->raw.x += Point3f_X(other);
    p->raw.y += Point3f_Y(other);
    p->raw.z += Point3f_Z(other);
}

boolean Point3f_Equality(const Point3f* p, const Point3f* other)
{
    assert(p && other);
    return p == other || (p->raw.x == Point3f_X(other) &&
                          p->raw.y == Point3f_Y(other) &&
                          p->raw.z == Point3f_Z(other));
}
