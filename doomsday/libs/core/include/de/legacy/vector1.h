/** @file vector1.h Vector math (2D, 3D, 4D).
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#ifndef DE_VECTOR_H
#define DE_VECTOR_H

#include "types.h"
#include "fixedpoint.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup vec2 2D Vectors
 * @ingroup legacyMath
 * @{
 */

// Floating-point.
typedef float vectorcompf_t;
typedef vectorcompf_t vec2f_t[2];
typedef const float const_pvec2f_t[2];
typedef vectorcompf_t *pvec2f_t;

typedef vec2f_t *arvec2f_t;
typedef const vec2f_t *const_arvec2f_t;

// Double floating-point.
typedef double vectorcompd_t;
typedef vectorcompd_t vec2d_t[2];
typedef const double const_pvec2d_t[2];
typedef vectorcompd_t *pvec2d_t;

typedef vec2d_t *arvec2d_t;
typedef const vec2d_t *const_arvec2d_t;

typedef const vectorcompd_t *pcvec2d_t; /// @todo Remove me

DE_PUBLIC void V2x_Set(fixed_t vec[2], fixed_t x, fixed_t y);

DE_PUBLIC fixed_t V2x_Intersection(fixed_t const v1[2], fixed_t const v1Delta[2], fixed_t const v2[2], fixed_t const v2Delta[2]);

/// @return @c 0 or 1.
DE_PUBLIC int V2x_PointOnLineSide(fixed_t const point[2], fixed_t const lineOrigin[2], fixed_t const lineDirection[2]);

/**
 * Set the vector's x and y components.
 */
DE_PUBLIC void V2f_Set(pvec2f_t vec, vectorcompf_t x, vectorcompf_t y);

DE_PUBLIC void V2f_SetFixed(pvec2f_t vec, fixed_t x, fixed_t y);

/**
 * 2-dimensional vector length.
 */
DE_PUBLIC float V2f_Length(const_pvec2f_t vec);

/**
 * The distance between two points.
 */
DE_PUBLIC float V2f_Distance(const_pvec2f_t a, const_pvec2f_t b);

/**
 * Normalize a 2-dimensional vector.
 *
 * @return  The length of the vector.
 */
DE_PUBLIC float V2f_Normalize(pvec2f_t vec);

/**
 * Make a copy of the source vector.
 */
DE_PUBLIC void V2f_Copy(pvec2f_t dest, const_pvec2f_t src);
DE_PUBLIC void V2f_Copyd(pvec2f_t dest, const_pvec2d_t src);

/**
 * Multiply the vector by the scalar.
 */
DE_PUBLIC void V2f_Scale(pvec2f_t vector, float scalar);

/**
 * Rotate the vector by a radian angle.
 */
DE_PUBLIC void V2f_Rotate(pvec2f_t vec, float radians);

/**
 * Calculate the sum of two 2-dimensional vectors.
 */
DE_PUBLIC void V2f_Sum(pvec2f_t dest, const_pvec2f_t src1, const_pvec2f_t src2);

/**
 * Subtract src1 from src2, return result in 'dest'.
 */
DE_PUBLIC void V2f_Subtract(pvec2f_t dest, const_pvec2f_t src1, const_pvec2f_t src2);

/**
 * Calculate the dot product of the two vectors.
 */
DE_PUBLIC float V2f_DotProduct(const_pvec2f_t a, const_pvec2f_t b);

/**
 * Calculate the scalar projection of 'a' onto 'b': dot(a,b)/len(b)
 */
DE_PUBLIC float V2f_ScalarProject(const_pvec2f_t a, const_pvec2f_t b);

/**
 * Project 'a' onto 'b' and store the resulting vector to 'dest':
 * dot(a,b)/dot(b,b)*b
 */
DE_PUBLIC float V2f_Project(pvec2f_t dest, const_pvec2f_t a, const_pvec2f_t b);

/**
 * @return  @c true, if the two vectors are parallel.
 */
DE_PUBLIC dd_bool V2f_IsParallel(const_pvec2f_t a, const_pvec2f_t b);

/**
 * @return  @c true, if the vector is a zero vector.
 */
DE_PUBLIC dd_bool V2f_IsZero(const_pvec2f_t vec);

/**
 * The line must be exactly one unit long!
 */
DE_PUBLIC float V2f_PointUnitLineDistance(const_pvec2f_t point, const_pvec2f_t linePoint, const_pvec2f_t lineDirection);

