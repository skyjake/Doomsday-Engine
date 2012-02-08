/**\file m_vector.h
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2004-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2012 Daniel Swanson <danij@dengine.net>
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

/**
 * Vector Math
 */

#ifndef LIBDENG_DATA_VECTOR_H
#define LIBDENG_DATA_VECTOR_H

typedef float         vectorcomp_t;
typedef vectorcomp_t  vec2_t[2];
typedef const float   const_pvec2_t[2];
typedef vectorcomp_t  vec3_t[3];
typedef const float   const_pvec3_t[3];
typedef vectorcomp_t  vec4_t[4];
typedef const float   const_pvec4_t[4];
typedef vectorcomp_t *pvec2_t;
typedef vectorcomp_t *pvec3_t;
typedef vectorcomp_t *pvec4_t;
typedef vec2_t       *arvec2_t;
typedef vec3_t       *arvec3_t;
typedef vec4_t       *arvec4_t;

// 2-dimensions:
void            V2_Set(pvec2_t vec, vectorcomp_t x, vectorcomp_t y);
void            V2_SetFixed(pvec2_t vec, fixed_t x, fixed_t y);
float           V2_Length(const pvec2_t vector);
float           V2_Distance(const pvec2_t a, const pvec2_t b);
float           V2_Normalize(pvec2_t vec);
void            V2_Copy(pvec2_t dest, const_pvec2_t src);
void            V2_Scale(pvec2_t vector, float scalar);
void            V2_Rotate(pvec2_t vec, float radians);
void            V2_Sum(pvec2_t dest, const_pvec2_t src1, const_pvec2_t src2);
void            V2_Subtract(pvec2_t dest, const_pvec2_t src1,
                            const_pvec2_t src2);
float           V2_DotProduct(const pvec2_t a, const pvec2_t b);
float           V2_ScalarProject(const pvec2_t a, const pvec2_t b);
void            V2_Project(pvec2_t dest, const pvec2_t a, const pvec2_t b);
boolean         V2_IsParallel(const pvec2_t a, const pvec2_t b);
boolean         V2_IsZero(const pvec2_t vec);
float           V2_Intersection(const_pvec2_t p1, const_pvec2_t delta1,
                                const_pvec2_t p2, const_pvec2_t delta2,
                                pvec2_t point);
float           V2_Intercept(const pvec2_t a, const pvec2_t b, const pvec2_t c,
                             const pvec2_t d, pvec2_t point);
boolean         V2_Intercept2(const pvec2_t a, const pvec2_t b,
                              const pvec2_t c, const pvec2_t d, pvec2_t point,
                              float *abFrac, float *cdFrac);
void            V2_Lerp(pvec2_t dest, const pvec2_t a, const pvec2_t b,
                        float c);
void            V2_InitBox(arvec2_t box, const pvec2_t point);
void            V2_AddToBox(arvec2_t box, const pvec2_t point);

// 3-dimensions:
void            V3_Set(pvec3_t vec, vectorcomp_t x, vectorcomp_t y, vectorcomp_t z);
void            V3_SetFixed(pvec3_t vec, fixed_t x, fixed_t y, fixed_t z);
float           V3_Length(const pvec3_t vec);
float           V3_Distance(const pvec3_t a, const pvec3_t b);
float           V3_Normalize(pvec3_t vec);
void            V3_Copy(pvec3_t dest, const_pvec3_t src);
void            V3_Scale(pvec3_t vec, float scalar);
void            V3_Sum(pvec3_t dest, const_pvec3_t src1, const_pvec3_t src2);
void            V3_Subtract(pvec3_t dest, const_pvec3_t src1,
                            const_pvec3_t src2);
float           V3_DotProduct(const_pvec3_t a, const_pvec3_t b);
void            V3_CrossProduct(pvec3_t dest, const_pvec3_t src1, const_pvec3_t src2);
void            V3_PointCrossProduct(pvec3_t dest, const pvec3_t v1,
                                     const pvec3_t v2, const pvec3_t v3);

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

int             V3_MajorAxis(const pvec3_t vec);
boolean         V3_IsZero(const pvec3_t vec);
void            V3_Lerp(pvec3_t dest, const pvec3_t a, const pvec3_t b, float c);

/**
 * Given a normalized normal, construct up and right vectors, oriented about
 * @a normal in our right-handed world coordinate space.
 *
 * @param tangent  The 'right' vector will be written back here.
 * @param bitangent  The 'up' vector will be written back here.
 * @param normal  Normal to construct tangents for.
 */
void V3_BuildTangents(pvec3_t tangent, pvec3_t bitangent, const_pvec3_t normal);

// 4-dimensions:
void            V4_Set(pvec4_t vec, vectorcomp_t x, vectorcomp_t y, vectorcomp_t z, vectorcomp_t w);
void            V4_SetFixed(pvec4_t vec, fixed_t x, fixed_t y, fixed_t z, fixed_t w);
float           V4_Length(const pvec4_t vec);
float           V4_Distance(const pvec4_t a, const pvec4_t b);
float           V4_Normalize(pvec4_t vec);
void            V4_Copy(pvec4_t dest, const_pvec4_t src);
void            V4_Scale(pvec4_t vec, float scalar);
void            V4_Sum(pvec4_t dest, const_pvec4_t src1, const_pvec4_t src2);
void            V4_Subtract(pvec4_t dest, const_pvec4_t src1, const_pvec4_t src2);
boolean         V4_IsZero(const pvec4_t vec);
void            V4_Lerp(pvec4_t dest, const pvec4_t a, const pvec4_t b, float c);

#endif /* LIBDENG_DATA_VECTOR_H */
