/** @file de/mathutil.h Math utilities.
 * @ingroup math
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
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
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#ifndef LIBDENG_MATHUTIL_H
#define LIBDENG_MATHUTIL_H

#include "libdeng.h"

typedef struct divline_s {
    fixed_t origin[2];
    fixed_t direction[2];
} Divline;

typedef Divline divline_t;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Calculate CRC-32 for an arbitrary data buffer. @ingroup math
 */
DENG_PUBLIC uint M_CRC32(byte *data, uint length);

/**
 * Gives an estimation of distance (not exact).
 */
DENG_PUBLIC double M_ApproxDistance(double dx, double dy);

DENG_PUBLIC float M_ApproxDistancef(float dx, float dy);

/**
 * Gives an estimation of 3D distance (not exact).
 */
DENG_PUBLIC double M_ApproxDistance3(double dx, double dy, double dz);

DENG_PUBLIC float M_ApproxDistance3f(float dx, float dy, float dz);

/**
 * To get a global angle from Cartesian coordinates, the coordinates are
 * flipped until they are in the first octant of the coordinate system, then
 * the y (<=x) is scaled and divided by x to get a tangent (slope) value
 * which is looked up in the tantoangle[] table.  The +1 size is to handle
 * the case when x==y without additional checking.
 *
 * @param x   X coordinate to test.
 * @param y   Y coordinate to test.
 *
 * @return  Angle between the test point and [0, 0].
 */
DENG_PUBLIC angle_t M_PointXYToAngle(double x, double y);

DENG_PUBLIC angle_t M_PointToAngle(double const point[2]);

/**
 * Translate a direction into an angle value (degrees).
 */
DENG_PUBLIC double M_DirectionToAngle(double const direction[2]);

DENG_PUBLIC double M_DirectionToAngleXY(double directionX, double directionY);

DENG_PUBLIC angle_t M_PointToAngle2(double const a[], double const b[]);

DENG_PUBLIC angle_t M_PointXYToAngle2(double aX, double aY, double bX, double bY);

DENG_PUBLIC double M_PointDistance(double const a[2], double const b[2]);

DENG_PUBLIC double M_PointXYDistance(double aX, double aY, double bX, double bY);

/**
 * Area of a triangle.
 */
DENG_PUBLIC double M_TriangleArea(double const v1[2], double const v2[2], double const v3[2]);

DENG_PUBLIC void M_RotateVector(float vec[3], float degYaw, float degPitch);

/**
 * Finds the power of 2 that is equal to or greater than the specified
 * number.
 */
DENG_PUBLIC int M_CeilPow2(int num);

/**
 * Finds the power of 2 that is less than or equal to the specified number.
 */
DENG_PUBLIC int M_FloorPow2(int num);

/**
 * Finds the power of 2 that is nearest the specified number. In ambiguous
 * cases, the smaller number is returned.
 */
DENG_PUBLIC int M_RoundPow2(int num);

/**
 * Weighted rounding. Weight determines the point where the number is still
 * rounded down (0..1).
 */
DENG_PUBLIC int M_WeightPow2(int num, float weight);

/**
 * @return Value mod length (length > 0).
 */
DENG_PUBLIC float M_CycleIntoRange(float value, float length);

/**
 * Using Euclid's Algorithm reduce the given numerator and denominator by
 * their greatest common integer divisor. @ingroup math
 * @param numerator  Input and output numerator.
 * @param denominator  Input and output denominator.
 * @return  Greatest common divisor.
 */
DENG_PUBLIC int M_RatioReduce(int* numerator, int* denominator);

DENG_PUBLIC slopetype_t M_SlopeType(double const direction[2]);

DENG_PUBLIC slopetype_t M_SlopeTypeXY(double directionX, double directionY);

DENG_PUBLIC slopetype_t M_SlopeTypeXY_FixedPrecision(fixed_t dx, fixed_t dy);

DENG_PUBLIC int M_NumDigits(int num);

/// @param angle  Normalised angle in the range [0..360].
DENG_PUBLIC double M_InverseAngle(double angle);

/**
 * @return @c 0 if point is in front of the line, else @c 1.
 */
DENG_PUBLIC int Divline_PointOnSide(Divline const *line, coord_t const point[2]);

DENG_PUBLIC int Divline_PointXYOnSide(Divline const *line, coord_t x, coord_t y);

/**
 * @return Fractional intercept point along the first divline.
 */
DENG_PUBLIC fixed_t Divline_Intersection(Divline const *v1, Divline const *v2);

// Bounding boxes:
DENG_PUBLIC void M_ClearBox(fixed_t* box);
DENG_PUBLIC void M_CopyBox(fixed_t dest[4], const fixed_t src[4]);
DENG_PUBLIC void M_AddToBox(fixed_t* box, fixed_t x, fixed_t y);
DENG_PUBLIC float M_BoundingBoxDiff(const float in[4], const float out[4]);
DENG_PUBLIC void M_JoinBoxes(float box[4], const float other[4]);

// Random numbers:
DENG_PUBLIC byte RNG_RandByte(void);
DENG_PUBLIC float RNG_RandFloat(void);
DENG_PUBLIC void RNG_Reset(void);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // LIBDENG_MATHUTIL_H