/**
 * Determine where the two lines cross each other.  Notice that the
 * lines are defined with a point and a vector.
 *
 * @return  A scaling factor for the first line.
 */
DE_PUBLIC float V2f_Intersection(const_pvec2f_t p1, const_pvec2f_t delta1, const_pvec2f_t p2, const_pvec2f_t delta2, pvec2f_t point);

/**
 * Intersection of lines a->b and c->d.  Unlike V2f_Intersection(), the
 * arguments are all points.
 */
DE_PUBLIC float V2f_Intercept(const_pvec2f_t a, const_pvec2f_t b, const_pvec2f_t c, const_pvec2f_t d, pvec2f_t point);

/**
 * @return  @c true, if the two lines intercept.
 */
DE_PUBLIC dd_bool V2f_Intercept2(const_pvec2f_t a, const_pvec2f_t b, const_pvec2f_t c, const_pvec2f_t d, pvec2f_t point, float *abFrac, float *cdFrac);

/**
 * Linear interpolation between a and b, by c.
 */
DE_PUBLIC void V2f_Lerp(pvec2f_t dest, const_pvec2f_t a, const_pvec2f_t b, float c);

/**
 * Left/top is the min-point.  Right/bottom is the max-point.
 */
DE_PUBLIC void V2f_InitBox(arvec2f_t box, const_pvec2f_t point);

DE_PUBLIC void V2f_AddToBox(arvec2f_t box, const_pvec2f_t point);

DE_PUBLIC void V2f_UniteBox(arvec2f_t box, arvec2f_t const other);

DE_PUBLIC void V2f_CopyBox(arvec2f_t dest, arvec2f_t const src);
DE_PUBLIC void V2f_CopyBoxd(arvec2f_t dest, arvec2d_t const src);

/**
 * Set the vector's x and y components.
 */
DE_PUBLIC void V2d_Set(pvec2d_t vec, vectorcompd_t x, vectorcompd_t y);

DE_PUBLIC void V2d_SetFixed(pvec2d_t vec, fixed_t x, fixed_t y);

/**
 * 2-dimensional vector length.
 */
DE_PUBLIC double V2d_Length(const_pvec2d_t vector);

/**
 * The distance between two points.
 */
DE_PUBLIC double V2d_Distance(const_pvec2d_t a, const_pvec2d_t b);

/**
 * Normalize a 2-dimensional vector.
 *
 * @return  The length of the vector.
 */
DE_PUBLIC double V2d_Normalize(pvec2d_t vec);

/**
 * Make a copy of the source vector.
 */
DE_PUBLIC void V2d_Copy(pvec2d_t dest, const_pvec2d_t src);
DE_PUBLIC void V2d_Copyf(pvec2d_t dest, const_pvec2f_t srcf);

/**
 * Multiply the vector by the scalar.
 */
DE_PUBLIC void V2d_Scale(pvec2d_t vector, double scalar);

/**
 * Rotate the vector by a radian angle.
 */
DE_PUBLIC void V2d_Rotate(pvec2d_t vec, double radians);

/**
 * Calculate the sum of two 2-dimensional vectors.
 */
DE_PUBLIC void V2d_Sum(pvec2d_t dest, const_pvec2d_t src1, const_pvec2d_t src2);

/**
 * Subtract src1 from src2, return result in 'dest'.
 */
DE_PUBLIC void V2d_Subtract(pvec2d_t dest, const_pvec2d_t src1, const_pvec2d_t src2);

/**
 * Distance from the line to a point.
 */
DE_PUBLIC double V2d_PointLineDistance(const_pvec2d_t point, const_pvec2d_t linePoint,
    const_pvec2d_t lineDirection, double *offset);

/**
 * Compute the parallel distance from the line to a point.
 */
DE_PUBLIC double V2d_PointLineParaDistance(const_pvec2d_t point, const_pvec2d_t lineDirection,
    double linePara, double lineLength);

/**
 * Compute the perpendicular distance from the line to a point.
 */
DE_PUBLIC double V2d_PointLinePerpDistance(const_pvec2d_t point, const_pvec2d_t lineDirection,
    double linePerp, double lineLength);

