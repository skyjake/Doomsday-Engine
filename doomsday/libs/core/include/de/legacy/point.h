/**
 * @file point.h
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

#ifndef LIBLEGACY_POINT_H
#define LIBLEGACY_POINT_H

#include "../liblegacy.h"

#ifdef __cplusplus
extern "C" {
#endif

/// @addtogroup legacyMath
/// @{

/**
 * 2D point with integer values. A handy POD structure for easy manipulation of
 * two dimensional plane points.
 */
typedef struct Point2Raw_s {
    union {
        struct {
            int x;
            int y;
        };
        int xy[2];
    };
//#ifdef __cplusplus
//    Point2Raw_s(int x_ = 0, int y_ = 0) : x(x_), y(y_) {}
//    Point2Raw_s(const Point2Raw_s &other) : x(other.x), y(other.y) {}
//#endif
} Point2Raw;

struct point2_s; // The Point2f instance (opaque).

/**
 * Point2 instance. Class for manipulating 2D plane points using integer precision.
 * Use Point2_New() or one of the other constructors to instantiate.
 */
typedef struct point2_s Point2;

DE_PUBLIC Point2 *Point2_New(void);
DE_PUBLIC Point2 *Point2_NewWithCoords(int x, int y);
DE_PUBLIC Point2 *Point2_NewFromRaw(const Point2Raw *rawPoint);
DE_PUBLIC void Point2_Delete(Point2 *point);

DE_PUBLIC Point2Raw *Point2_Raw(const Point2 *point, Point2Raw *rawPoint);

DE_PUBLIC dd_bool Point2_IsNull(const Point2 *point);

DE_PUBLIC int Point2_X(const Point2 *point);
DE_PUBLIC int Point2_Y(const Point2 *point);

DE_PUBLIC void Point2_SetX(Point2 *point, int x);
DE_PUBLIC void Point2_SetY(Point2 *point, int y);

DE_PUBLIC const int *Point2_XY(const Point2 *point);
DE_PUBLIC void Point2_SetXY(Point2 *point, int x, int y);

DE_PUBLIC void Point2_Translate(Point2 *point, const Point2Raw *delta);
DE_PUBLIC void Point2_TranslateXY(Point2 *point, int x, int y);
DE_PUBLIC void Point2_TranslateX(Point2 *point, int x);
DE_PUBLIC void Point2_TranslateY(Point2 *point, int y);

DE_PUBLIC void Point2_Sum(Point2 *point, const Point2 *other);

DE_PUBLIC dd_bool Point2_Equality(const Point2 *point, const Point2 *other);

