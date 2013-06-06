/** @file matrix.h  Matrix templates.
 *
 * @authors Copyright © 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDENG2_MATRIX_H
#define LIBDENG2_MATRIX_H

#include "../libdeng2.h"
#include "../math.h"
#include "../Vector"
#include "../Writer"
#include "../Reader"
#include "../String"
#include "../ByteRefArray"

#include <QTextStream>

namespace de {

// Utilities.
DENG2_PUBLIC dfloat  Matrix3_Determinant(dfloat  const *values9);
DENG2_PUBLIC ddouble Matrix3_Determinant(ddouble const *values9);
DENG2_PUBLIC bool    Matrix3_Inverse(dfloat  *out9,  dfloat const  *in9);
DENG2_PUBLIC bool    Matrix3_Inverse(ddouble *out9,  ddouble const *in9);
DENG2_PUBLIC bool    Matrix4_Inverse(dfloat  *out16, dfloat const  *in16);
DENG2_PUBLIC bool    Matrix4_Inverse(ddouble *out16, ddouble const *in16);

/**
 * 3x3 matrix. @ingroup math
 */
template <typename Type>
class Matrix3
{
public:
    typedef Vector2<Type> Vec2;
    typedef Vector3<Type> Vec3;
    typedef Vector4<Type> Vec4;

    enum SpecialMatrix {
        Zero,               ///< All elements are zero.
        Uninitialized
    };

public:
    /// Construct an identity matrix.
    Matrix3() {
        data().clear();
        at(0, 0) = at(1, 1) = at(2, 2) = Type(1);
    }
    Matrix3(SpecialMatrix specialType) {
        switch(specialType) {
        case Zero:
            data().clear();
            break;
        default:
            break;
        }
    }
    Matrix3(Type const *values9) {
        data().set(0, reinterpret_cast<IByteArray::Byte const *>(values9), sizeof(_values));
    }
    Matrix3(ByteRefArray const &otherData) {
        DENG2_ASSERT(otherData.size() == sizeof(_values));
        otherData.get(0, _values, sizeof(_values));
    }

    // Accessors.
    inline Type &at(int row, int col) {
        DENG2_ASSERT(row >= 0 && row < 3);
        DENG2_ASSERT(col >= 0 && col < 3);
        return _values[col*3 + row];
    }
    inline Type at(int row, int col) const {
        DENG2_ASSERT(row >= 0 && row < 3);
        DENG2_ASSERT(col >= 0 && col < 3);
        return _values[col*3 + row];
    }
    Vec3 row(int row) const {
        return Vec3(at(row, 0), at(row, 1), at(row, 2));
    }
    Vec3 column(int col) const {
        return Vec3(at(0, col), at(1, col), at(2, col));
    }
    inline Type &operator [] (int index) {
        DENG2_ASSERT(index >= 0 && index < 9);
        return _values[index];
    }
    inline Type operator [] (int index) const {
        DENG2_ASSERT(index >= 0 && index < 9);
        return _values[index];
    }
    ByteRefArray const data() const {
        return ByteRefArray(_values, sizeof(_values));
    }
    ByteRefArray data() {
        return ByteRefArray(_values, sizeof(_values));
    }
    Type const *values() const {
        return _values;
    }
    Type *values() {
        return _values;
    }

    // Math operations.
    Matrix3 operator * (Matrix3 const &right) const {
        Matrix3 result(Zero);
        for(int i = 0; i < 3; ++i)
            for(int j = 0; j < 3; ++j)
                for(int k = 0; k < 3; ++k)
                    result.at(i, j) += at(i, k) * right.at(k, j);
        return result;
    }
    Vec3 operator * (Vec3 const &vector) const {
        Vec3 result;
        for(int i = 0; i < 3; ++i)
            for(int j = 0; j < 3; ++j)
                result[i] += at(i, j) * vector[j];
        return result;
    }
    Vec4 operator * (Vec4 const &vector) const {
        return Vec4::fromEuclidean(*this * vector.toEuclidean());
    }
    Matrix3 inverse() const {
        Matrix3 result(Uninitialized);
        Matrix3_Inverse(result._values, _values);
        return result;
    }
    Matrix3 transpose() const {
        Matrix3 m(Uninitialized);
        for(int i = 0; i < 3; ++i)
            for(int j = 0; j < 3; ++j)
                m.at(i, j) = at(j, i);
        return m;
    }
    String asText() const {
        String str;
        QTextStream s(&str);
        s << "Matrix3:\n"
          << "  " << row(0) << "\n"
          << "  " << row(1) << "\n"
          << "  " << row(2) << "\n";
        return str;
    }

public:
    // Specialized constructors.
    inline static Matrix3 zero() { return m(Zero); }

private:
    Type _values[9];
};