/**
 * Determines on which side of line the point is.
 *
 * @param point  The point.
 * @param lineOrigin  Point on the line (origin).
 * @param lineDirection  Line angle delta (from origin -> out).
 *
 * @return @c <0 Point is to the left of the line.
 *         @c =0 Point lies directly on the line.
 *         @c >0 Point is to the right of the line.
 */
DE_PUBLIC double V2d_PointOnLineSide(const_pvec2d_t point, const_pvec2d_t lineOrigin,
    const_pvec2d_t lineDirection);

/**
 * Determines on which side of line the point is.
 *
 * @param point  The point.
 * @param lineDirection  Line angle delta (from origin -> out).
 * @param linePerp  Perpendicular d of the line.
 * @param lineLength  Length of the line.
 * @param epsilon  Distance within which @a point is considered on top of the line.
 *
 * @return @c <0 Point is to the left of the line.
 *         @c =0 Point lies directly on the line.
 *         @c >0 Point is to the right of the line.
 */
DE_PUBLIC double V2d_PointOnLineSide2(const_pvec2d_t point, const_pvec2d_t lineDirection,
    double linePerp, double lineLength, double epsilon);

/**
 * Calculate the dot product of the two vectors.
 */
DE_PUBLIC double V2d_DotProduct(const_pvec2d_t a, const_pvec2d_t b);

/**
 * Calculate the scalar projection of 'a' onto 'b': dot(a,b)/len(b)
 */
DE_PUBLIC double V2d_ScalarProject(const_pvec2d_t a, const_pvec2d_t b);

/**
 * Project 'a' onto 'b' and store the resulting vector to 'dest':
 * dot(a,b)/dot(b,b)*b
 */
DE_PUBLIC double V2d_Project(pvec2d_t dest, const_pvec2d_t a, const_pvec2d_t b);

DE_PUBLIC double V2d_ProjectOnLine(pvec2d_t dest, const_pvec2d_t point,
    const_pvec2d_t lineOrigin, const_pvec2d_t lineDirection);

/**
 * @return  @c true, if the two vectors are parallel.
 */
DE_PUBLIC dd_bool V2d_IsParallel(const_pvec2d_t a, const_pvec2d_t b);

/**
 * @return  @c true, if the vector is a zero vector.
 */
DE_PUBLIC dd_bool V2d_IsZero(const_pvec2d_t vec);

/**
 * Determine where the two lines cross each other.  Notice that the
 * lines are defined with a point and a vector.
 *
 * @return  A scaling factor for the first line.
 */
DE_PUBLIC double V2d_Intersection(const_pvec2d_t p1, const_pvec2d_t delta1, const_pvec2d_t p2, const_pvec2d_t delta2, pvec2d_t point);

/**
 * Intersection of lines a->b and c->d.  Unlike V2d_Intersection(), the
 * arguments are all points.
 */
DE_PUBLIC double V2d_Intercept(const_pvec2d_t a, const_pvec2d_t b, const_pvec2d_t c, const_pvec2d_t d, pvec2d_t point);

/**
 * @return  @c true, if the two lines intercept.
 */
DE_PUBLIC dd_bool V2d_Intercept2(const_pvec2d_t a, const_pvec2d_t b, const_pvec2d_t c, const_pvec2d_t d, pvec2d_t point, double *abFrac, double *cdFrac);

/**
 * Linear interpolation between a and b, by c.
 */
DE_PUBLIC void V2d_Lerp(pvec2d_t dest, const_pvec2d_t a, const_pvec2d_t b, double c);

/**
 * Left/top is the min-point.  Right/bottom is the max-point.
 */
DE_PUBLIC void V2d_InitBox(arvec2d_t box, const_pvec2d_t point);
DE_PUBLIC void V2d_InitBoxXY(arvec2d_t box, double x, double y);

DE_PUBLIC void V2d_AddToBox(arvec2d_t box, const_pvec2d_t point);
DE_PUBLIC void V2d_AddToBoxXY(arvec2d_t box, double x, double y);

DE_PUBLIC void V2d_UniteBox(arvec2d_t box, const_arvec2d_t other);

DE_PUBLIC void V2d_CopyBox(arvec2d_t dest, const_arvec2d_t src);

/// @}

/**
 * @defgroup vec3 3D Vectors
 * @ingroup legacyMath
 * @{
 */

// Floating-point.
typedef vectorcompf_t vec3f_t[3];
typedef float const const_pvec3f_t[3];
typedef vectorcompf_t* pvec3f_t;
typedef vec3f_t* arvec3f_t;

