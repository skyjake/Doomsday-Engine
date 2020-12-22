/** @file vector1.c Vector math (2D, 3D, 4D).
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

#include <math.h>
#include <float.h>

#include "de/legacy/vector1.h"

void V2x_Set(fixed_t vec[], fixed_t x, fixed_t y)
{
    vec[VX] = x;
    vec[VY] = y;
}

fixed_t V2x_Intersection(fixed_t const v1[], fixed_t const v1Delta[],
                         fixed_t const v2[], fixed_t const v2Delta[])
{
    fixed_t r, div = FixedMul(v1Delta[VY] >> 8, v2Delta[VX]) -
                     FixedMul(v1Delta[VX] >> 8, v2Delta[VY]);
    if (div == 0)
    {
        // Parallel.
        r = 0;
    }
    else
    {
        r = FixedMul((v1[VX] - v2[VX]) >> 8, v1Delta[VY]) +
            FixedMul((v2[VY] - v1[VY]) >> 8, v1Delta[VX]);
        r = FixedDiv(r, div);
    }
    return r;
}

int V2x_PointOnLineSide(fixed_t const point[], fixed_t const lineOrigin[], fixed_t const lineDirection[])
{
    if (!lineDirection[VX])
    {
        return (point[VX] <= lineOrigin[VX])? lineDirection[VY] > 0 : lineDirection[VY] < 0;
    }
    else if (!lineDirection[VY])
    {
        return (point[VY] <= lineOrigin[VY])? lineDirection[VX] < 0 : lineDirection[VX] > 0;
    }
    else
    {
        fixed_t dX = point[VX] - lineOrigin[VX];
        fixed_t dY = point[VY] - lineOrigin[VY];

        // Try to quickly decide by comparing signs.
        if ((lineDirection[VY] ^ lineDirection[VX] ^ dX ^ dY) & 0x80000000)
        {
            // Left is negative.
            return ((lineDirection[VY] ^ dX) & 0x80000000)? 1 : 0;
        }
        else
        {
            // if left >= right return 1 else 0.
            return FixedMul(dY >> 8, lineDirection[VX] >> 8) >=
                   FixedMul(lineDirection[VY] >> 8, dX >> 8);
        }
    }
}

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

float V2f_Length(const_pvec2f_t vec)
{
    if (vec[VX] == 0 && vec[VY] == 0) return 0;
    return (float) sqrt(vec[VX] * vec[VX] + vec[VY] * vec[VY]);
}

float V2f_Distance(const_pvec2f_t a, const_pvec2f_t b)
{
    vec2f_t vec;
    V2f_Subtract(vec, b, a);
    return V2f_Length(vec);
}

float V2f_Normalize(pvec2f_t vec)
{
    float len = V2f_Length(vec);
    if (len != 0)
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

void V2f_Copyd(pvec2f_t dest, const_pvec2d_t src)
{
    vec2f_t other;
    V2f_Set(other, (float) src[VX], (float) src[VY]);
    V2f_Copy(dest, other);
}

void V2f_Scale(pvec2f_t vec, float scalar)
{
    vec[VX] *= scalar;
    vec[VY] *= scalar;
}

void V2f_Rotate(pvec2f_t vec, float radians)
{
    float const c = (float) cos(radians);
    float const s = (float) sin(radians);
    float const x = c * vec[VX] - s * vec[VY];
    float const y = s * vec[VX] + c * vec[VY];

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

float V2f_DotProduct(const_pvec2f_t a, const_pvec2f_t b)
{
    return a[VX] * b[VX] + a[VY] * b[VY];
}

float V2f_ScalarProject(const_pvec2f_t a, const_pvec2f_t b)
{
    float dot, len = V2f_Length(b);
    if (len == 0) return 0;

    dot = V2f_DotProduct(a, b);
    return dot / len;
}

float V2f_Project(pvec2f_t dest, const_pvec2f_t a, const_pvec2f_t b)
{
    float div = V2f_DotProduct(b, b);
    if (div == 0)
    {
        dest[VX] = dest[VY] = 0;
        return 0;
    }

    V2f_Copy(dest, b);
    V2f_Scale(dest, V2f_DotProduct(a, b) / div);
    return div;
}

dd_bool V2f_IsParallel(const_pvec2f_t a, const_pvec2f_t b)
{
#define EPSILON .9999f

    float aLen = V2f_Length(a);
    float bLen = V2f_Length(b);
    float dot;

    // Both must be non-zero vectors.
    if (aLen == 0 || bLen == 0) return true;

    dot = V2f_DotProduct(a, b) / aLen / bLen;

    // If it's close enough, we'll consider them parallel.
    return dot > EPSILON || dot < -EPSILON;

#undef EPSILON
}

dd_bool V2f_IsZero(const_pvec2f_t vec)
{
    return vec[VX] == 0 && vec[VY] == 0;
}

float V2f_PointUnitLineDistance(const_pvec2f_t point, const_pvec2f_t linePoint, const_pvec2f_t lineDirection)
{
    return (float) fabs(((linePoint[VY] - point[VY]) * (lineDirection[VX] - linePoint[VX]) -
                         (linePoint[VX] - point[VX]) * (lineDirection[VY] - linePoint[VY])));
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

    if (div == 0)
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

    if (point)
    {
        // Calculate the intersection point.
        for (i = 0; i < 2; ++i)
            point[i] = p1[i] + r * delta1[i];
    }

    // Return the scaling factor.
    return r;
}

float V2f_Intercept(const_pvec2f_t a, const_pvec2f_t b, const_pvec2f_t c,
    const_pvec2f_t d, pvec2f_t point)
{
    vec2f_t ab, cd;

    ab[0] = b[VX] - a[VX];
    ab[1] = b[VY] - a[VY];
    cd[0] = d[VX] - c[VX];
    cd[1] = d[VY] - c[VY];

    return V2f_Intersection(a, ab, c, cd, point);
}

dd_bool V2f_Intercept2(const_pvec2f_t a, const_pvec2f_t b, const_pvec2f_t c,
    const_pvec2f_t d, pvec2f_t point, float* abFrac, float* cdFrac)
{
    float ab, cd;

    ab = V2f_Intercept(a, b, c, d, point);
    cd = V2f_Intercept(c, d, a, b, NULL);

    if (abFrac) *abFrac = ab;
    if (cdFrac) *cdFrac = cd;

    return (ab >= 0 && ab <= 1 && cd >= 0 && cd <= 1);
}

void V2f_Lerp(pvec2f_t dest, const_pvec2f_t a, const_pvec2f_t b, float c)
{
    uint i;
    for (i = 0; i < 2; ++i)
    {
        dest[i] = a[i] + c * (b[i] - a[i]);
    }
}

void V2f_InitBox(arvec2f_t box, const_pvec2f_t point)
{
    V2f_Copy(box[0], point);
    V2f_Copy(box[1], point);
}

void V2f_AddToBox(arvec2f_t box, const_pvec2f_t point)
{
    if (point[VX] < box[0][VX])
        box[0][VX] = point[VX];
    if (point[VX] > box[1][VX])
        box[1][VX] = point[VX];

    if (point[VY] < box[0][VY])
        box[0][VY] = point[VY];
    if (point[VY] > box[1][VY])
        box[1][VY] = point[VY];
}

void V2f_UniteBox(arvec2f_t box, arvec2f_t const other)
{
    if (other[0][VX] < box[0][VX])
        box[0][VX] = other[0][VX];

    if (other[1][VX] > box[1][VX])
        box[1][VX] = other[1][VX];

    if (other[0][VY] < box[0][VY])
        box[0][VY] = other[0][VY];

    if (other[1][VY] > box[1][VY])
        box[1][VY] = other[1][VY];
}

void V2f_CopyBox(arvec2f_t dest, arvec2f_t const src)
{
    V2f_Copy(dest[0], src[0]);
    V2f_Copy(dest[1], src[1]);
}

void V2f_CopyBoxd(arvec2f_t dest, arvec2d_t const src)
{
    vec2f_t other[2];
    V2f_Set(other[0], (float) src[0][VX], (float) src[0][VY]);
    V2f_Set(other[1], (float) src[1][VX], (float) src[1][VY]);
    V2f_CopyBox(dest, other);
}

void V2d_Set(pvec2d_t vec, vectorcompd_t x, vectorcompd_t y)
{
    vec[VX] = x;
    vec[VY] = y;
}

void V2d_SetFixed(pvec2d_t vec, fixed_t x, fixed_t y)
{
    vec[VX] = FIX2FLT(x);
    vec[VY] = FIX2FLT(y);
}

double V2d_Length(const_pvec2d_t vec)
{
    if (vec[VX] == 0 && vec[VY] == 0) return 0;
    return sqrt(vec[VX] * vec[VX] + vec[VY] * vec[VY]);
}

double V2d_Distance(const_pvec2d_t a, const_pvec2d_t b)
{
    vec2d_t vec;
    V2d_Subtract(vec, b, a);
    return V2d_Length(vec);
}

double V2d_Normalize(pvec2d_t vec)
{
    double len = V2d_Length(vec);
    if (len != 0)
    {
        vec[VX] /= len;
        vec[VY] /= len;
    }
    return len;
}

void V2d_Copy(pvec2d_t dest, const_pvec2d_t src)
{
    dest[VX] = src[VX];
    dest[VY] = src[VY];
}

void V2d_Copyf(pvec2d_t dest, const_pvec2f_t srcf)
{
    V2d_Set(dest, srcf[VX], srcf[VY]);
}

void V2d_Scale(pvec2d_t vec, double scalar)
{
    vec[VX] *= scalar;
    vec[VY] *= scalar;
}

void V2d_Rotate(pvec2d_t vec, double radians)
{
    double const c = cos(radians);
    double const s = sin(radians);
    double const x = c * vec[VX] - s * vec[VY];
    double const y = s * vec[VX] + c * vec[VY];

    vec[VX] = x;
    vec[VY] = y;
}

void V2d_Sum(pvec2d_t dest, const_pvec2d_t src1, const_pvec2d_t src2)
{
    dest[VX] = src1[VX] + src2[VX];
    dest[VY] = src1[VY] + src2[VY];
}

void V2d_Subtract(pvec2d_t dest, const_pvec2d_t src1, const_pvec2d_t src2)
{
    dest[VX] = src1[VX] - src2[VX];
    dest[VY] = src1[VY] - src2[VY];
}

double V2d_PointLineDistance(const_pvec2d_t point, const_pvec2d_t linePoint,
    const_pvec2d_t lineDirection, double *offset)
{
    vec2d_t delta;
    double len;

    V2d_Subtract(delta, lineDirection, linePoint);
    len = V2d_Length(delta);
    if (len == 0)
    {
        if (offset) *offset = 0;
        return 0;
    }

    if (offset)
    {
        *offset = ((linePoint[VY] - point[VY]) * (linePoint[VY]     - lineDirection[VY]) -
                   (linePoint[VX] - point[VX]) * (lineDirection[VX] - linePoint[VX])   ) / len;
    }

    return ((linePoint[VY] - point[VY]) * (lineDirection[VX] - linePoint[VX]) -
            (linePoint[VX] - point[VX]) * (lineDirection[VY] - linePoint[VY])) / len;
}

double V2d_PointLineParaDistance(const_pvec2d_t point, const_pvec2d_t lineDirection,
    double linePara, double lineLength)
{
    return (point[VX] * lineDirection[VX] + point[VY] * lineDirection[VY] + linePara) / lineLength;
}

double V2d_PointLinePerpDistance(const_pvec2d_t point, const_pvec2d_t lineDirection,
    double linePerp, double lineLength)
{
    return (point[VX] * lineDirection[VY] - point[VY] * lineDirection[VX] + linePerp) / lineLength;
}

double V2d_PointOnLineSide(const_pvec2d_t point, const_pvec2d_t lineOrigin, const_pvec2d_t lineDirection)
{
    return (lineOrigin[VY] - point[VY]) * lineDirection[VX] - (lineOrigin[VX] - point[VX]) * lineDirection[VY];
}

double V2d_PointOnLineSide2(const_pvec2d_t point, const_pvec2d_t lineDirection,
    double linePerp, double lineLength, double epsilon)
{
    double perp = V2d_PointLinePerpDistance(point, lineDirection, linePerp, lineLength);
    if (fabs(perp) <= epsilon) return 0;
    return perp;
}

double V2d_DotProduct(const_pvec2d_t a, const_pvec2d_t b)
{
    return a[VX] * b[VX] + a[VY] * b[VY];
}

double V2d_ScalarProject(const_pvec2d_t a, const_pvec2d_t b)
{
    double dot, len = V2d_Length(b);
    if (len == 0) return 0;

    dot = V2d_DotProduct(a, b);
    return dot / len;
}

double V2d_Project(pvec2d_t dest, const_pvec2d_t a, const_pvec2d_t b)
{
    double div = V2d_DotProduct(b, b);
    if (div == 0)
    {
        if (dest)
        {
            dest[VX] = dest[VY] = 0;
        }
        return 0;
    }

    if (dest)
    {
        V2d_Copy(dest, b);
        V2d_Scale(dest, V2d_DotProduct(a, b) / div);
    }

    return div;
}

double V2d_ProjectOnLine(pvec2d_t dest, const_pvec2d_t point,
    const_pvec2d_t lineOrigin, const_pvec2d_t lineDirection)
{
    double div = V2d_DotProduct(lineDirection, lineDirection);
    double pointVec[2];

    if (div == 0)
    {
        if (dest)
        {
            dest[VX] = dest[VY] = 0;
        }
        return 0;
    }

    V2d_Subtract(pointVec, point, lineOrigin);
    div = V2d_DotProduct(pointVec, lineDirection) / div;

    if (dest)
    {
        dest[VX] = lineOrigin[VX] + lineDirection[VX] * div;
        dest[VY] = lineOrigin[VY] + lineDirection[VY] * div;
    }

    return div;
}

dd_bool V2d_IsParallel(const_pvec2d_t a, const_pvec2d_t b)
{
#define EPSILON .99999999

    double aLen = V2d_Length(a);
    double bLen = V2d_Length(b);
    double dot;

    // Both must be non-zero vectors.
    if (aLen == 0 || bLen == 0) return true;

    dot = V2d_DotProduct(a, b) / aLen / bLen;

    // If it's close enough, we'll consider them parallel.
    return dot > EPSILON || dot < -EPSILON;

#undef EPSILON
}

dd_bool V2d_IsZero(const_pvec2d_t vec)
{
    return vec[VX] == 0 && vec[VY] == 0;
}

double V2d_Intersection(const_pvec2d_t linePointA, const_pvec2d_t lineDirectionA,
    const_pvec2d_t linePointB, const_pvec2d_t lineDirectionB, pvec2d_t point)
{
    /*
     *     (YA-YC)(XD-XC)-(XA-XC)(YD-YC)
     * r = -----------------------------
     *     (XB-XA)(YD-YC)-(YB-YA)(XD-XC)
     */

    double r, div;

    div = lineDirectionA[VX] * lineDirectionB[VY] - lineDirectionA[VY] * lineDirectionB[VX];

    if (div == 0)
    {
        // Special case: lines are parallel.
        r = 0;
    }
    else
    {
        r = ((linePointA[VY] - linePointB[VY]) * lineDirectionB[VX] -
             (linePointA[VX] - linePointB[VX]) * lineDirectionB[VY]) / div;
    }

    /*
     * XI = XA + r(XB-XA)
     * YI = YA + r(YB-YA)
     */

    if (point)
    {
        // Calculate the intersection point.
        point[VX] = linePointA[VX] + r * lineDirectionA[VX];
        point[VY] = linePointA[VY] + r * lineDirectionA[VY];
    }

    // Return the scaling factor.
    return r;
}