// Serialization of Matrix3.
template <typename Type>
inline Writer &operator << (Writer &to, Matrix3<Type> const &mat3) {
    for(int i = 0; i < 9; ++i) to << mat3[i];
    return to;
}

template <typename Type>
inline void operator << (Writer const &to, Matrix3<Type> const &mat3) {
    Writer w(to);
    for(int i = 0; i < 9; ++i) w << mat3[i];
}

template <typename Type>
inline Reader &operator >> (Reader &from, Matrix3<Type> &mat3) {
    for(int i = 0; i < 9; ++i) from >> mat3[i];
    return from;
}

template <typename Type>
inline void operator >> (Reader const &from, Matrix3<Type> &mat3) {
    Reader r(from);
    for(int i = 0; i < 9; ++i) r >> mat3[i];
}

template <typename Type>
inline QTextStream &operator << (QTextStream &os, Matrix3<Type> const &mat3) {
    os << mat3.asText();
    return os;
}

/**
 * 4x4 matrix. @ingroup math
 */
template <typename Type>
class Matrix4
{
public:
    typedef Vector2<Type> Vec2;
    typedef Vector3<Type> Vec3;
    typedef Vector4<Type> Vec4;

    enum SpecialMatrix {
        Zero,               ///< All elements are zero.
        Uninitialized
    };

public:
    /// Construct an identity matrix.
    Matrix4() {
        data().clear();
        at(0, 0) = at(1, 1) = at(2, 2) = at(3, 3) = Type(1);
    }
    Matrix4(SpecialMatrix specialType) {
        switch(specialType) {
        case Zero:
            data().clear();
            break;
        default:
            break;
        }
    }
    Matrix4(Type const *values16) {
        data().set(0, reinterpret_cast<IByteArray::Byte const *>(values16), sizeof(_values));
    }
    Matrix4(ByteRefArray const &otherData) {
        DENG2_ASSERT(otherData.size() == sizeof(_values));
        otherData.get(0, _values, sizeof(_values));
    }

    // Accessors.
    inline Type &at(int row, int col) {
        DENG2_ASSERT(row >= 0 && row < 4);
        DENG2_ASSERT(col >= 0 && col < 4);
        return _values[col*4 + row];
    }
    inline Type at(int row, int col) const {
        DENG2_ASSERT(row >= 0 && row < 4);
        DENG2_ASSERT(col >= 0 && col < 4);
        return _values[col*4 + row];
    }
    Vec4 row(int row) const {
        return Vec4(at(row, 0), at(row, 1), at(row, 2), at(row, 3));
    }
    Vec4 column(int col) const {
        return Vec4(at(0, col), at(1, col), at(2, col), at(3, col));
    }
    inline Type &operator [] (int index) {
        DENG2_ASSERT(index >= 0 && index < 16);
        return _values[index];
    }
    inline Type operator [] (int index) const {
        DENG2_ASSERT(index >= 0 && index < 16);
        return _values[index];
    }
    ByteRefArray const data() const {
        return ByteRefArray(_values, sizeof(_values));
    }
    ByteRefArray data() {
        return ByteRefArray(_values, sizeof(_values));
    }
    Type const *values() const {
        return _values;
    }
    Type *values() {
        return _values;
    }