// Double floating-point.
typedef vectorcompd_t vec3d_t[3];
typedef double const const_pvec3d_t[3];
typedef vectorcompd_t* pvec3d_t;
typedef vec3d_t* arvec3d_t;

/**
 * Set the vector's x, y and z components.
 */
DE_PUBLIC void V3f_Set(pvec3f_t vec, vectorcompf_t x, vectorcompf_t y, vectorcompf_t z);

DE_PUBLIC void V3f_SetFixed(pvec3f_t vec, fixed_t x, fixed_t y, fixed_t z);

/**
 * 3-dimensional vector length.
 */
DE_PUBLIC float V3f_Length(const_pvec3f_t vec);

/**
 * The distance between two points.
 */
DE_PUBLIC float V3f_Distance(const_pvec3f_t a, const_pvec3f_t b);

/**
 * Normalize a 3-dimensional vector.
 *
 * @return  The length of the vector.
 */
DE_PUBLIC float V3f_Normalize(pvec3f_t vec);

/**
 * Make a copy of the source vector.
 */
DE_PUBLIC void V3f_Copy(pvec3f_t dest, const_pvec3f_t src);
DE_PUBLIC void V3f_Copyd(pvec3f_t dest, const_pvec3d_t src);

/**
 * Multiply the vector by the scalar.
 */
DE_PUBLIC void V3f_Scale(pvec3f_t vec, float scalar);

/**
 * Calculate the sum of two 3-dimensional vectors.
 */
DE_PUBLIC void V3f_Sum(pvec3f_t dest, const_pvec3f_t src1, const_pvec3f_t src2);

/**
 * Subtract src1 from src2, return result in 'dest'.
 */
DE_PUBLIC void V3f_Subtract(pvec3f_t dest, const_pvec3f_t src1, const_pvec3f_t src2);

/**
 * Calculate the dot product of the two vectors.
 */
DE_PUBLIC float V3f_DotProduct(const_pvec3f_t a, const_pvec3f_t b);

/**
 * Calculate the cross product of two vectors.
 *
 * @param dest  Result will be written back here.
 * @param a     First vector.
 * @param b     Second vector.
 */
DE_PUBLIC void V3f_CrossProduct(pvec3f_t dest, const_pvec3f_t a, const_pvec3f_t b);

DE_PUBLIC void V3f_CrossProductd(pvec3f_t dest, const_pvec3d_t ad, const_pvec3d_t bd);

/**
 * Cross product of two vectors composed of three points.
 *
 * @param dest  Result will be written back here.
 * @param v1  First vector.
 * @param v2  Second vector.
 * @param v3  Third vector.
 */
DE_PUBLIC void V3f_PointCrossProduct(pvec3f_t dest, const_pvec3f_t v1, const_pvec3f_t v2, const_pvec3f_t v3);

/**
 * Find the closest point in the plane, to an arbitary point.
 *
 * @param dest  Result will be written back here.
 * @param planeNormal  The normalized plane normal.
 * @param planePoint  A point already on the plane.
 * @param arbPoint  The arbitrary point to find the closest point too.
 *
 * @return  Distance from the closest point on the plane to the specified arbitary point.
 */
DE_PUBLIC float V3f_ClosestPointOnPlane(pvec3f_t dest, const_pvec3f_t planeNormal, const_pvec3f_t planePoint, const_pvec3f_t arbPoint);

/**
 * Determine which axis of the given vector is the major.
 */
DE_PUBLIC int V3f_MajorAxis(const_pvec3f_t vec);

/**
 * @return  @c true, if the vector is a zero vector.
 */
DE_PUBLIC dd_bool V3f_IsZero(const_pvec3f_t vec);

/**
 * Linear interpolation between a and b, by c.
 */
DE_PUBLIC void V3f_Lerp(pvec3f_t dest, const_pvec3f_t a, const_pvec3f_t b, vectorcompf_t c);

/**
 * Given a normalized normal, construct up and right vectors, oriented about
 * @a normal in our right-handed world coordinate space.
 *
 * @param tangent  The 'right' vector will be written back here.
 * @param bitangent  The 'up' vector will be written back here.
 * @param normal  Normal to construct tangents for.
 */
DE_PUBLIC void V3f_BuildTangents(pvec3f_t tangent, pvec3f_t bitangent, const_pvec3f_t normal);