double V2d_Intercept(const_pvec2d_t a, const_pvec2d_t b, const_pvec2d_t c,
    const_pvec2d_t d, pvec2d_t point)
{
    vec2d_t ab, cd;

    ab[0] = b[VX] - a[VX];
    ab[1] = b[VY] - a[VY];
    cd[0] = d[VX] - c[VX];
    cd[1] = d[VY] - c[VY];

    return V2d_Intersection(a, ab, c, cd, point);
}

dd_bool V2d_Intercept2(const_pvec2d_t a, const_pvec2d_t b, const_pvec2d_t c,
    const_pvec2d_t d, pvec2d_t point, double* abFrac, double* cdFrac)
{
    double ab, cd;

    ab = V2d_Intercept(a, b, c, d, point);
    cd = V2d_Intercept(c, d, a, b, NULL);

    if (abFrac) *abFrac = ab;
    if (cdFrac) *cdFrac = cd;

    return (ab >= 0 && ab <= 1 && cd >= 0 && cd <= 1);
}

void V2d_Lerp(pvec2d_t dest, const_pvec2d_t a, const_pvec2d_t b, double c)
{
    uint i;
    for (i = 0; i < 2; ++i)
    {
        dest[i] = a[i] + c * (b[i] - a[i]);
    }
}

