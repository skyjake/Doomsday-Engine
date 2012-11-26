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

#include "libdeng.h"

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
#ifdef __cplusplus
    Point2Raw_s(int x_ = 0, int y_ = 0) : x(x_), y(y_) {}
#endif
} Point2Raw;

struct point2_s; // The Point2f instance (opaque).

/**
 * Point2 instance. Class for manipulating 2D plane points using integer precision.
 * Use Point2_New() or one of the other constructors to instantiate.
 */
typedef struct point2_s Point2;

DENG_PUBLIC Point2 *Point2_New(void);
DENG_PUBLIC Point2 *Point2_NewWithCoords(int x, int y);
DENG_PUBLIC Point2 *Point2_NewFromRaw(Point2Raw const *rawPoint);
DENG_PUBLIC void Point2_Delete(Point2 *point);

DENG_PUBLIC Point2Raw *Point2_Raw(Point2 const *point, Point2Raw *rawPoint);

DENG_PUBLIC boolean Point2_IsNull(Point2 const *point);

DENG_PUBLIC int Point2_X(Point2 const *point);
DENG_PUBLIC int Point2_Y(Point2 const *point);

DENG_PUBLIC void Point2_SetX(Point2 *point, int x);
DENG_PUBLIC void Point2_SetY(Point2 *point, int y);

DENG_PUBLIC int const *Point2_XY(Point2 const *point);
DENG_PUBLIC void Point2_SetXY(Point2 *point, int x, int y);

DENG_PUBLIC void Point2_Translate(Point2 *point, Point2Raw const *delta);
DENG_PUBLIC void Point2_TranslateXY(Point2 *point, int x, int y);
DENG_PUBLIC void Point2_TranslateX(Point2 *point, int x);
DENG_PUBLIC void Point2_TranslateY(Point2 *point, int y);

DENG_PUBLIC void Point2_Sum(Point2 *point, Point2 const *other);

DENG_PUBLIC boolean Point2_Equality(Point2 const *point, Point2 const *other);

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
#ifdef __cplusplus
    Point2Rawf_s(double x_ = 0, double y_ = 0) : x(x_), y(y_) {}
#endif
} Point2Rawf;

struct point2f_s; // The Point2f instance (opaque).

/**
 * Point2f instance. Class for manipulating 2D plane points using floating-point precision.
 * Use Point2f_New() or one of the other constructors to instantiate.
 */
typedef struct point2f_s Point2f;

DENG_PUBLIC Point2f *Point2f_New(void);
DENG_PUBLIC Point2f *Point2f_NewWithCoords(double x, double y);
DENG_PUBLIC Point2f *Point2f_NewFromRaw(Point2Rawf const *rawPoint);
DENG_PUBLIC void Point2f_Delete(Point2f *point);

DENG_PUBLIC Point2Rawf *Point2f_Raw(Point2f const *point, Point2Rawf *rawPoint);

DENG_PUBLIC boolean Point2f_IsNull(Point2f const *point);

DENG_PUBLIC double Point2f_X(Point2f const *point);
DENG_PUBLIC double Point2f_Y(Point2f const *point);

DENG_PUBLIC void Point2f_SetX(Point2f *point, double x);
DENG_PUBLIC void Point2f_SetY(Point2f *point, double y);

DENG_PUBLIC double const *Point2f_XY(Point2f const *point);
DENG_PUBLIC void Point2f_SetXY(Point2f *point, double x, double y);

DENG_PUBLIC void Point2f_Translate(Point2f *point, Point2Rawf const *delta);
DENG_PUBLIC void Point2f_TranslateXY(Point2f *point, double x, double y);
DENG_PUBLIC void Point2f_TranslateX(Point2f *point, double x);
DENG_PUBLIC void Point2f_TranslateY(Point2f *point, double y);

DENG_PUBLIC void Point2f_Sum(Point2f *point, Point2f const *other);

DENG_PUBLIC boolean Point2f_Equality(Point2f const *point, Point2f const *other);

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

DENG_PUBLIC Point3 *Point3_New(void);
DENG_PUBLIC Point3 *Point3_NewWithCoords(int x, int y, int z);
DENG_PUBLIC Point3 *Point3_NewFromRaw(Point3Raw const *rawPoint);
DENG_PUBLIC void Point3_Delete(Point3 *point);

DENG_PUBLIC Point3Raw *Point3_Raw(Point3 const *point, Point3Raw *rawPoint);

DENG_PUBLIC boolean Point3_IsNull(Point3 const *point);

DENG_PUBLIC int Point3_X(Point3 const *point);
DENG_PUBLIC int Point3_Y(Point3 const *point);
DENG_PUBLIC int Point3_Z(Point3 const *point);

DENG_PUBLIC void Point3_SetX(Point3 *point, int x);
DENG_PUBLIC void Point3_SetY(Point3 *point, int y);
DENG_PUBLIC void Point3_SetZ(Point3 *point, int z);

DENG_PUBLIC int const *Point3_XYZ(Point3 const *point);
DENG_PUBLIC void Point3_SetXYZ(Point3 *point, int x, int y, int z);

DENG_PUBLIC void Point3_Translate(Point3 *point, int x, int y, int z);
DENG_PUBLIC void Point3_TranslateX(Point3 *point, int x);
DENG_PUBLIC void Point3_TranslateY(Point3 *point, int y);
DENG_PUBLIC void Point3_TranslateZ(Point3 *point, int z);

DENG_PUBLIC void Point3_Sum(Point3 *point, Point3 const *other);

DENG_PUBLIC boolean Point3_Equality(Point3 const *point, Point3 const *other);

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

DENG_PUBLIC Point3f *Point3f_New(void);
DENG_PUBLIC Point3f *Point3f_NewWithCoords(double x, double y, double z);
DENG_PUBLIC Point3f *Point3f_NewFromRaw(Point3Rawf const *rawPoint);
DENG_PUBLIC void Point3f_Delete(Point3f *point);

DENG_PUBLIC Point3Rawf *Point3f_Raw(Point3f const *point, Point3Rawf *rawPoint);

DENG_PUBLIC boolean Point3f_IsNull(Point3f const *point);

DENG_PUBLIC double Point3f_X(Point3f const *point);
DENG_PUBLIC double Point3f_Y(Point3f const *point);
DENG_PUBLIC double Point3f_Z(Point3f const *point);

DENG_PUBLIC void Point3f_SetX(Point3f *point, double x);
DENG_PUBLIC void Point3f_SetY(Point3f *point, double y);
DENG_PUBLIC void Point3f_SetZ(Point3f *point, double z);

DENG_PUBLIC double const *Point3f_XYZ(Point3f const *point);
DENG_PUBLIC void Point3f_SetXYZ(Point3f *point, double x, double y, double z);

DENG_PUBLIC void Point3f_Translate(Point3f *point, double x, double y, double z);
DENG_PUBLIC void Point3f_TranslateX(Point3f *point, double x);
DENG_PUBLIC void Point3f_TranslateY(Point3f *point, double y);
DENG_PUBLIC void Point3f_TranslateZ(Point3f *point, double z);

DENG_PUBLIC void Point3f_Sum(Point3f *point, Point3f const *other);

DENG_PUBLIC boolean Point3f_Equality(Point3f const *point, Point3f const *other);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* LIBDENG_DATA_POINT_H */