/**
 * Set the vector's x, y and z components.
 */
DE_PUBLIC void V3d_Set(pvec3d_t vec, vectorcompd_t x, vectorcompd_t y, vectorcompd_t z);

DE_PUBLIC void V3d_SetFixed(pvec3d_t vec, fixed_t x, fixed_t y, fixed_t z);

/**
 * 3-dimensional vector length.
 */
DE_PUBLIC double V3d_Length(const_pvec3d_t vec);

/**
 * The distance between two points.
 */
DE_PUBLIC double V3d_Distance(const_pvec3d_t a, const_pvec3d_t b);

/**
 * Normalize a 3-dimensional vector.
 *
 * @return  The length of the vector.
 */
DE_PUBLIC double V3d_Normalize(pvec3d_t vec);

/**
 * Make a copy of the source vector.
 */
DE_PUBLIC void V3d_Copy(pvec3d_t dest, const_pvec3d_t src);
DE_PUBLIC void V3d_Copyf(pvec3d_t dest, const_pvec3f_t src);

/**
 * Multiply the vector by the scalar.
 */
DE_PUBLIC void V3d_Scale(pvec3d_t vec, vectorcompd_t scalar);

/**
 * Calculate the sum of two 3-dimensional vectors.
 */
DE_PUBLIC void V3d_Sum(pvec3d_t dest, const_pvec3d_t src1, const_pvec3d_t src2);

/**
 * Subtract src1 from src2, return result in 'dest'.
 */
DE_PUBLIC void V3d_Subtract(pvec3d_t dest, const_pvec3d_t src1, const_pvec3d_t src2);

/**
 * Calculate the dot product of the two vectors.
 */
DE_PUBLIC double V3d_DotProduct(const_pvec3d_t a, const_pvec3d_t b);
DE_PUBLIC double V3d_DotProductf(const_pvec3d_t a, const_pvec3f_t b);

/**
 * Calculate the cross product of two vectors.
 *
 * @param dest  Result will be written back here.
 * @param src1  First vector.
 * @param src2  Second vector.
 */
DE_PUBLIC void V3d_CrossProduct(pvec3d_t dest, const_pvec3d_t src1, const_pvec3d_t src2);

/**
 * Cross product of two vectors composed of three points.
 *
 * @param dest  Result will be written back here.
 * @param v1  First vector.
 * @param v2  Second vector.
 * @param v3  Third vector.
 */
DE_PUBLIC void V3d_PointCrossProduct(pvec3d_t dest, const_pvec3d_t v1, const_pvec3d_t v2, const_pvec3d_t v3);

/**
 * Find the closest point in the plane, to an arbitary point.
 *
 * @param dest  Result will be written back here.
 * @param planeNormal  The normalized plane normal.
 * @param planePoint  A point already on the plane.
 * @param arbPoint  The arbitrary point to find the closest point too.
 *
 * @return  Distance from the closest point on the plane to the specified arbitary point.
 */
DE_PUBLIC double V3d_ClosestPointOnPlane(pvec3d_t dest, const_pvec3d_t planeNormal, const_pvec3d_t planePoint, const_pvec3d_t arbPoint);
DE_PUBLIC double V3d_ClosestPointOnPlanef(pvec3d_t dest, const_pvec3f_t planeNormalf, const_pvec3d_t planePoint, const_pvec3d_t arbPoint);

/**
 * Determine which axis of the given vector is the major.
 */
int V3d_MajorAxis(const_pvec3d_t vec);

/**
 * @return  @c true, if the vector is a zero vector.
 */
DE_PUBLIC dd_bool V3d_IsZero(const_pvec3d_t vec);

/**
 * Linear interpolation between a and b, by c.
 */
DE_PUBLIC void V3d_Lerp(pvec3d_t dest, const_pvec3d_t a, const_pvec3d_t b, vectorcompd_t c);

/**
 * Given a normalized normal, construct up and right vectors, oriented about
 * @a normal in our right-handed world coordinate space.
 *
 * @param tangent  The 'right' vector will be written back here.
 * @param bitangent  The 'up' vector will be written back here.
 * @param normal  Normal to construct tangents for.
 */
DE_PUBLIC void V3d_BuildTangents(pvec3d_t tangent, pvec3d_t bitangent, const_pvec3d_t normal);

/// @}

