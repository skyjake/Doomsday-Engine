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

#ifdef __cplusplus
extern "C" {
#endif

/**
 * 2-dimensions:
 */

typedef float vectorcomp_t;
typedef vectorcomp_t vec2_t[2];
typedef const float const_pvec2_t[2];
typedef vectorcomp_t* pvec2_t;
typedef vec2_t* arvec2_t;

/**
 * Set the vector's x and y components.
 */
void V2_Set(pvec2_t vec, vectorcomp_t x, vectorcomp_t y);

void V2_SetFixed(pvec2_t vec, fixed_t x, fixed_t y);

/**
 * 2-dimensional vector length.
 */
float V2_Length(const pvec2_t vector);

/**
 * The distance between two points.
 */
float V2_Distance(const pvec2_t a, const pvec2_t b);

/**
 * Normalize a 2-dimensional vector.
 *
 * @return  The length of the vector.
 */
float V2_Normalize(pvec2_t vec);

/**
 * Make a copy of the source vector.
 */
void V2_Copy(pvec2_t dest, const_pvec2_t src);

/**
 * Multiply the vector by the scalar.
 */
void V2_Scale(pvec2_t vector, float scalar);

/**
 * Rotate the vector by a radian angle.
 */
void V2_Rotate(pvec2_t vec, float radians);

/**
 * Calculate the sum of two 2-dimensional vectors.
 */
void V2_Sum(pvec2_t dest, const_pvec2_t src1, const_pvec2_t src2);

/**
 * Subtract src1 from src2, return result in 'dest'.
 */
void V2_Subtract(pvec2_t dest, const_pvec2_t src1, const_pvec2_t src2);

/**
 * Calculate the dot product of the two vectors.
 */
float V2_DotProduct(const pvec2_t a, const pvec2_t b);

/**
 * Calculate the scalar projection of 'a' onto 'b': dot(a,b)/len(b)
 */
float V2_ScalarProject(const pvec2_t a, const pvec2_t b);

/**
 * Project 'a' onto 'b' and store the resulting vector to 'dest':
 * dot(a,b)/dot(b,b)*b
 */
void V2_Project(pvec2_t dest, const pvec2_t a, const pvec2_t b);

/**
 * @return  @c true, if the two vectors are parallel.
 */
boolean V2_IsParallel(const pvec2_t a, const pvec2_t b);

/**
 * @return  @c true, if the vector is a zero vector.
 */
boolean V2_IsZero(const pvec2_t vec);

/**
 * Determine where the two lines cross each other.  Notice that the
 * lines are defined with a point and a vector.
 *
 * @return  A scaling factor for the first line.
 */
float V2_Intersection(const_pvec2_t p1, const_pvec2_t delta1, const_pvec2_t p2, const_pvec2_t delta2, pvec2_t point);

/**
 * Intersection of lines a->b and c->d.  Unlike V2_Intersection(), the
 * arguments are all points.
 */
float V2_Intercept(const pvec2_t a, const pvec2_t b, const pvec2_t c, const pvec2_t d, pvec2_t point);

/**
 * @return  @c true, if the two lines intercept.
 */
boolean V2_Intercept2(const pvec2_t a, const pvec2_t b, const pvec2_t c, const pvec2_t d, pvec2_t point, float *abFrac, float *cdFrac);

/**
 * Linear interpolation between a and b, by c.
 */
void V2_Lerp(pvec2_t dest, const pvec2_t a, const pvec2_t b, float c);

/**
 * Left/top is the min-point.  Right/bottom is the max-point.
 */
void V2_InitBox(arvec2_t box, const pvec2_t point);

void V2_AddToBox(arvec2_t box, const pvec2_t point);

void V2_CopyBox(arvec2_t dest, const arvec2_t src);

/**
 * 3-dimensions:
 */

typedef vectorcomp_t vec3_t[3];
typedef const float const_pvec3_t[3];
typedef vectorcomp_t* pvec3_t;
typedef vec3_t* arvec3_t;

/**
 * Set the vector's x, y and z components.
 */
void V3_Set(pvec3_t vec, vectorcomp_t x, vectorcomp_t y, vectorcomp_t z);

void V3_SetFixed(pvec3_t vec, fixed_t x, fixed_t y, fixed_t z);

/**
 * 3-dimensional vector length.
 */
float V3_Length(const pvec3_t vec);

/**
 * The distance between two points.
 */
float V3_Distance(const pvec3_t a, const pvec3_t b);

/**
 * Normalize a 3-dimensional vector.
 *
 * @return  The length of the vector.
 */
float V3_Normalize(pvec3_t vec);

/**
 * Make a copy of the source vector.
 */
void V3_Copy(pvec3_t dest, const_pvec3_t src);

/**
 * Multiply the vector by the scalar.
 */
void V3_Scale(pvec3_t vec, float scalar);

/**
 * Calculate the sum of two 3-dimensional vectors.
 */
void V3_Sum(pvec3_t dest, const_pvec3_t src1, const_pvec3_t src2);

/**
 * Subtract src1 from src2, return result in 'dest'.
 */
void V3_Subtract(pvec3_t dest, const_pvec3_t src1, const_pvec3_t src2);

/**
 * Calculate the dot product of the two vectors.
 */
float V3_DotProduct(const_pvec3_t a, const_pvec3_t b);

/**
 * Calculate the cross product of two vectors.
 *
 * @param dest  Result will be written back here.
 * @param src1  First vector.
 * @param src2  Second vector.
 */
void V3_CrossProduct(pvec3_t dest, const_pvec3_t src1, const_pvec3_t src2);

/**
 * Cross product of two vectors composed of three points.
 *
 * @param dest  Result will be written back here.
 * @param v1  First vector.
 * @param v2  Second vector.
 * @param v3  Third vector.
 */
void V3_PointCrossProduct(pvec3_t dest, const pvec3_t v1, const pvec3_t v2, const pvec3_t v3);

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
float V3_ClosestPointOnPlane(pvec3_t dest, const_pvec3_t planeNormal, const_pvec3_t planePoint, const_pvec3_t arbPoint);

/**
 * Determine which axis of the given vector is the major.
 */
int V3_MajorAxis(const pvec3_t vec);

/**
 * @return  @c true, if the vector is a zero vector.
 */
boolean V3_IsZero(const pvec3_t vec);

/**
 * Linear interpolation between a and b, by c.
 */
void V3_Lerp(pvec3_t dest, const pvec3_t a, const pvec3_t b, float c);

/**
 * Given a normalized normal, construct up and right vectors, oriented about
 * @a normal in our right-handed world coordinate space.
 *
 * @param tangent  The 'right' vector will be written back here.
 * @param bitangent  The 'up' vector will be written back here.
 * @param normal  Normal to construct tangents for.
 */
void V3_BuildTangents(pvec3_t tangent, pvec3_t bitangent, const_pvec3_t normal);

/**
 * 4-dimensions:
 */

typedef vectorcomp_t vec4_t[4];
typedef const float const_pvec4_t[4];
typedef vectorcomp_t* pvec4_t;
typedef vec4_t* arvec4_t;

/**
 * Set the vector's x, y, z and w components.
 */
void V4_Set(pvec4_t vec, vectorcomp_t x, vectorcomp_t y, vectorcomp_t z, vectorcomp_t w);

void V4_SetFixed(pvec4_t vec, fixed_t x, fixed_t y, fixed_t z, fixed_t w);

/**
 * 4-dimensional vector length.
 */
float V4_Length(const pvec4_t vec);

/**
 * The distance between two points.
 */
float V4_Distance(const pvec4_t a, const pvec4_t b);

/**
 * Normalize a 4-dimensional vector.
 *
 * @return  The length of the vector.
 */
float V4_Normalize(pvec4_t vec);

/**
 * Make a copy of the source vector.
 */
void V4_Copy(pvec4_t dest, const_pvec4_t src);

/**
 * Multiply the vector by the scalar.
 */
void V4_Scale(pvec4_t vec, float scalar);

/**
 * Calculate the sum of two 4-dimensional vectors.
 */
void V4_Sum(pvec4_t dest, const_pvec4_t src1, const_pvec4_t src2);

/**
 * Subtract src1 from src2, return result in 'dest'.
 */
void V4_Subtract(pvec4_t dest, const_pvec4_t src1, const_pvec4_t src2);

/**
 * @return  @c true, if the vector is a zero vector.
 */
boolean V4_IsZero(const pvec4_t vec);

/**
 * Linear interpolation between a and b, by c.
 */
void V4_Lerp(pvec4_t dest, const pvec4_t a, const pvec4_t b, float c);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /// LIBDENG_DATA_VECTOR
