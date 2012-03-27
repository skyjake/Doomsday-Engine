/**
 * @file m_vector.h
 * Vector math. @ingroup data
 *
 * @authors Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2012 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG_DATA_VECTOR
#define LIBDENG_DATA_VECTOR

#include "dd_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * 2-dimensions:
 */

typedef float vectorcompf_t;
typedef vectorcompf_t vec2f_t[2];
typedef const float const_pvec2f_t[2];
typedef vectorcompf_t* pvec2f_t;
typedef vec2f_t* arvec2f_t;

/**
 * Set the vector's x and y components.
 */
void V2f_Set(pvec2f_t vec, vectorcompf_t x, vectorcompf_t y);

void V2f_SetFixed(pvec2f_t vec, fixed_t x, fixed_t y);

/**
 * 2-dimensional vector length.
 */
float V2f_Length(const pvec2f_t vector);

/**
 * The distance between two points.
 */
float V2f_Distance(const pvec2f_t a, const pvec2f_t b);

/**
 * Normalize a 2-dimensional vector.
 *
 * @return  The length of the vector.
 */
float V2f_Normalize(pvec2f_t vec);

/**
 * Make a copy of the source vector.
 */
void V2f_Copy(pvec2f_t dest, const_pvec2f_t src);

/**
 * Multiply the vector by the scalar.
 */
void V2f_Scale(pvec2f_t vector, float scalar);

/**
 * Rotate the vector by a radian angle.
 */
void V2f_Rotate(pvec2f_t vec, float radians);

/**
 * Calculate the sum of two 2-dimensional vectors.
 */
void V2f_Sum(pvec2f_t dest, const_pvec2f_t src1, const_pvec2f_t src2);

/**
 * Subtract src1 from src2, return result in 'dest'.
 */
void V2f_Subtract(pvec2f_t dest, const_pvec2f_t src1, const_pvec2f_t src2);

/**
 * Calculate the dot product of the two vectors.
 */
float V2f_DotProduct(const pvec2f_t a, const pvec2f_t b);

/**
 * Calculate the scalar projection of 'a' onto 'b': dot(a,b)/len(b)
 */
float V2f_ScalarProject(const pvec2f_t a, const pvec2f_t b);

/**
 * Project 'a' onto 'b' and store the resulting vector to 'dest':
 * dot(a,b)/dot(b,b)*b
 */
void V2f_Project(pvec2f_t dest, const pvec2f_t a, const pvec2f_t b);

/**
 * @return  @c true, if the two vectors are parallel.
 */
boolean V2f_IsParallel(const pvec2f_t a, const pvec2f_t b);

/**
 * @return  @c true, if the vector is a zero vector.
 */
boolean V2f_IsZero(const pvec2f_t vec);

/**
 * Determine where the two lines cross each other.  Notice that the
 * lines are defined with a point and a vector.
 *
 * @return  A scaling factor for the first line.
 */
float V2f_Intersection(const_pvec2f_t p1, const_pvec2f_t delta1, const_pvec2f_t p2, const_pvec2f_t delta2, pvec2f_t point);

/**
 * Intersection of lines a->b and c->d.  Unlike V2f_Intersection(), the
 * arguments are all points.
 */
float V2f_Intercept(const pvec2f_t a, const pvec2f_t b, const pvec2f_t c, const pvec2f_t d, pvec2f_t point);

/**
 * @return  @c true, if the two lines intercept.
 */
boolean V2f_Intercept2(const pvec2f_t a, const pvec2f_t b, const pvec2f_t c, const pvec2f_t d, pvec2f_t point, float *abFrac, float *cdFrac);

/**
 * Linear interpolation between a and b, by c.
 */
void V2f_Lerp(pvec2f_t dest, const pvec2f_t a, const pvec2f_t b, float c);

/**
 * Left/top is the min-point.  Right/bottom is the max-point.
 */
void V2f_InitBox(arvec2f_t box, const pvec2f_t point);

void V2f_AddToBox(arvec2f_t box, const pvec2f_t point);

void V2f_CopyBox(arvec2f_t dest, const arvec2f_t src);

/**
 * 3-dimensions:
 */

typedef vectorcompf_t vec3f_t[3];
typedef const float const_pvec3f_t[3];
typedef vectorcompf_t* pvec3f_t;
typedef vec3f_t* arvec3f_t;

/**
 * Set the vector's x, y and z components.
 */
void V3f_Set(pvec3f_t vec, vectorcompf_t x, vectorcompf_t y, vectorcompf_t z);

void V3f_SetFixed(pvec3f_t vec, fixed_t x, fixed_t y, fixed_t z);

/**
 * 3-dimensional vector length.
 */
