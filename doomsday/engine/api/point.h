/**
 * @file point.h
 * 2D points. @ingroup math
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

#ifndef LIBDENG_DATA_POINT_H
#define LIBDENG_DATA_POINT_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * 2D point with integer values. A handy POD structure for easy manipulation of
 * two dimensional plane points. @ingroup math
 */
typedef struct Point2Raw_s {
    union {
        struct {
            int x;
            int y;
        };
        int xy[2];
    };
} Point2Raw;

struct point2_s; // The Point2f instance (opaque).

/**
 * Point2 instance. Class for manipulating 2D plane points using integer precision.
 * Use Point2_New() or one of the other constructors to instantiate.
 */
typedef struct point2_s Point2;

Point2* Point2_New(void);
Point2* Point2_NewWithCoords(int x, int y);
Point2* Point2_NewFromRaw(const Point2Raw* rawPoint);
void Point2_Delete(Point2* point);

Point2Raw* Point2_Raw(const Point2* point, Point2Raw* rawPoint);

boolean Point2_IsNull(const Point2* point);

int Point2_X(const Point2* point);
int Point2_Y(const Point2* point);

void Point2_SetX(Point2* point, int x);
void Point2_SetY(Point2* point, int y);

const int* Point2_XY(const Point2* point);
void Point2_SetXY(Point2* point, int x, int y);

void Point2_Translate(Point2* point, const Point2Raw* delta);
void Point2_TranslateXY(Point2* point, int x, int y);
void Point2_TranslateX(Point2* point, int x);
void Point2_TranslateY(Point2* point, int y);

void Point2_Sum(Point2* point, const Point2* other);

boolean Point2_Equality(const Point2* point, const Point2* other);

/**
 * 2D point with floating point values. A handy POD structure for easy
 * manipulation of two dimensional plane points.
 * @ingroup math
 */
typedef struct Point2Rawf_s {
    union {
        struct {
            double x;
            double y;
        };
        double xy[2];
    };
} Point2Rawf;

struct point2f_s; // The Point2f instance (opaque).

/**
 * Point2f instance. Class for manipulating 2D plane points using floating-point precision.
 * Use Point2f_New() or one of the other constructors to instantiate.
 */
typedef struct point2f_s Point2f;

Point2f* Point2f_New(void);
Point2f* Point2f_NewWithCoords(double x, double y);
Point2f* Point2f_NewFromRaw(const Point2Rawf* rawPoint);
void Point2f_Delete(Point2f* point);

Point2Rawf* Point2f_Raw(const Point2f* point, Point2Rawf* rawPoint);

boolean Point2f_IsNull(const Point2f* point);

double Point2f_X(const Point2f* point);
double Point2f_Y(const Point2f* point);

void Point2f_SetX(Point2f* point, double x);
void Point2f_SetY(Point2f* point, double y);

const double* Point2f_XY(const Point2f* point);
void Point2f_SetXY(Point2f* point, double x, double y);

void Point2f_Translate(Point2f* point, const Point2Rawf* delta);
void Point2f_TranslateXY(Point2f* point, double x, double y);
void Point2f_TranslateX(Point2f* point, double x);
void Point2f_TranslateY(Point2f* point, double y);

void Point2f_Sum(Point2f* point, const Point2f* other);

boolean Point2f_Equality(const Point2f* point, const Point2f* other);

/**
 * 3D point with integer values. A handy POD structure for easy manipulation of
 * three dimensional space points. @ingroup math
 */
typedef struct Point3Raw_s {
    union {
        struct {
            int x;
            int y;
            int z;
        };
        int xy[2];
        int xyz[3];
    };
} Point3Raw;

struct point3_s; // The Point3f instance (opaque).

/**
 * Point3 instance. Class for manipulating 3D plane points using integer precision.
 * Use Point3_New() or one of the other constructors to instantiate.
 */
typedef struct point3_s Point3;

Point3* Point3_New(void);
Point3* Point3_NewWithCoords(int x, int y, int z);
Point3* Point3_NewFromRaw(const Point3Raw* rawPoint);
void Point3_Delete(Point3* point);

Point3Raw* Point3_Raw(const Point3* point, Point3Raw* rawPoint);

boolean Point3_IsNull(const Point3* point);

int Point3_X(const Point3* point);
int Point3_Y(const Point3* point);
int Point3_Z(const Point3* point);

void Point3_SetX(Point3* point, int x);
void Point3_SetY(Point3* point, int y);
void Point3_SetZ(Point3* point, int z);

const int* Point3_XYZ(const Point3* point);
void Point3_SetXYZ(Point3* point, int x, int y, int z);

void Point3_Translate(Point3* point, int x, int y, int z);
void Point3_TranslateX(Point3* point, int x);
void Point3_TranslateY(Point3* point, int y);
void Point3_TranslateZ(Point3* point, int z);

void Point3_Sum(Point3* point, const Point3* other);

boolean Point3_Equality(const Point3* point, const Point3* other);

/**
 * 3D point with floating point values. A handy POD structure for easy
 * manipulation of three dimensional space points.
 * @ingroup math
 */
typedef struct Point3Rawf_s {
    union {
        struct {
            double x;
            double y;
            double z;
        };
        double xy[2];
        double xyz[3];
    };
} Point3Rawf;

struct point3f_s; // The Point3f instance (opaque).

/**
 * Point3f instance. Class for manipulating 3D plane points using floating-point precision.
 * Use Point3f_New() or one of the other constructors to instantiate.
 */
typedef struct point3f_s Point3f;

Point3f* Point3f_New(void);
Point3f* Point3f_NewWithCoords(double x, double y, double z);
Point3f* Point3f_NewFromRaw(const Point3Rawf* rawPoint);
void Point3f_Delete(Point3f* point);

Point3Rawf* Point3f_Raw(const Point3f* point, Point3Rawf* rawPoint);

boolean Point3f_IsNull(const Point3f* point);

double Point3f_X(const Point3f* point);
double Point3f_Y(const Point3f* point);
double Point3f_Z(const Point3f* point);

void Point3f_SetX(Point3f* point, double x);
void Point3f_SetY(Point3f* point, double y);
void Point3f_SetZ(Point3f* point, double z);

const double* Point3f_XYZ(const Point3f* point);
void Point3f_SetXYZ(Point3f* point, double x, double y, double z);

void Point3f_Translate(Point3f* point, double x, double y, double z);
void Point3f_TranslateX(Point3f* point, double x);
void Point3f_TranslateY(Point3f* point, double y);
void Point3f_TranslateZ(Point3f* point, double z);

void Point3f_Sum(Point3f* point, const Point3f* other);

boolean Point3f_Equality(const Point3f* point, const Point3f* other);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* LIBDENG_DATA_POINT_H */
