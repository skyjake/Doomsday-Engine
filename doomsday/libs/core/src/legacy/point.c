/**
 * @file point.c
 * 2D points.
 *
 * @authors Copyright © 2013 Daniel Swanson <danij@dengine.net>
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

#include "de/legacy/point.h"
#include "de/legacy/memory.h"

struct point2_s {
    /// Use a "raw" Point natively so that we can simply copy when called upon.
    Point2Raw raw;
};

struct point2f_s {
    /// Use a "raw" Point natively so that we can simply copy when called upon.
    Point2Rawf raw;
};

Point2 *Point2_New(void)
{
    Point2 *p = M_Calloc(sizeof *p);
    return p;
}

Point2 *Point2_NewWithCoords(int x, int y)
{
    Point2 *p = Point2_New();
    Point2_SetXY(p, x, y);
    return p;
}

Point2 *Point2_NewFromRaw(Point2Raw const *rawPoint)
{
    DE_ASSERT(rawPoint);
    return Point2_NewWithCoords(rawPoint->x, rawPoint->y);
}

void Point2_Delete(Point2 *p)
{
    DE_ASSERT(p);
    M_Free(p);
}

dd_bool Point2_IsNull(Point2 const *p)
{
    DE_ASSERT(p);
    return p->raw.x == 0 && p->raw.y == 0;
}

Point2Raw *Point2_Raw(Point2 const *p, Point2Raw *rawPoint)
{
    DE_ASSERT(p);
    if (!rawPoint) return NULL;
    memcpy(rawPoint, &p->raw, sizeof(*rawPoint));
    return rawPoint;
}

int Point2_X(Point2 const *p)
{
    DE_ASSERT(p);
    return p->raw.x;
}

int Point2_Y(Point2 const *p)
{
    DE_ASSERT(p);
    return p->raw.y;
}

void Point2_SetX(Point2 *p, int x)
{
    DE_ASSERT(p);
    p->raw.x = x;
}

void Point2_SetY(Point2 *p, int y)
{
    DE_ASSERT(p);
    p->raw.y = y;
}

int const *Point2_XY(Point2 const *p)
{
    DE_ASSERT(p);
    return p->raw.xy;
}

void Point2_SetXY(Point2 *p, int x, int y)
{
    DE_ASSERT(p);
    p->raw.x = x;
    p->raw.y = y;
}

void Point2_Translate(Point2 *p, Point2Raw const *delta)
{
    DE_ASSERT(p);
    if (!delta) return;
    p->raw.x += delta->x;
    p->raw.y += delta->y;
}

void Point2_TranslateXY(Point2 *p, int x, int y)
{
    DE_ASSERT(p);
    p->raw.x += x;
    p->raw.y += y;
}

void Point2_TranslateX(Point2 *p, int x)
{
    DE_ASSERT(p);
    p->raw.x += x;
}

void Point2_TranslateY(Point2 *p, int y)
{
    DE_ASSERT(p);
    p->raw.y += y;
}

void Point2_Sum(Point2 *p, Point2 const *other)
{
    DE_ASSERT(p && other);
    p->raw.x += Point2_X(other);
    p->raw.y += Point2_Y(other);
}

dd_bool Point2_Equality(Point2 const *p, Point2 const *other)
{
    DE_ASSERT(p && other);
    return p == other || (p->raw.x == Point2_X(other) && p->raw.y == Point2_Y(other));
}

Point2f *Point2f_New(void)
{
    Point2f *p = M_Calloc(sizeof *p);
    return p;
}

Point2f *Point2f_NewWithCoords(double x, double y)
{
    Point2f *p = Point2f_New();
    Point2f_SetXY(p, x, y);
    return p;
}

Point2f *Point2f_NewFromRaw(Point2Rawf const *rawPoint)
{
    DE_ASSERT(rawPoint);
    return Point2f_NewWithCoords(rawPoint->x, rawPoint->y);
}

void Point2f_Delete(Point2f *p)
{
    DE_ASSERT(p);
    M_Free(p);
}

dd_bool Point2f_IsNull(Point2f const *p)
{
    DE_ASSERT(p);
    return p->raw.x == 0 && p->raw.y == 0;
}

Point2Rawf *Point2f_Raw(Point2f const *p, Point2Rawf *rawPoint)
{
    DE_ASSERT(p);
    if (!rawPoint) return NULL;
    memcpy(rawPoint, &p->raw, sizeof(*rawPoint));
    return rawPoint;
}

double Point2f_X(Point2f const *p)
{
    DE_ASSERT(p);
    return p->raw.x;
}

double Point2f_Y(Point2f const *p)
{
    DE_ASSERT(p);
    return p->raw.y;
}

void Point2f_SetX(Point2f *p, double x)
{
    DE_ASSERT(p);
    p->raw.x = x;
}

void Point2f_SetY(Point2f *p, double y)
{
    DE_ASSERT(p);
    p->raw.y = y;
}

double const *Point2f_XY(Point2f const *p)
{
    DE_ASSERT(p);
    return p->raw.xy;
}

void Point2f_SetXY(Point2f *p, double x, double y)
{
    DE_ASSERT(p);
    p->raw.x = x;
    p->raw.y = y;
}

void Point2f_Translate(Point2f *p, Point2Rawf const *delta)
{
    DE_ASSERT(p);
    if (!delta) return;
    p->raw.x += delta->x;
    p->raw.y += delta->y;
}

void Point2f_TranslateXY(Point2f *p, double x, double y)
{
    DE_ASSERT(p);
    p->raw.x += x;
    p->raw.y += y;
}

void Point2f_TranslateX(Point2f *p, double x)
{
    DE_ASSERT(p);
    p->raw.x += x;
}

void Point2f_TranslateY(Point2f *p, double y)
{
    DE_ASSERT(p);
    p->raw.y += y;
}

void Point2f_Sum(Point2f *p, Point2f const *other)
{
    DE_ASSERT(p && other);
    p->raw.x += Point2f_X(other);
    p->raw.y += Point2f_Y(other);
}

dd_bool Point2f_Equality(Point2f const *p, Point2f const *other)
{
    DE_ASSERT(p && other);
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

Point3 *Point3_New(void)
{
    Point3 *p = M_Calloc(sizeof *p);
    return p;
}

Point3 *Point3_NewWithCoords(int x, int y, int z)
{
    Point3 *p = Point3_New();
    Point3_SetXYZ(p, x, y, z);
    return p;
}

Point3 *Point3_NewFromRaw(Point3Raw const *rawPoint)
{
    DE_ASSERT(rawPoint);
    return Point3_NewWithCoords(rawPoint->x, rawPoint->y, rawPoint->z);
}

void Point3_Delete(Point3 *p)
{
    DE_ASSERT(p);
    M_Free(p);
}

dd_bool Point3_IsNull(Point3 const *p)
{
    DE_ASSERT(p);
    return p->raw.x == 0 && p->raw.y == 0 && p->raw.z == 0;
}

Point3Raw *Point3_Raw(Point3 const *p, Point3Raw *rawPoint)
{
    DE_ASSERT(p);
    if (!rawPoint) return NULL;
    memcpy(rawPoint, &p->raw, sizeof(*rawPoint));
    return rawPoint;
}

int Point3_X(Point3 const *p)
{
    DE_ASSERT(p);
    return p->raw.x;
}

int Point3_Y(Point3 const *p)
{
    DE_ASSERT(p);
    return p->raw.y;
}

int Point3_Z(Point3 const *p)
{
    DE_ASSERT(p);
    return p->raw.z;
}

void Point3_SetX(Point3 *p, int x)
{
    DE_ASSERT(p);
    p->raw.x = x;
}

void Point3_SetY(Point3 *p, int y)
{
    DE_ASSERT(p);
    p->raw.y = y;
}

void Point3_SetZ(Point3 *p, int z)
{
    DE_ASSERT(p);
    p->raw.z = z;
}

int const *Point3_XYZ(Point3 const *p)
{
    DE_ASSERT(p);
    return p->raw.xyz;
}

void Point3_SetXYZ(Point3 *p, int x, int y, int z)
{
    DE_ASSERT(p);
    p->raw.x = x;
    p->raw.y = y;
    p->raw.z = z;
}

void Point3_Translate(Point3 *p, int x, int y, int z)
{
    DE_ASSERT(p);
    p->raw.x += x;
    p->raw.y += y;
    p->raw.z += z;
}

void Point3_TranslateX(Point3 *p, int x)
{
    DE_ASSERT(p);
    p->raw.x += x;
}

void Point3_TranslateY(Point3 *p, int y)
{
    DE_ASSERT(p);
    p->raw.y += y;
}

void Point3_TranslateZ(Point3 *p, int z)
{
    DE_ASSERT(p);
    p->raw.z += z;
}

void Point3_Sum(Point3 *p, Point3 const *other)
{
    DE_ASSERT(p && other);
    p->raw.x += Point3_X(other);
    p->raw.y += Point3_Y(other);
    p->raw.z += Point3_Z(other);
}

dd_bool Point3_Equality(Point3 const *p, Point3 const *other)
{
    DE_ASSERT(p && other);
    return p == other || (p->raw.x == Point3_X(other) &&
                          p->raw.y == Point3_Y(other) &&
                          p->raw.z == Point3_Z(other));
}

Point3f *Point3f_New(void)
{
    Point3f *p = M_Calloc(sizeof *p);
    return p;
}

Point3f *Point3f_NewWithCoords(double x, double y, double z)
{
    Point3f *p = Point3f_New();
    Point3f_SetXYZ(p, x, y, z);
    return p;
}

Point3f *Point3f_NewFromRaw(Point3Rawf const *rawPoint)
{
    DE_ASSERT(rawPoint);
    return Point3f_NewWithCoords(rawPoint->x, rawPoint->y, rawPoint->z);
}

void Point3f_Delete(Point3f *p)
{
    DE_ASSERT(p);
    M_Free(p);
}

dd_bool Point3f_IsNull(Point3f const *p)
{
    DE_ASSERT(p);
    return p->raw.x == 0 && p->raw.y == 0 && p->raw.z == 0;
}

Point3Rawf *Point3f_Raw(Point3f const *p, Point3Rawf *rawPoint)
{
    DE_ASSERT(p);
    if (!rawPoint) return NULL;
    memcpy(rawPoint, &p->raw, sizeof(*rawPoint));
    return rawPoint;
}

double Point3f_X(Point3f const *p)
{
    DE_ASSERT(p);
    return p->raw.x;
}

double Point3f_Y(Point3f const *p)
{
    DE_ASSERT(p);
    return p->raw.y;
}

double Point3f_Z(Point3f const *p)
{
    DE_ASSERT(p);
    return p->raw.z;
}

void Point3f_SetX(Point3f *p, double x)
{
    DE_ASSERT(p);
    p->raw.x = x;
}

void Point3f_SetY(Point3f *p, double y)
{
    DE_ASSERT(p);
    p->raw.y = y;
}

void Point3f_SetZ(Point3f *p, double z)
{
    DE_ASSERT(p);
    p->raw.z = z;
}

double const *Point3f_XYZ(Point3f const *p)
{
    DE_ASSERT(p);
    return p->raw.xyz;
}

void Point3f_SetXYZ(Point3f *p, double x, double y, double z)
{
    DE_ASSERT(p);
    p->raw.x = x;
    p->raw.y = y;
    p->raw.z = z;
}

void Point3f_Translate(Point3f *p, double x, double y, double z)
{
    DE_ASSERT(p);
    p->raw.x += x;
    p->raw.y += y;
    p->raw.z += z;
}

void Point3f_TranslateX(Point3f *p, double x)
{
    DE_ASSERT(p);
    p->raw.x += x;
}

void Point3f_TranslateY(Point3f *p, double y)
{
    DE_ASSERT(p);
    p->raw.y += y;
}

void Point3f_TranslateZ(Point3f *p, double z)
{
    DE_ASSERT(p);
    p->raw.z += z;
}

void Point3f_Sum(Point3f *p, Point3f const *other)
{
    DE_ASSERT(p && other);
    p->raw.x += Point3f_X(other);
    p->raw.y += Point3f_Y(other);
    p->raw.z += Point3f_Z(other);
}

dd_bool Point3f_Equality(Point3f const *p, Point3f const *other)
{
    DE_ASSERT(p && other);
    return p == other || (p->raw.x == Point3f_X(other) &&
                          p->raw.y == Point3f_Y(other) &&
                          p->raw.z == Point3f_Z(other));
}