/**
 * @defgroup vec4 4D Vectors
 * @ingroup legacyMath
 * @{
 */

typedef vectorcompf_t vec4f_t[4];
typedef const float *const_pvec4f_t;
typedef vectorcompf_t* pvec4f_t;
typedef vec4f_t* arvec4f_t;

/**
 * Set the vector's x, y, z and w components.
 */
DE_PUBLIC void V4f_Set(pvec4f_t vec, vectorcompf_t x, vectorcompf_t y, vectorcompf_t z, vectorcompf_t w);

DE_PUBLIC void V4f_SetFixed(pvec4f_t vec, fixed_t x, fixed_t y, fixed_t z, fixed_t w);

/**
 * 4-dimensional vector length.
 */
DE_PUBLIC float V4f_Length(const_pvec4f_t vec);

/**
 * The distance between two points.
 */
DE_PUBLIC float V4f_Distance(const_pvec4f_t a, const_pvec4f_t b);

/**
 * Normalize a 4-dimensional vector.
 *
 * @return  The length of the vector.
 */
DE_PUBLIC float V4f_Normalize(pvec4f_t vec);

/**
 * Make a copy of the source vector.
 */
DE_PUBLIC void V4f_Copy(pvec4f_t dest, const_pvec4f_t src);

/**
 * Multiply the vector by the scalar.
 */
DE_PUBLIC void V4f_Scale(pvec4f_t vec, float scalar);

/**
 * Calculate the sum of two 4-dimensional vectors.
 */
DE_PUBLIC void V4f_Sum(pvec4f_t dest, const_pvec4f_t src1, const_pvec4f_t src2);

/**
 * Subtract src1 from src2, return result in 'dest'.
 */
DE_PUBLIC void V4f_Subtract(pvec4f_t dest, const_pvec4f_t src1, const_pvec4f_t src2);

/**
 * @return  @c true, if the vector is a zero vector.
 */
DE_PUBLIC dd_bool V4f_IsZero(const_pvec4f_t vec);

/**
 * Linear interpolation between a and b, by c.
 */
DE_PUBLIC void V4f_Lerp(pvec4f_t dest, const_pvec4f_t a, const_pvec4f_t b, float c);

typedef vectorcompd_t vec4d_t[4];
typedef const double *const_pvec4d_t;
typedef vectorcompd_t *pvec4d_t;
typedef vec4d_t *arvec4d_t;

/**
 * Set the vector's x, y, z and w components.
 */
DE_PUBLIC void V4d_Set(pvec4d_t vec, vectorcompd_t x, vectorcompd_t y, vectorcompd_t z, vectorcompd_t w);

DE_PUBLIC void V4d_SetFixed(pvec4d_t vec, fixed_t x, fixed_t y, fixed_t z, fixed_t w);

/**
 * 4-dimensional vector length.
 */
DE_PUBLIC double V4d_Length(const_pvec4d_t vec);

/**
 * The distance between two points.
 */
DE_PUBLIC double V4d_Distance(const_pvec4d_t a, const_pvec4d_t b);

/**
 * Normalize a 4-dimensional vector.
 *
 * @return  The length of the vector.
 */
DE_PUBLIC double V4d_Normalize(pvec4d_t vec);

/**
 * Make a copy of the source vector.
 */
DE_PUBLIC void V4d_Copy(pvec4d_t dest, const_pvec4d_t src);

/**
 * Multiply the vector by the scalar.
 */
DE_PUBLIC void V4d_Scale(pvec4d_t vec, double scalar);

/**
 * Calculate the sum of two 4-dimensional vectors.
 */
DE_PUBLIC void V4d_Sum(pvec4d_t dest, const_pvec4d_t src1, const_pvec4d_t src2);

/**
 * Subtract src1 from src2, return result in 'dest'.
 */
DE_PUBLIC void V4d_Subtract(pvec4d_t dest, const_pvec4d_t src1, const_pvec4d_t src2);

/**
 * @return  @c true, if the vector is a zero vector.
 */
DE_PUBLIC dd_bool V4d_IsZero(const_pvec4d_t vec);

/**
 * Linear interpolation between a and b, by c.
 */
DE_PUBLIC void V4d_Lerp(pvec4d_t dest, const_pvec4d_t a, const_pvec4d_t b, double c);

/// @}

#ifdef __cplusplus
} // extern "C"
#endif

#endif // DE_VECTOR_H