    // Math operations.
    Matrix4 operator * (Matrix4 const &right) const {
        Matrix4 result(Zero);
        for(int i = 0; i < 4; ++i)
            for(int j = 0; j < 4; ++j)
                for(int k = 0; k < 4; ++k)
                    result.at(i, j) += at(i, k) * right.at(k, j);
        return result;
    }
    Vec3 operator * (Vec3 const &vector) const {
        return (*this * Vec4::fromEuclidean(vector)).toEuclidean();
    }
    Vec4 operator * (Vec4 const &vector) const {
        Vec4 result;
        for(int i = 0; i < 4; ++i)
            for(int j = 0; j < 4; ++j)
                result[i] += at(i, j) * vector[j];
        return result;
    }
    Matrix4 inverse() const {
        Matrix4 result(Uninitialized);
        Matrix4_Inverse(result._values, _values);
        return result;
    }
    Matrix4 transpose() const {
        Matrix4 m(Uninitialized);
        for(int i = 0; i < 4; ++i)
            for(int j = 0; j < 4; ++j)
                m.at(i, j) = at(j, i);
        return m;
    }
    String asText() const {
        String str;
        QTextStream s(&str);
        s << "Matrix4:\n"
          << "  " << row(0) << "\n"
          << "  " << row(1) << "\n"
          << "  " << row(2) << "\n"
          << "  " << row(3) << "\n";
        return str;
    }

public:
    // Specialized constructors.
    inline static Matrix4 zero() { return m(Zero); }
    static Matrix4 ortho(Type left, Type right, Type top, Type bottom,
                         Type nearDistance = -1.f, Type farDistance = 1.f) {
        Matrix4 m;
        m.at(0, 0) =  Type(2) / (right - left);
        m.at(1, 1) =  Type(2) / (top - bottom);
        m.at(2, 2) = -Type(2) / (farDistance - nearDistance);
        m[12]      = -(right + left) / (right - left);
        m[13]      = -(top + bottom) / (top - bottom);
        m[14]      = -(farDistance + nearDistance)   / (farDistance - nearDistance);
        return m;
    }
    static Matrix4 perspective(Type fov, Type aspectRatio, Type nearDistance = 1.f, Type farDistance = 1000.f) {
        Type const halfWidth  = std::tan(Type(.5) * degreeToRadian(fov));
        Type const halfHeight = halfWidth / aspectRatio;
        Type const depth      = farDistance - nearDistance;
        Matrix4 m(Zero);
        m.at(0, 0) = Type(1) / halfWidth;
        m.at(1, 1) = Type(1) / halfHeight;
        m.at(2, 2) = -(farDistance + nearDistance) / depth;
        m.at(2, 3) = -Type(1);
        m.at(3, 2) = -Type(2) * farDistance * nearDistance / depth;
        return m;
    }
    static Matrix4 perspectiveZoom(Type width, Type height, Type near = 1.f, Type far = 1000.f, Type zoom = 1.f) {
        Type const zoomHalf =  zoom / 2;
        Type const aspect   =  width / height;
        Type const left     = -zoomHalf;
        Type const right    =  zoomHalf;
        Type const bottom   = -zoomHalf / aspect;
        Type const top      =  zoomHalf / aspect;
        Type m[16] = {
            2 * near / (right - left), 0, 0, 0,
            0, 2 * near / (top - bottom), 0, 0,
            (right + left) / (right - left), (top + bottom) / (top - bottom), -(far + near) / (far - near), -1,
            0, 0, -2 * (far * near) / (far - near), 0
        };
        return m;
    }
    static Matrix4 rotate(Type angleDegrees, Vec3 const &unitAxis = Vec3(0, 0, 1)) {
        Type const ang = degreeToRadian(angleDegrees);
        Type const c   = std::cos(ang);
        Type const s   = std::sin(ang);
        Type m[16] = {
            unitAxis.x*unitAxis.x*(1-c)+c,            unitAxis.x*unitAxis.y*(1-c)+unitAxis.z*s, unitAxis.x*unitAxis.z*(1-c)-unitAxis.y*s, 0,
            unitAxis.x*unitAxis.y*(1-c)-unitAxis.z*s, unitAxis.y*unitAxis.y*(1-c)+c,            unitAxis.y*unitAxis.z*(1-c)+unitAxis.x*s, 0,
            unitAxis.x*unitAxis.z*(1-c)+unitAxis.y*s, unitAxis.z*unitAxis.y*(1-c)-unitAxis.x*s, unitAxis.z*unitAxis.z*(1-c)+c,            0,
            0, 0, 0, 1
        };
        return m;
    }
    static Matrix4 rotateAround(Vec3 const &pivot, Type angleDegrees, Vec3 const &axis = Vec3(0, 0, 1)) {
        return translate(pivot) * rotate(angleDegrees, axis) * translate(-pivot);
    }
    static Matrix4 translate(Vec3 const &translation) {
        return scaleThenTranslate(Vec3(1, 1, 1), translation);
    }
    static Matrix4 scale(Type scalar) {
        return scale(Vec3(scalar, scalar, scalar));
    }
    static Matrix4 scale(Vec2 const &scalar) {
        return scale(Vec3(scalar, Type(1)));
    }
    static Matrix4 scale(Vec3 const &scalar) {
        return scaleThenTranslate(scalar, Vec3(0, 0, 0));
    }
    static Matrix4 scaleThenTranslate(Type scalar, Vec3 const &translation) {
        return scaleThenTranslate(Vec3(scalar, scalar, scalar), translation);
    }
    static Matrix4 scaleThenTranslate(Vec2 const &scalar, Vec3 const &translation) {
        return scaleThenTranslate(Vec3(scalar, Type(1)), translation);
    }
    static Matrix4 scaleThenTranslate(Vec3 const &scalar, Vec3 const &translation) {
        Matrix4 m(Zero);
        m[0]  = scalar.x;
        m[5]  = scalar.y;
        m[10] = scalar.z;
        m[12] = translation.x;
        m[13] = translation.y;
        m[14] = translation.z;
        m[15] = 1;
        return m;
    }
    static Matrix4 lookAt(Vec3 const &target, Vec3 const &eyePos, Vec3 const &up) {
        Matrix4 m(Zero);
        Vec3 f = (target - eyePos).normalize();
        Vec3 s = f.cross(up.normalize());
        Vec3 u = s.cross(f);
        m[0]  =  s.x;
        m[1]  =  u.x;
        m[2]  = -f.x;
        m[4]  =  s.y;
        m[5]  =  u.y;
        m[6]  = -f.y;
        m[8]  =  s.z;
        m[9]  =  u.z;
        m[10] = -f.z;
        m[15] = Type(1);
        return m * translate(-eyePos);
    }

private:
    Type _values[16];
};