void V2d_InitBox(arvec2d_t box, const_pvec2d_t point)
{
    V2d_Copy(box[0], point);
    V2d_Copy(box[1], point);
}

void V2d_InitBoxXY(arvec2d_t box, double x, double y)
{
    vec2d_t point; V2d_Set(point, x, y);
    V2d_InitBox(box, point);
}

void V2d_AddToBox(arvec2d_t box, const_pvec2d_t point)
{
    if (point[VX] < box[0][VX])
        box[0][VX] = point[VX];
    if (point[VX] > box[1][VX])
        box[1][VX] = point[VX];

    if (point[VY] < box[0][VY])
        box[0][VY] = point[VY];
    if (point[VY] > box[1][VY])
        box[1][VY] = point[VY];
}

void V2d_AddToBoxXY(arvec2d_t box, double x, double y)
{
    vec2d_t point; V2d_Set(point, x, y);
    V2d_AddToBox(box, point);
}

void V2d_UniteBox(arvec2d_t box, const_arvec2d_t other)
{
    if (other[0][VX] < box[0][VX])
        box[0][VX] = other[0][VX];

    if (other[1][VX] > box[1][VX])
        box[1][VX] = other[1][VX];

    if (other[0][VY] < box[0][VY])
        box[0][VY] = other[0][VY];

    if (other[1][VY] > box[1][VY])
        box[1][VY] = other[1][VY];
}

