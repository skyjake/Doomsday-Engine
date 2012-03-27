/**
 * @file m_vector.c
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

#include <math.h>
#include <float.h>

#include "de_base.h"
#include "m_vector.h"

void V2f_Set(pvec2f_t vec, vectorcompf_t x, vectorcompf_t y)
{
    vec[VX] = x;
    vec[VY] = y;
}

void V2f_SetFixed(pvec2f_t vec, fixed_t x, fixed_t y)
{
    vec[VX] = FIX2FLT(x);
    vec[VY] = FIX2FLT(y);
}

float V2f_Length(const pvec2f_t vec)
{
    if(vec[VX] == 0 && vec[VY] == 0) return 0;
    return sqrt(vec[VX] * vec[VX] + vec[VY] * vec[VY]);
}

float V2f_Distance(const pvec2f_t a, const pvec2f_t b)
{
    vec2f_t vec;
    V2f_Subtract(vec, b, a);
    return V2f_Length(vec);
}

float V2f_Normalize(pvec2f_t vec)
{
    float len = V2f_Length(vec);
    if(len != 0)
    {
        vec[VX] /= len;
        vec[VY] /= len;
    }
    return len;
}

void V2f_Copy(pvec2f_t dest, const_pvec2f_t src)
{
    dest[VX] = src[VX];
    dest[VY] = src[VY];
}

void V2f_Scale(pvec2f_t vec, float scalar)
{
    vec[VX] *= scalar;
    vec[VY] *= scalar;
}

void V2f_Rotate(pvec2f_t vec, float radians)
{
    const float c = cos(radians);
    const float s = sin(radians);
    const float x = c * vec[VX] - s * vec[VY];
    const float y = s * vec[VX] + c * vec[VY];

    vec[VX] = x;
    vec[VY] = y;
}

void V2f_Sum(pvec2f_t dest, const_pvec2f_t src1, const_pvec2f_t src2)
{
    dest[VX] = src1[VX] + src2[VX];
    dest[VY] = src1[VY] + src2[VY];
}

void V2f_Subtract(pvec2f_t dest, const_pvec2f_t src1, const_pvec2f_t src2)
{
    dest[VX] = src1[VX] - src2[VX];
    dest[VY] = src1[VY] - src2[VY];
}

float V2f_DotProduct(const pvec2f_t a, const pvec2f_t b)
{
    return a[VX] * b[VX] + a[VY] * b[VY];
}

float V2f_ScalarProject(const pvec2f_t a, const pvec2f_t b)
{
    float dot, len = V2f_Length(b);
    if(len == 0) return 0;

    dot = V2f_DotProduct(a, b);
    return dot / len;
}

void V2f_Project(pvec2f_t dest, const pvec2f_t a, const pvec2f_t b)
{
    float div = V2f_DotProduct(b, b);
    if(div == 0)
    {
        dest[VX] = dest[VY] = 0;
        return;
    }

    V2f_Copy(dest, b);
    V2f_Scale(dest, V2f_DotProduct(a, b) / div);
}

boolean V2f_IsParallel(const pvec2f_t a, const pvec2f_t b)
{
#define EPSILON .9999f

    float aLen = V2f_Length(a);
    float bLen = V2f_Length(b);
    float dot;

    // Both must be non-zero vectors.
    if(aLen == 0 || bLen == 0) return true;

    dot = V2f_DotProduct(a, b) / aLen / bLen;

    // If it's close enough, we'll consider them parallel.
    return dot > EPSILON || dot < -EPSILON;

#undef EPSILON
}

boolean V2f_IsZero(const pvec2f_t vec)
{
    return vec[VX] == 0 && vec[VY] == 0;
}

float V2f_Intersection(const_pvec2f_t p1, const_pvec2f_t delta1, const_pvec2f_t p2,
    const_pvec2f_t delta2, pvec2f_t point)
{
    /*
     *     (YA-YC)(XD-XC)-(XA-XC)(YD-YC)
     * r = -----------------------------
     *     (XB-XA)(YD-YC)-(YB-YA)(XD-XC)
     */

    float r, div;
    int i;

    div = delta1[VX] * delta2[VY] - delta1[VY] * delta2[VX];

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
        for(i = 0; i < 2; ++i)
            point[i] = p1[i] + r * delta1[i];
    }

    // Return the scaling factor.
    return r;
}

float V2f_Intercept(const pvec2f_t a, const pvec2f_t b, const pvec2f_t c,
    const pvec2f_t d, pvec2f_t point)
{
    vec2f_t ab, cd;

    ab[0] = b[VX] - a[VX];
    ab[1] = b[VY] - a[VY];
    cd[0] = d[VX] - c[VX];
    cd[1] = d[VY] - c[VY];

    return V2f_Intersection(a, ab, c, cd, point);
}

boolean V2f_Intercept2(const pvec2f_t a, const pvec2f_t b, const pvec2f_t c,
    const pvec2f_t d, pvec2f_t point, float* abFrac, float* cdFrac)
{
    float ab, cd;

    ab = V2f_Intercept(a, b, c, d, point);
    cd = V2f_Intercept(c, d, a, b, NULL);

    if(abFrac) *abFrac = ab;
    if(cdFrac) *cdFrac = cd;

    return (ab >= 0 && ab <= 1 && cd >= 0 && cd <= 1);
}

