/* DE1: $Id$
 * Copyright (C) 2003, 2004 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * along with this program; if not: http://www.opensource.org/
 */

/*
 * m_vector.c: Vector Math
 */

// HEADER FILES ------------------------------------------------------------

#include <math.h>

#include "de_base.h"
#include "m_vector.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

/*
 * Set the vector's x and y components.
 */
void V2_Set(pvec2_t vec, vectorcomp_t x, vectorcomp_t y)
{
    vec[VX] = x;
    vec[VY] = y;
}

void V2_SetFixed(pvec2_t vec, fixed_t x, fixed_t y)
{
    vec[VX] = FIX2FLT(x);
    vec[VY] = FIX2FLT(y);
}

/*
 * 2-dimensional vector length.
 */
float V2_Length(const pvec2_t vec)
{
    if(vec[VX] == 0 && vec[VY] == 0)
        return 0;
    return sqrt(vec[VX] * vec[VX] + vec[VY] * vec[VY]);
}

/*
 * The distance between two points.
 */
float V2_Distance(const pvec2_t a, const pvec2_t b)
{
    vec2_t  vec;

    V2_Subtract(vec, b, a);
    return V2_Length(vec);
}

/*
 * Normalize a 2-dimensional vector.  Returns the length of the
 * vector.
 */
float V2_Normalize(float *vec)
{
    float   len = V2_Length(vec);

    if(len != 0)
    {
        vec[VX] /= len;
        vec[VY] /= len;
    }
    return len;
}

/*
 * Make a copy of the source vector.
 */
void V2_Copy(pvec2_t dest, const pvec2_t src)
{
    dest[VX] = src[VX];
    dest[VY] = src[VY];
}

/*
 * Multiply the vector by the scalar.
 */
void V2_Scale(pvec2_t vec, float scalar)
{
    vec[VX] *= scalar;
    vec[VY] *= scalar;
}

/*
 * Calculate the sum of two 2-dimensional vectors.
 */
void V2_Sum(pvec2_t dest, const pvec2_t src1, const pvec2_t src2)
{
    dest[VX] = src1[VX] + src2[VX];
    dest[VY] = src1[VY] + src2[VY];
}

/*
 * Subtract src1 from src2, return result in 'dest'.
 */
void V2_Subtract(pvec2_t dest, const pvec2_t src1, const pvec2_t src2)
{
    dest[VX] = src1[VX] - src2[VX];
    dest[VY] = src1[VY] - src2[VY];
}

/*
 * Calculate the dot product of the two vectors.
 */
float V2_DotProduct(const pvec2_t a, const pvec2_t b)
{
    return a[VX] * b[VX] + a[VY] * b[VY];
}

/*
 * Calculate the scalar projection of 'a' onto 'b': dot(a,b)/len(b)
 */
float V2_ScalarProject(const pvec2_t a, const pvec2_t b)
{
    float   dot = V2_DotProduct(a, b);
    float   len = V2_Length(b);

    if(len == 0)
        return 0;
    return dot / len;
}

/*
 * Project 'a' onto 'b' and store the resulting vector to 'dest':
 * dot(a,b)/dot(b,b)*b
 */
void V2_Project(pvec2_t dest, const pvec2_t a, const pvec2_t b)
{
    float   div = V2_DotProduct(b, b);

    if(div == 0)
    {
        dest[VX] = dest[VY] = 0;
        return;
    }
    V2_Copy(dest, b);
    V2_Scale(dest, V2_DotProduct(a, b) / div);
}

/*
 * Returns true if the two vectors are parallel.
 */
boolean V2_IsParallel(const pvec2_t a, const pvec2_t b)
{
    float   aLen = V2_Length(a);
    float   bLen = V2_Length(b);
    float   dot;

    // Both must be non-zero vectors.
    if(aLen == 0 || bLen == 0)
        return true;

    dot = V2_DotProduct(a, b) / aLen / bLen;

    // If it's close enough, we'll consider them parallel.
#define EPSILON .9999f
    return dot > EPSILON || dot < -EPSILON;
}

/*
 * Returns true if the vector is a zero vector.
 */
boolean V2_IsZero(const pvec2_t vec)
{
    return vec[VX] == 0 && vec[VY] == 0;
}

/*
 * Determine where the two lines cross each other.  Notice that the
 * lines are defined with a point and a vector.  Returns a scaling
 * factor for the first line.
 */
float V2_Intersection(const pvec2_t p1, const pvec2_t delta1, const pvec2_t p2,
                      const pvec2_t delta2, pvec2_t point)
{
    /*
     *     (YA-YC)(XD-XC)-(XA-XC)(YD-YC)
     * r = -----------------------------
     *     (XB-XA)(YD-YC)-(YB-YA)(XD-XC)
     */

    float   r, div = delta1[VX] * delta2[VY] - delta1[VY] * delta2[VX];
    int     i;

    if(div == 0)
    {
        // Special case: lines are parallel.
        r = 0;
    }
    else
    {
        r = ((p1[VY] - p2[VY]) * delta2[VX] -
             (p1[VX] - p2[VX]) * delta2[VY]) / div;
    }

    /*
     * XI=XA+r(XB-XA)
     * YI=YA+r(YB-YA)
     */

    if(point)
    {
        // Calculate the intersection point.
        for(i = 0; i < 2; i++)
            point[i] = p1[i] + r * delta1[i];
    }

    // Return the scaling factor.
    return r;
}

/*
 * Intersection of lines a->b and c->d.  Unlike V2_Intersection(), the
 * arguments are all points.
 */
float V2_Intercept(const pvec2_t a, const pvec2_t b, const pvec2_t c,
                   const pvec2_t d, pvec2_t point)
{
    vec2_t  ab = { b[VX] - a[VX], b[VY] - a[VY] };
    vec2_t  cd = { d[VX] - c[VX], d[VY] - c[VY] };

    return V2_Intersection(a, ab, c, cd, point);
}

/*
 * Returns true if the two lines intercept.
 */
boolean V2_Intercept2(const pvec2_t a, const pvec2_t b, const pvec2_t c,
                      const pvec2_t d, pvec2_t point, float *abFrac,
                      float *cdFrac)
{
    float   ab, cd;

    ab = V2_Intercept(a, b, c, d, point);
    cd = V2_Intercept(c, d, a, b, NULL);

    if(abFrac)
        *abFrac = ab;
    if(cdFrac)
        *cdFrac = cd;

    return (ab >= 0 && ab <= 1 && cd >= 0 && cd <= 1);
}

/*
 * Linear interpolation between a and b, by c.
 */
void V2_Lerp(pvec2_t dest, const pvec2_t a, const pvec2_t b, float c)
{
    int     i;

    for(i = 0; i < 2; i++)
    {
        dest[i] = a[i] + c * (b[i] - a[i]);
    }
}

/*
 * Left/top is the min-point.  Right/bottom is the max-point.
 */
void V2_InitBox(arvec2_t box, const pvec2_t point)
{
    V2_Copy(box[0], point);
    V2_Copy(box[1], point);
}

void V2_AddToBox(arvec2_t box, const pvec2_t point)
{
    if(point[VX] < box[0][VX])
        box[0][VX] = point[VX];
    if(point[VX] > box[1][VX])
        box[1][VX] = point[VX];

    if(point[VY] < box[0][VY])
        box[0][VY] = point[VY];
    if(point[VY] > box[1][VY])
        box[1][VY] = point[VY];
}