void V2d_CopyBox(arvec2d_t dest, const_arvec2d_t src)
{
    V2d_Copy(dest[0], src[0]);
    V2d_Copy(dest[1], src[1]);
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

float V3f_Length(const_pvec3f_t vec)
{
    if (vec[VX] == 0 && vec[VY] == 0 && vec[VZ] == 0) return 0;
    return (float) sqrt(vec[VX] * vec[VX] + vec[VY] * vec[VY] + vec[VZ] * vec[VZ]);
}

float V3f_Distance(const_pvec3f_t a, const_pvec3f_t b)
{
    vec3f_t vec;
    V3f_Subtract(vec, b, a);
    return V3f_Length(vec);
}

float V3f_Normalize(pvec3f_t vec)
{
    float len = V3f_Length(vec);
    if (len)
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

void V3f_Copyd(pvec3f_t dest, const_pvec3d_t src)
{
    V3f_Set(dest, (float) src[VX], (float) src[VY], (float) src[VZ]);
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

void V3f_CrossProduct(pvec3f_t dest, const_pvec3f_t a, const_pvec3f_t b)
{
    dest[VX] = a[VY] * b[VZ] - a[VZ] * b[VY];
    dest[VY] = a[VZ] * b[VX] - a[VX] * b[VZ];
    dest[VZ] = a[VX] * b[VY] - a[VY] * b[VX];
}

void V3f_CrossProductd(pvec3f_t dest, const_pvec3d_t src1d, const_pvec3d_t src2d)
{
    vec3f_t src1, src2;
    V3f_Copyd(src1, src1d);
    V3f_Copyd(src2, src2d);
    V3f_CrossProduct(dest, src1, src2);
}

void V3f_PointCrossProduct(pvec3f_t dest, const_pvec3f_t v1, const_pvec3f_t v2,
    const_pvec3f_t v3)
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

int V3f_MajorAxis(const_pvec3f_t vec)
{
    vec3f_t fn;
    int axis;

    V3f_Set(fn, fabsf(vec[VX]), fabsf(vec[VY]), fabsf(vec[VZ]));

    axis = VX;
    if (fn[VY] > fn[axis])
        axis = VY;
    if (fn[VZ] > fn[axis])
        axis = VZ;

    return axis;
}

dd_bool V3f_IsZero(const_pvec3f_t vec)
{
    return vec[VX] == 0 && vec[VY] == 0 && vec[VZ] == 0;
}

void V3f_Lerp(pvec3f_t dest, const_pvec3f_t a, const_pvec3f_t b, float c)
{
    uint i;
    for (i = 0; i < 3; ++i)
    {
        dest[i] = a[i] + c * (b[i] - a[i]);
    }
}

void V3f_BuildTangents(pvec3f_t tangent, pvec3f_t bitangent, const_pvec3f_t normal)
{
    vec3f_t const rotm[3] = {
        { 0.f, 0.f, 1.f },
        { 0.f, 0.f, 1.f },
        { 0.f, 0.f, 1.f }
    };
    int axis = VX;
    vec3f_t fn;

    V3f_Set(fn, fabsf(normal[VX]), fabsf(normal[VY]), fabsf(normal[VZ]));

    if (fn[VY] > fn[axis])
        axis = VY;
    if (fn[VZ] > fn[axis])
        axis = VZ;

    if (fabsf(fn[VX] - 1.0f) < FLT_EPSILON ||
       fabsf(fn[VY] - 1.0f) < FLT_EPSILON ||
       fabsf(fn[VZ] - 1.0f) < FLT_EPSILON)
    {
        // We must build the tangent vector manually.
        if (axis == VX && normal[VX] > 0.f)
        {
            V3f_Set(tangent, 0.f, 1.f, 0.f);
        }
        else if (axis == VX)
        {
            V3f_Set(tangent, 0.f, -1.f, 0.f);
        }

        if (axis == VY && normal[VY] > 0.f)
        {
            V3f_Set(tangent, -1.f, 0.f, 0.f);
        }
        else if (axis == VY)
        {
            V3f_Set(tangent, 1.f, 0.f, 0.f);
        }

        if (axis == VZ)
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

void V3d_Set(pvec3d_t vec, vectorcompd_t x, vectorcompd_t y, vectorcompd_t z)
{
    vec[VX] = x;
    vec[VY] = y;
    vec[VZ] = z;
}

void V3d_SetFixed(pvec3d_t vec, fixed_t x, fixed_t y, fixed_t z)
{
    vec[VX] = FIX2FLT(x);
    vec[VY] = FIX2FLT(y);
    vec[VZ] = FIX2FLT(z);
}

double V3d_Length(const_pvec3d_t vec)
{
    if (vec[VX] == 0 && vec[VY] == 0 && vec[VZ] == 0) return 0;
    return sqrt(vec[VX] * vec[VX] + vec[VY] * vec[VY] + vec[VZ] * vec[VZ]);
}

double V3d_Distance(const_pvec3d_t a, const_pvec3d_t b)
{
    vec3d_t vec;
    V3d_Subtract(vec, b, a);
    return V3d_Length(vec);
}

double V3d_Normalize(pvec3d_t vec)
{
    double len = V3d_Length(vec);
    if (len != 0)
    {
        vec[VX] /= len;
        vec[VY] /= len;
        vec[VZ] /= len;
    }
    return len;
}

void V3d_Copy(pvec3d_t dest, const_pvec3d_t src)
{
    dest[VX] = src[VX];
    dest[VY] = src[VY];
    dest[VZ] = src[VZ];
}

void V3d_Copyf(pvec3d_t dest, const_pvec3f_t srcf)
{
    V3d_Set(dest, srcf[VX], srcf[VY], srcf[VZ]);
}

void V3d_Scale(pvec3d_t vec, double scalar)
{
    vec[VX] *= scalar;
    vec[VY] *= scalar;
    vec[VZ] *= scalar;
}

void V3d_Sum(pvec3d_t dest, const_pvec3d_t src1, const_pvec3d_t src2)
{
    dest[VX] = src1[VX] + src2[VX];
    dest[VY] = src1[VY] + src2[VY];
    dest[VZ] = src1[VZ] + src2[VZ];
}

void V3d_Subtract(pvec3d_t dest, const_pvec3d_t src1, const_pvec3d_t src2)
{
    dest[VX] = src1[VX] - src2[VX];
    dest[VY] = src1[VY] - src2[VY];
    dest[VZ] = src1[VZ] - src2[VZ];
}

double V3d_DotProduct(const_pvec3d_t a, const_pvec3d_t b)
{
    return a[VX] * b[VX] + a[VY] * b[VY] + a[VZ] * b[VZ];
}

double V3d_DotProductf(const_pvec3d_t a, const_pvec3f_t bf)
{
    vec3d_t b;
    V3d_Copyf(b, bf);
    return V3d_DotProduct(a, b);
}

void V3d_CrossProduct(pvec3d_t dest, const_pvec3d_t src1, const_pvec3d_t src2)
{
    dest[VX] = src1[VY] * src2[VZ] - src1[VZ] * src2[VY];
    dest[VY] = src1[VZ] * src2[VX] - src1[VX] * src2[VZ];
    dest[VZ] = src1[VX] * src2[VY] - src1[VY] * src2[VX];
}

void V3d_PointCrossProduct(pvec3d_t dest, const_pvec3d_t v1, const_pvec3d_t v2,
    const_pvec3d_t v3)
{
    vec3d_t a, b;
    V3d_Subtract(a, v2, v1);
    V3d_Subtract(b, v3, v1);
    V3d_CrossProduct(dest, a, b);
}

double V3d_ClosestPointOnPlane(pvec3d_t dest, const_pvec3d_t planeNormal,
    const_pvec3d_t planePoint, const_pvec3d_t arbPoint)
{
    vec3d_t pvec;
    double distance;

    V3d_Subtract(pvec, arbPoint, planePoint);
    distance = V3d_DotProduct(planeNormal, pvec);

    V3d_Copy(dest, planeNormal);
    V3d_Scale(dest, distance);
    V3d_Subtract(dest, arbPoint, dest);

    return distance;
}

double V3d_ClosestPointOnPlanef(pvec3d_t dest, const_pvec3f_t planeNormalf,
    const_pvec3d_t planePoint, const_pvec3d_t arbPoint)
{
    vec3d_t planeNormal;
    V3d_Copyf(planeNormal, planeNormalf);
    return V3d_ClosestPointOnPlane(dest, planeNormal, planePoint, arbPoint);
}

int V3d_MajorAxis(const_pvec3d_t vec)
{
    vec3d_t fn;
    int axis;

    V3d_Set(fn, fabs(vec[VX]), fabs(vec[VY]), fabs(vec[VZ]));

    axis = VX;
    if (fn[VY] > fn[axis])
        axis = VY;
    if (fn[VZ] > fn[axis])
        axis = VZ;

    return axis;
}

dd_bool V3d_IsZero(const_pvec3d_t vec)
{
    return vec[VX] == 0 && vec[VY] == 0 && vec[VZ] == 0;
}

void V3d_Lerp(pvec3d_t dest, const_pvec3d_t a, const_pvec3d_t b, double c)
{
    uint i;
    for (i = 0; i < 3; ++i)
    {
        dest[i] = a[i] + c * (b[i] - a[i]);
    }
}

void V3d_BuildTangents(pvec3d_t tangent, pvec3d_t bitangent, const_pvec3d_t normal)
{
    vec3d_t const rotm[3] = {
        { 0.f, 0.f, 1.f },
        { 0.f, 0.f, 1.f },
        { 0.f, 0.f, 1.f }
    };
    int axis = VX;
    vec3d_t fn;

    V3d_Set(fn, fabs(normal[VX]), fabs(normal[VY]), fabs(normal[VZ]));

    if (fn[VY] > fn[axis])
        axis = VY;
    if (fn[VZ] > fn[axis])
        axis = VZ;

    if (fabs(fn[VX] - 1.0) < FLT_EPSILON ||
       fabs(fn[VY] - 1.0) < FLT_EPSILON ||
       fabs(fn[VZ] - 1.0) < FLT_EPSILON)
    {
        // We must build the tangent vector manually.
        if (axis == VX && normal[VX] > 0.f)
        {
            V3d_Set(tangent, 0.f, 1.f, 0.f);
        }
        else if (axis == VX)
        {
            V3d_Set(tangent, 0.f, -1.f, 0.f);
        }

        if (axis == VY && normal[VY] > 0.f)
        {
            V3d_Set(tangent, -1.f, 0.f, 0.f);
        }
        else if (axis == VY)
        {
            V3d_Set(tangent, 1.f, 0.f, 0.f);
        }

        if (axis == VZ)
        {
            V3d_Set(tangent, 1.f, 0.f, 0.f);
        }
    }
    else
    {
        // Can use a cross product of the normal.
        V3d_CrossProduct(tangent, (pvec3d_t)rotm[axis], normal);
        V3d_Normalize(tangent);
    }

    V3d_CrossProduct(bitangent, tangent, normal);
    V3d_Normalize(bitangent);
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

float V4f_Length(const_pvec4f_t vec)
{
    if (vec[0] == 0 && vec[1] == 0 && vec[2] == 0 && vec[3] == 0) return 0;
    return (float) sqrt(vec[0] * vec[0] + vec[1] * vec[1] +
                        vec[2] * vec[2] + vec[3] * vec[3]);
}

float V4f_Distance(const_pvec4f_t a, const_pvec4f_t b)
{
    vec4f_t vec;
    V4f_Subtract(vec, b, a);
    return V4f_Length(vec);
}

float V4f_Normalize(pvec4f_t vec)
{
    float len = V4f_Length(vec);
    if (len != 0)
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

dd_bool V4f_IsZero(const_pvec4f_t vec)
{
    return vec[0] == 0 && vec[1] == 0 && vec[2] == 0 && vec[3] == 0;
}

void V4f_Lerp(pvec4f_t dest, const_pvec4f_t a, const_pvec4f_t b, float c)
{
    uint i;
    for (i = 0; i < 4; ++i)
    {
        dest[i] = a[i] + c * (b[i] - a[i]);
    }
}

void V4d_Set(pvec4d_t vec, vectorcompd_t x, vectorcompd_t y, vectorcompd_t z, vectorcompd_t w)
{
    vec[0] = x;
    vec[1] = y;
    vec[2] = z;
    vec[3] = w;
}

void V4d_SetFixed(pvec4d_t vec, fixed_t x, fixed_t y, fixed_t z, fixed_t w)
{
    vec[0] = FIX2FLT(x);
    vec[1] = FIX2FLT(y);
    vec[2] = FIX2FLT(z);
    vec[3] = FIX2FLT(w);
}

double V4d_Length(const_pvec4d_t vec)
{
    if (vec[0] == 0 && vec[1] == 0 && vec[2] == 0 && vec[3] == 0) return 0;
    return sqrt(vec[0] * vec[0] + vec[1] * vec[1] +
                vec[2] * vec[2] + vec[3] * vec[3]);
}

double V4d_Distance(const_pvec4d_t a, const_pvec4d_t b)
{
    vec4d_t vec;
    V4d_Subtract(vec, b, a);
    return V4d_Length(vec);
}

double V4d_Normalize(pvec4d_t vec)
{
    double len = V4d_Length(vec);
    if (len != 0)
    {
        vec[0] /= len;
        vec[1] /= len;
        vec[2] /= len;
        vec[3] /= len;
    }
    return len;
}

void V4d_Copy(pvec4d_t dest, const_pvec4d_t src)
{
    dest[0] = src[0];
    dest[1] = src[1];
    dest[2] = src[2];
    dest[3] = src[3];
}

void V4d_Scale(pvec4d_t vec, double scalar)
{
    vec[0] *= scalar;
    vec[1] *= scalar;
    vec[2] *= scalar;
    vec[3] *= scalar;
}

void V4d_Sum(pvec4d_t dest, const_pvec4d_t src1, const_pvec4d_t src2)
{
    dest[0] = src1[0] + src2[0];
    dest[1] = src1[1] + src2[1];
    dest[2] = src1[2] + src2[2];
    dest[3] = src1[3] + src2[3];
}

void V4d_Subtract(pvec4d_t dest, const_pvec4d_t src1, const_pvec4d_t src2)
{
    dest[0] = src1[0] - src2[0];
    dest[1] = src1[1] - src2[1];
    dest[2] = src1[2] - src2[2];
    dest[3] = src1[3] - src2[3];
}

dd_bool V4d_IsZero(const_pvec4d_t vec)
{
    return vec[0] == 0 && vec[1] == 0 && vec[2] == 0 && vec[3] == 0;
}

void V4d_Lerp(pvec4d_t dest, const_pvec4d_t a, const_pvec4d_t b, double c)
{
    uint i;
    for (i = 0; i < 4; ++i)
    {
        dest[i] = a[i] + c * (b[i] - a[i]);
    }
}