void V2f_Lerp(pvec2f_t dest, const pvec2f_t a, const pvec2f_t b, float c)
{
    uint i;
    for(i = 0; i < 2; ++i)
    {
        dest[i] = a[i] + c * (b[i] - a[i]);
    }
}

void V2f_InitBox(arvec2f_t box, const pvec2f_t point)
{
    V2f_Copy(box[0], point);
    V2f_Copy(box[1], point);
}

void V2f_AddToBox(arvec2f_t box, const pvec2f_t point)
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

void V2f_CopyBox(arvec2f_t dest, const arvec2f_t src)
{
    V2f_Copy(dest[0], src[0]);
    V2f_Copy(dest[1], src[1]);
}

void V3f_Set(pvec3f_t vec, vectorcompf_t x, vectorcompf_t y, vectorcompf_t z)
{
    vec[VX] = x;
    vec[VY] = y;
    vec[VZ] = z;
}

void V3f_SetFixed(pvec3f_t vec, fixed_t x, fixed_t y, fixed_t z)
{
    vec[VX] = FIX2FLT(x);
    vec[VY] = FIX2FLT(y);
    vec[VZ] = FIX2FLT(z);
}

float V3f_Length(const pvec3f_t vec)
{
    if(vec[VX] == 0 && vec[VY] == 0 && vec[VZ] == 0) return 0;
    return sqrt(vec[VX] * vec[VX] + vec[VY] * vec[VY] + vec[VZ] * vec[VZ]);
}

float V3f_Distance(const pvec3f_t a, const pvec3f_t b)
{
    vec3f_t vec;
    V3f_Subtract(vec, b, a);
    return V3f_Length(vec);
}

float V3f_Normalize(pvec3f_t vec)
{
    float len = V3f_Length(vec);
    if(len != 0)
    {
        vec[VX] /= len;
        vec[VY] /= len;
        vec[VZ] /= len;
    }
    return len;
}

void V3f_Copy(pvec3f_t dest, const_pvec3f_t src)
{
    dest[VX] = src[VX];
    dest[VY] = src[VY];
    dest[VZ] = src[VZ];
}

void V3f_Scale(pvec3f_t vec, float scalar)
{
    vec[VX] *= scalar;
    vec[VY] *= scalar;
    vec[VZ] *= scalar;
}

void V3f_Sum(pvec3f_t dest, const_pvec3f_t src1, const_pvec3f_t src2)
{
    dest[VX] = src1[VX] + src2[VX];
    dest[VY] = src1[VY] + src2[VY];
    dest[VZ] = src1[VZ] + src2[VZ];
}

void V3f_Subtract(pvec3f_t dest, const_pvec3f_t src1, const_pvec3f_t src2)
{
    dest[VX] = src1[VX] - src2[VX];
    dest[VY] = src1[VY] - src2[VY];
    dest[VZ] = src1[VZ] - src2[VZ];
}

float V3f_DotProduct(const_pvec3f_t a, const_pvec3f_t b)
{
    return a[VX] * b[VX] + a[VY] * b[VY] + a[VZ] * b[VZ];
}

void V3f_CrossProduct(pvec3f_t dest, const_pvec3f_t src1, const_pvec3f_t src2)
{
    dest[VX] = src1[VY] * src2[VZ] - src1[VZ] * src2[VY];
    dest[VY] = src1[VZ] * src2[VX] - src1[VX] * src2[VZ];
    dest[VZ] = src1[VX] * src2[VY] - src1[VY] * src2[VX];
}

void V3f_PointCrossProduct(pvec3f_t dest, const pvec3f_t v1, const pvec3f_t v2,
    const pvec3f_t v3)
{
    vec3f_t a, b;
    V3f_Subtract(a, v2, v1);
    V3f_Subtract(b, v3, v1);
    V3f_CrossProduct(dest, a, b);
}

float V3f_ClosestPointOnPlane(pvec3f_t dest, const_pvec3f_t planeNormal,
    const_pvec3f_t planePoint, const_pvec3f_t arbPoint)
{
    vec3f_t pvec;
    float distance;

    V3f_Subtract(pvec, arbPoint, planePoint);
    distance = V3f_DotProduct(planeNormal, pvec);

    V3f_Copy(dest, planeNormal);
    V3f_Scale(dest, distance);
    V3f_Subtract(dest, arbPoint, dest);

    return distance;
}

int V3f_MajorAxis(const pvec3f_t vec)
{
    vec3f_t fn;
    int axis;

    V3f_Set(fn, fabsf(vec[VX]), fabsf(vec[VY]), fabsf(vec[VZ]));

    axis = VX;
    if(fn[VY] > fn[axis])
        axis = VY;
    if(fn[VZ] > fn[axis])
        axis = VZ;

    return axis;
}