/**
 * 2D point with floating point values. A handy POD structure for easy
 * manipulation of two dimensional plane points.
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

DE_PUBLIC Point2f *Point2f_New(void);
DE_PUBLIC Point2f *Point2f_NewWithCoords(double x, double y);
DE_PUBLIC Point2f *Point2f_NewFromRaw(const Point2Rawf *rawPoint);
DE_PUBLIC void Point2f_Delete(Point2f *point);

DE_PUBLIC Point2Rawf *Point2f_Raw(const Point2f *point, Point2Rawf *rawPoint);

DE_PUBLIC dd_bool Point2f_IsNull(const Point2f *point);

DE_PUBLIC double Point2f_X(const Point2f *point);
DE_PUBLIC double Point2f_Y(const Point2f *point);

DE_PUBLIC void Point2f_SetX(Point2f *point, double x);
DE_PUBLIC void Point2f_SetY(Point2f *point, double y);

DE_PUBLIC const double *Point2f_XY(const Point2f *point);
DE_PUBLIC void Point2f_SetXY(Point2f *point, double x, double y);

DE_PUBLIC void Point2f_Translate(Point2f *point, const Point2Rawf *delta);
DE_PUBLIC void Point2f_TranslateXY(Point2f *point, double x, double y);
DE_PUBLIC void Point2f_TranslateX(Point2f *point, double x);
DE_PUBLIC void Point2f_TranslateY(Point2f *point, double y);

DE_PUBLIC void Point2f_Sum(Point2f *point, const Point2f *other);

DE_PUBLIC dd_bool Point2f_Equality(const Point2f *point, const Point2f *other);

/**
 * 3D point with integer values. A handy POD structure for easy manipulation of
 * three dimensional space points.
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

DE_PUBLIC Point3 *Point3_New(void);
DE_PUBLIC Point3 *Point3_NewWithCoords(int x, int y, int z);
DE_PUBLIC Point3 *Point3_NewFromRaw(const Point3Raw *rawPoint);
DE_PUBLIC void Point3_Delete(Point3 *point);

DE_PUBLIC Point3Raw *Point3_Raw(const Point3 *point, Point3Raw *rawPoint);

DE_PUBLIC dd_bool Point3_IsNull(const Point3 *point);

DE_PUBLIC int Point3_X(const Point3 *point);
DE_PUBLIC int Point3_Y(const Point3 *point);
DE_PUBLIC int Point3_Z(const Point3 *point);

DE_PUBLIC void Point3_SetX(Point3 *point, int x);
DE_PUBLIC void Point3_SetY(Point3 *point, int y);
DE_PUBLIC void Point3_SetZ(Point3 *point, int z);

DE_PUBLIC const int *Point3_XYZ(const Point3 *point);
DE_PUBLIC void Point3_SetXYZ(Point3 *point, int x, int y, int z);

DE_PUBLIC void Point3_Translate(Point3 *point, int x, int y, int z);
DE_PUBLIC void Point3_TranslateX(Point3 *point, int x);
DE_PUBLIC void Point3_TranslateY(Point3 *point, int y);
DE_PUBLIC void Point3_TranslateZ(Point3 *point, int z);

DE_PUBLIC void Point3_Sum(Point3 *point, const Point3 *other);

DE_PUBLIC dd_bool Point3_Equality(const Point3 *point, const Point3 *other);

/**
 * 3D point with floating point values. A handy POD structure for easy
 * manipulation of three dimensional space points.
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

DE_PUBLIC Point3f *Point3f_New(void);
DE_PUBLIC Point3f *Point3f_NewWithCoords(double x, double y, double z);
DE_PUBLIC Point3f *Point3f_NewFromRaw(const Point3Rawf *rawPoint);
DE_PUBLIC void Point3f_Delete(Point3f *point);

DE_PUBLIC Point3Rawf *Point3f_Raw(const Point3f *point, Point3Rawf *rawPoint);

DE_PUBLIC dd_bool Point3f_IsNull(const Point3f *point);

DE_PUBLIC double Point3f_X(const Point3f *point);
DE_PUBLIC double Point3f_Y(const Point3f *point);
DE_PUBLIC double Point3f_Z(const Point3f *point);

DE_PUBLIC void Point3f_SetX(Point3f *point, double x);
DE_PUBLIC void Point3f_SetY(Point3f *point, double y);
DE_PUBLIC void Point3f_SetZ(Point3f *point, double z);

DE_PUBLIC const double *Point3f_XYZ(const Point3f *point);
DE_PUBLIC void Point3f_SetXYZ(Point3f *point, double x, double y, double z);

DE_PUBLIC void Point3f_Translate(Point3f *point, double x, double y, double z);
DE_PUBLIC void Point3f_TranslateX(Point3f *point, double x);
DE_PUBLIC void Point3f_TranslateY(Point3f *point, double y);
DE_PUBLIC void Point3f_TranslateZ(Point3f *point, double z);

DE_PUBLIC void Point3f_Sum(Point3f *point, const Point3f *other);

DE_PUBLIC dd_bool Point3f_Equality(const Point3f *point, const Point3f *other);

/// @}

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* LIBLEGACY_POINT_H */
