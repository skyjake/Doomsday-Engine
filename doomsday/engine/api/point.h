/**\file uri.h
 *\section License
 * License: GPL
 * Online License Link: http://www.trolltech.com/gpl/
 *
 *\author Daniel Swanson <danij@dengine.net>
 *\author Copyright (C) 1992-2000 Trolltech AS.  All rights reserved.
 *
 * This class is based on the semantics defined for the QUrl class, a component
 * of the Qt GUI Toolkit.
 *
 * \todo Derive from Qt::QUrl
 */

#ifndef LIBDENG_DATA_POINT_H
#define LIBDENG_DATA_POINT_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Point2Raw. Point 2 Raw (integer). Is intended as a handy POD
 * structure for easy manipulation of two dimensional plane points,
 * with integer precison.
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

/**
 * Point2. Class for manipulating 2d plane points using integer precision.
 */
struct point2_s; // The Point2f instance (opaque).
typedef struct point2_s Point2;

Point2* Point2_New(void);
Point2* Point2_NewWithCoords(int x, int y);
Point2* Point2_NewFromRaw(const Point2Raw* rawPoint);
void Point2_Delete(Point2* point);

boolean Point2_IsNull(const Point2* point);

const Point2Raw* Point2_ToRaw(const Point2* point);

int Point2_X(const Point2* point);
int Point2_Y(const Point2* point);

void Point2_SetX(Point2* point, int x);
void Point2_SetY(Point2* point, int y);

const int* Point2_XY(const Point2* point);
void Point2_SetXY(Point2* point, int x, int y);

void Point2_Translate(Point2* point, int x, int y);

void Point2_Sum(Point2* point, const Point2* other);

boolean Point2_Equality(const Point2* point, const Point2* other);

/**
 * Point2Rawf. Point 2 Raw (f)loating point. Is intended as a handy POD
 * structure for easy manipulation of two dimensional plane points,
 * with floating point precison.
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

/**
 * Point2f. Class for manipulating 2d plane points using floating point precision.
 */
struct point2f_s; // The Point2f instance (opaque).
typedef struct point2f_s Point2f;

Point2f* Point2f_New(void);
Point2f* Point2f_NewWithCoords(double x, double y);
Point2f* Point2f_NewFromRaw(const Point2Rawf* rawPoint);
void Point2f_Delete(Point2f* point);

boolean Point2f_IsNull(const Point2f* point);

const Point2Rawf* Point2f_ToRaw(const Point2f* point);

double Point2f_X(const Point2f* point);
double Point2f_Y(const Point2f* point);

void Point2f_SetX(Point2f* point, double x);
void Point2f_SetY(Point2f* point, double y);

const double* Point2f_XY(const Point2f* point);
void Point2f_SetXY(Point2f* point, double x, double y);

void Point2f_Translate(Point2* point, double x, double y);

void Point2f_Sum(Point2f* point, const Point2f* other);

boolean Point2f_Equality(const Point2f* point, const Point2f* other);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* LIBDENG_DATA_POINT_H */