boolean V3f_IsZero(const pvec3f_t vec)
{
    return vec[VX] == 0 && vec[VY] == 0 && vec[VZ] == 0;
}

void V3f_Lerp(pvec3f_t dest, const pvec3f_t a, const pvec3f_t b, float c)
{
    uint i;
    for(i = 0; i < 3; ++i)
    {
        dest[i] = a[i] + c * (b[i] - a[i]);
    }
}

void V3f_BuildTangents(pvec3f_t tangent, pvec3f_t bitangent, const_pvec3f_t normal)
{
    const vec3f_t rotm[3] = {
        { 0.f, 0.f, 1.f },
        { 0.f, 0.f, 1.f },
        { 0.f, 0.f, 1.f }
    };
    int axis = VX;
    vec3f_t fn;

    V3f_Set(fn, fabsf(normal[VX]), fabsf(normal[VY]), fabsf(normal[VZ]));

    if(fn[VY] > fn[axis])
        axis = VY;
    if(fn[VZ] > fn[axis])
        axis = VZ;

    if(fabsf(fn[VX] - 1.0f) < FLT_EPSILON ||
       fabsf(fn[VY] - 1.0f) < FLT_EPSILON ||
       fabsf(fn[VZ] - 1.0f) < FLT_EPSILON)
    {
        // We must build the tangent vector manually.
        if(axis == VX && normal[VX] > 0.f)
        {
            V3f_Set(tangent, 0.f, 1.f, 0.f);
        }
        else if(axis == VX)
        {
            V3f_Set(tangent, 0.f, -1.f, 0.f);
        }

        if(axis == VY && normal[VY] > 0.f)
        {
            V3f_Set(tangent, -1.f, 0.f, 0.f);
        }
        else if(axis == VY)
        {
            V3f_Set(tangent, 1.f, 0.f, 0.f);
        }

        if(axis == VZ)
        {
            V3f_Set(tangent, 1.f, 0.f, 0.f);
        }
    }
    else
    {
        // Can use a cross product of the normal.
        V3f_CrossProduct(tangent, (pvec3f_t)rotm[axis], normal);
        V3f_Normalize(tangent);
    }

    V3f_CrossProduct(bitangent, tangent, normal);
    V3f_Normalize(bitangent);
}

void V4f_Set(pvec4f_t vec, vectorcompf_t x, vectorcompf_t y, vectorcompf_t z, vectorcompf_t w)
{
    vec[0] = x;
    vec[1] = y;
    vec[2] = z;
    vec[3] = w;
}

void V4f_SetFixed(pvec4f_t vec, fixed_t x, fixed_t y, fixed_t z, fixed_t w)
{
    vec[0] = FIX2FLT(x);
    vec[1] = FIX2FLT(y);
    vec[2] = FIX2FLT(z);
    vec[3] = FIX2FLT(w);
}

float V4f_Length(const pvec4f_t vec)
{
    if(vec[0] == 0 && vec[1] == 0 && vec[2] == 0 && vec[3] == 0) return 0;
    return sqrt(vec[0] * vec[0] + vec[1] * vec[1] +
                vec[2] * vec[2] + vec[3] * vec[3]);
}

float V4f_Distance(const pvec4f_t a, const pvec4f_t b)
{
    vec4f_t vec;
    V4f_Subtract(vec, b, a);
    return V4f_Length(vec);
}

float V4f_Normalize(pvec4f_t vec)
{
    float len = V4f_Length(vec);
    if(len != 0)
    {
        vec[0] /= len;
        vec[1] /= len;
        vec[2] /= len;
        vec[3] /= len;
    }
    return len;
}

void V4f_Copy(pvec4f_t dest, const_pvec4f_t src)
{
    dest[0] = src[0];
    dest[1] = src[1];
    dest[2] = src[2];
    dest[3] = src[3];
}

void V4f_Scale(pvec4f_t vec, float scalar)
{
    vec[0] *= scalar;
    vec[1] *= scalar;
    vec[2] *= scalar;
    vec[3] *= scalar;
}

void V4f_Sum(pvec4f_t dest, const_pvec4f_t src1, const_pvec4f_t src2)
{
    dest[0] = src1[0] + src2[0];
    dest[1] = src1[1] + src2[1];
    dest[2] = src1[2] + src2[2];
    dest[3] = src1[3] + src2[3];
}

void V4f_Subtract(pvec4f_t dest, const_pvec4f_t src1, const_pvec4f_t src2)
{
    dest[0] = src1[0] - src2[0];
    dest[1] = src1[1] - src2[1];
    dest[2] = src1[2] - src2[2];
    dest[3] = src1[3] - src2[3];
}

boolean V4f_IsZero(const pvec4f_t vec)
{
    return vec[0] == 0 && vec[1] == 0 && vec[2] == 0 && vec[3] == 0;
}

void V4f_Lerp(pvec4f_t dest, const pvec4f_t a, const pvec4f_t b, float c)
{
    uint i;
    for(i = 0; i < 4; ++i)
    {
        dest[i] = a[i] + c * (b[i] - a[i]);
    }
}