// Serialization of Matrix4.
template <typename Type>
inline Writer &operator << (Writer &to, Matrix4<Type> const &mat4) {
    for(int i = 0; i < 16; ++i) to << mat4[i];
    return to;
}

template <typename Type>
inline void operator << (Writer const &to, Matrix4<Type> const &mat4) {
    Writer w(to);
    for(int i = 0; i < 16; ++i) w << mat4[i];
}

template <typename Type>
inline Reader &operator >> (Reader &from, Matrix4<Type> &mat4) {
    for(int i = 0; i < 16; ++i) from >> mat4[i];
    return from;
}

template <typename Type>
inline void operator >> (Reader const &from, Matrix4<Type> &mat4) {
    Reader r(from);
    for(int i = 0; i < 16; ++i) r >> mat4[i];
}

template <typename Type>
inline QTextStream &operator << (QTextStream &os, Matrix4<Type> const &mat4) {
    os << mat4.asText();
    return os;
}

///@{
/// @ingroup types
typedef Matrix3<dfloat>  Matrix3f;
typedef Matrix3<ddouble> Matrix3d;
typedef Matrix4<dfloat>  Matrix4f;
typedef Matrix4<ddouble> Matrix4d;
///@}

} // namespace de

#endif // LIBDENG2_MATRIX_H