float V3f_Length(const pvec3f_t vec);

/**
 * The distance between two points.
 */
float V3f_Distance(const pvec3f_t a, const pvec3f_t b);

/**
 * Normalize a 3-dimensional vector.
 *
 * @return  The length of the vector.
 */
float V3f_Normalize(pvec3f_t vec);

/**
 * Make a copy of the source vector.
 */
void V3f_Copy(pvec3f_t dest, const_pvec3f_t src);

/**
 * Multiply the vector by the scalar.
 */
void V3f_Scale(pvec3f_t vec, float scalar);

/**
 * Calculate the sum of two 3-dimensional vectors.
 */
void V3f_Sum(pvec3f_t dest, const_pvec3f_t src1, const_pvec3f_t src2);

/**
 * Subtract src1 from src2, return result in 'dest'.
 */
void V3f_Subtract(pvec3f_t dest, const_pvec3f_t src1, const_pvec3f_t src2);

/**
 * Calculate the dot product of the two vectors.
 */
float V3f_DotProduct(const_pvec3f_t a, const_pvec3f_t b);

/**
 * Calculate the cross product of two vectors.
 *
 * @param dest  Result will be written back here.
 * @param src1  First vector.
 * @param src2  Second vector.
 */
void V3f_CrossProduct(pvec3f_t dest, const_pvec3f_t src1, const_pvec3f_t src2);

/**
 * Cross product of two vectors composed of three points.
 *
 * @param dest  Result will be written back here.
 * @param v1  First vector.
 * @param v2  Second vector.
 * @param v3  Third vector.
 */
void V3f_PointCrossProduct(pvec3f_t dest, const pvec3f_t v1, const pvec3f_t v2, const pvec3f_t v3);

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
float V3f_ClosestPointOnPlane(pvec3f_t dest, const_pvec3f_t planeNormal, const_pvec3f_t planePoint, const_pvec3f_t arbPoint);

/**
 * Determine which axis of the given vector is the major.
 */
int V3f_MajorAxis(const pvec3f_t vec);

/**
 * @return  @c true, if the vector is a zero vector.
 */
boolean V3f_IsZero(const pvec3f_t vec);

/**
 * Linear interpolation between a and b, by c.
 */
void V3f_Lerp(pvec3f_t dest, const pvec3f_t a, const pvec3f_t b, float c);

/**
 * Given a normalized normal, construct up and right vectors, oriented about
 * @a normal in our right-handed world coordinate space.
 *
 * @param tangent  The 'right' vector will be written back here.
 * @param bitangent  The 'up' vector will be written back here.
 * @param normal  Normal to construct tangents for.
 */
void V3f_BuildTangents(pvec3f_t tangent, pvec3f_t bitangent, const_pvec3f_t normal);

/**
 * 4-dimensions:
 */

typedef vectorcompf_t vec4f_t[4];
typedef const float const_pvec4f_t[4];
typedef vectorcompf_t* pvec4f_t;
typedef vec4f_t* arvec4f_t;

/**
 * Set the vector's x, y, z and w components.
 */
void V4f_Set(pvec4f_t vec, vectorcompf_t x, vectorcompf_t y, vectorcompf_t z, vectorcompf_t w);

void V4f_SetFixed(pvec4f_t vec, fixed_t x, fixed_t y, fixed_t z, fixed_t w);

/**
 * 4-dimensional vector length.
 */
float V4f_Length(const pvec4f_t vec);

/**
 * The distance between two points.
 */
float V4f_Distance(const pvec4f_t a, const pvec4f_t b);

/**
 * Normalize a 4-dimensional vector.
 *
 * @return  The length of the vector.
 */
float V4f_Normalize(pvec4f_t vec);

/**
 * Make a copy of the source vector.
 */
void V4f_Copy(pvec4f_t dest, const_pvec4f_t src);

/**
 * Multiply the vector by the scalar.
 */
void V4f_Scale(pvec4f_t vec, float scalar);

/**
 * Calculate the sum of two 4-dimensional vectors.
 */
void V4f_Sum(pvec4f_t dest, const_pvec4f_t src1, const_pvec4f_t src2);

/**
 * Subtract src1 from src2, return result in 'dest'.
 */
void V4f_Subtract(pvec4f_t dest, const_pvec4f_t src1, const_pvec4f_t src2);

/**
 * @return  @c true, if the vector is a zero vector.
 */
boolean V4f_IsZero(const pvec4f_t vec);

/**
 * Linear interpolation between a and b, by c.
 */
void V4f_Lerp(pvec4f_t dest, const pvec4f_t a, const pvec4f_t b, float c);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /// LIBDENG_DATA_VECTOR
