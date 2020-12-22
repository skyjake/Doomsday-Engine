/** @file matrix.h  Matrix templates.
 *
 * @authors Copyright © 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * LGPL: http://www.gnu.org/licenses/lgpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser
 * General Public License for more details. You should have received a copy of
 * the GNU Lesser General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#include "de/matrix.h"

namespace de {

template <typename Type>
Type Matrix3_DeterminantT(const Type *mat)
{
    return (mat[0] * ( mat[4] * mat[8] - mat[7] * mat[5] ) -
            mat[1] * ( mat[3] * mat[8] - mat[6] * mat[5] ) +
            mat[2] * ( mat[3] * mat[7] - mat[6] * mat[4] ));
}

dfloat Matrix3_Determinant(const dfloat *values9)
{
    return Matrix3_DeterminantT(values9);
}

ddouble Matrix3_Determinant(const ddouble *values9)
{
    return Matrix3_DeterminantT(values9);
}

template <typename Type>
bool Matrix3_InverseT(Type *result, const Type *mat)
{
    Type det = Matrix3_DeterminantT(mat);
    if (de::abs(det) < .0005f)
    {
        Matrix3<Type> identity;
        identity.data().get(0, reinterpret_cast<IByteArray::Byte *>(result),
                            identity.data().size());
        return false;
    }

    result[0] =    mat[4] * mat[8] - mat[5] * mat[7]   / det;
    result[1] = -( mat[1] * mat[8] - mat[7] * mat[2] ) / det;
    result[2] =    mat[1] * mat[5] - mat[4] * mat[2]   / det;
    result[3] = -( mat[3] * mat[8] - mat[5] * mat[6] ) / det;
    result[4] =    mat[0] * mat[8] - mat[6] * mat[2]   / det;
    result[5] = -( mat[0] * mat[5] - mat[3] * mat[2] ) / det;
    result[6] =    mat[3] * mat[7] - mat[6] * mat[4]   / det;
    result[7] = -( mat[0] * mat[7] - mat[6] * mat[1] ) / det;
    result[8] =    mat[0] * mat[4] - mat[1] * mat[3]   / det;

    return true;
}

bool Matrix3_Inverse(dfloat *out9, const dfloat *in9)
{
    return Matrix3_InverseT(out9, in9);
}

bool Matrix3_Inverse(ddouble *out9, const ddouble *in9)
{
    return Matrix3_InverseT(out9, in9);
}

template <typename Type>
void Matrix4_SubmatrixT(const Type *mat4, Type *mat3, int i, int j)
{
    // Loop through 3x3 submatrix.
    for (int di = 0; di < 3; di++)
    {
        for (int dj = 0; dj < 3; dj++)
        {
            // Map 3x3 element (destination) to 4x4 element (source).
            int si = di + (di >= i? 1 : 0);
            int sj = dj + (dj >= j? 1 : 0);

            // Copy element.
            mat3[di * 3 + dj] = mat4[si * 4 + sj];
        }
    }
}

template <typename Type>
Type Matrix4_DeterminantT(const Type *mat)
{
    Type result = 0;
    Type i = 1;
    for (int n = 0; n < 4; n++, i *= -1)
    {
        Type sub[3*3];
        Matrix4_SubmatrixT(mat, sub, 0, n);
        result += mat[n] * Matrix3_DeterminantT(sub) * i;
    }
    return result;
}

template <typename Type>
bool Matrix4_InverseT(Type *out16, const Type *in16)
{
    Type det = Matrix4_DeterminantT(in16);

    if (de::abs(det) < .000001f) // .0005f)
    {
        Matrix4<Type> identity;
        identity.data().get(0, reinterpret_cast<IByteArray::Byte *>(out16),
                            identity.data().size());
        return false;
    }

    for (int i = 0; i < 4; i++)
    {
        for (int j = 0; j < 4; j++)
        {
            Type sub[3*3];
            int sign = 1 - ((i + j) % 2) * 2;
            Matrix4_SubmatrixT(in16, sub, i, j);
            out16[i + j*4] = (Matrix3_DeterminantT(sub) * sign) / det;
        }
    }

    return true;
}

bool Matrix4_Inverse(dfloat *out16, const dfloat *in16)
{
    return Matrix4_InverseT(out16, in16);
}

bool Matrix4_Inverse(ddouble *out16, const ddouble *in16)
{
    return Matrix4_InverseT(out16, in16);
}

} // namespace de
