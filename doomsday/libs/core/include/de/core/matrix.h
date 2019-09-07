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

#ifndef LIBCORE_MATRIX_H
#define LIBCORE_MATRIX_H

#include "../libcore.h"
#include "../math.h"
#include "../Vector"
#include "../Writer"
#include "../Reader"
#include "../String"
#include "../ByteRefArray"

#include <sstream>

namespace de {

// Utilities.
DE_PUBLIC dfloat  Matrix3_Determinant(const dfloat *values9);
DE_PUBLIC ddouble Matrix3_Determinant(const ddouble *values9);
DE_PUBLIC bool    Matrix3_Inverse(dfloat  *out9,  const dfloat *in9);
DE_PUBLIC bool    Matrix3_Inverse(ddouble *out9,  const ddouble *in9);
DE_PUBLIC bool    Matrix4_Inverse(dfloat  *out16, const dfloat *in16);
DE_PUBLIC bool    Matrix4_Inverse(ddouble *out16, const ddouble *in16);

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
        switch (specialType) {
        case Zero:
            data().clear();
            break;
        default:
            break;
        }
    }
    Matrix3(const Type *values9) {
        data().set(0, reinterpret_cast<const IByteArray::Byte *>(values9), sizeof(_values));
    }
    Matrix3(const ByteRefArray &otherData) {
        DE_ASSERT(otherData.size() == sizeof(_values));
        otherData.get(0, _values, sizeof(_values));
    }

    // Accessors.
    inline Type &at(int row, int col) {
        DE_ASSERT(row >= 0 && row < 3);
        DE_ASSERT(col >= 0 && col < 3);
        return _values[col*3 + row];
    }
    inline Type at(int row, int col) const {
        DE_ASSERT(row >= 0 && row < 3);
        DE_ASSERT(col >= 0 && col < 3);
        return _values[col*3 + row];
    }
    Vec3 row(int row) const {
        return Vec3(at(row, 0), at(row, 1), at(row, 2));
    }
    Vec3 column(int col) const {
        return Vec3(at(0, col), at(1, col), at(2, col));
    }
    inline Type &operator [] (int index) {
        DE_ASSERT(index >= 0 && index < 9);
        return _values[index];
    }
    inline Type operator [] (int index) const {
        DE_ASSERT(index >= 0 && index < 9);
        return _values[index];
    }
    ByteRefArray const data() const {
        return ByteRefArray(_values, sizeof(_values));
    }
    ByteRefArray data() {
        return ByteRefArray(_values, sizeof(_values));
    }
    const Type *values() const {
        return _values;
    }
    Type *values() {
        return _values;
    }

    // Math operations.
    Matrix3 operator * (const Matrix3 &right) const {
        Matrix3 result(Zero);
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j)
                for (int k = 0; k < 3; ++k)
                    result.at(i, j) += at(i, k) * right.at(k, j);
        return result;
    }
    Vec3 operator * (const Vec3 &vector) const {
        Vec3 result;
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j)
                result[i] += at(i, j) * vector[j];
        return result;
    }
    Vec4 operator * (const Vec4 &vector) const {
        return Vec4::fromEuclidean(*this * vector.toEuclidean());
    }
    Matrix3 inverse() const {
        Matrix3 result(Uninitialized);
        Matrix3_Inverse(result._values, _values);
        return result;
    }
    Matrix3 transpose() const {
        Matrix3 m(Uninitialized);
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j)
                m.at(i, j) = at(j, i);
        return m;
    }
    String asText() const {
        std::ostringstream s;
        s << "Matrix3:\n"
          << "  " << row(0) << "\n"
          << "  " << row(1) << "\n"
          << "  " << row(2) << "\n";
        return s.str();
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
    for (int i = 0; i < 9; ++i) to << mat3[i];
    return to;
}

template <typename Type>
inline void operator << (const Writer &to, Matrix3<Type> const &mat3) {
    Writer w(to);
    for (int i = 0; i < 9; ++i) w << mat3[i];
}

template <typename Type>
inline Reader &operator >> (Reader &from, Matrix3<Type> &mat3) {
    for (int i = 0; i < 9; ++i) from >> mat3[i];
    return from;
}

template <typename Type>
inline void operator >> (const Reader &from, Matrix3<Type> &mat3) {
    Reader r(from);
    for (int i = 0; i < 9; ++i) r >> mat3[i];
}

template <typename Type>
inline std::ostream &operator << (std::ostream &os, Matrix3<Type> const &mat3) {
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
        switch (specialType) {
        case Zero:
            data().clear();
            break;
        default:
            break;
        }
    }
    Matrix4(const Type *values16) {
        data().set(0, reinterpret_cast<const IByteArray::Byte *>(values16), sizeof(_values));
    }
    Matrix4(const ByteRefArray &otherData) {
        DE_ASSERT(otherData.size() == sizeof(_values));
        otherData.get(0, _values, sizeof(_values));
    }

    // Accessors.
    inline Type &at(int row, int col) {
        DE_ASSERT(row >= 0 && row < 4);
        DE_ASSERT(col >= 0 && col < 4);
        return _values[col*4 + row];
    }
    inline Type at(int row, int col) const {
        DE_ASSERT(row >= 0 && row < 4);
        DE_ASSERT(col >= 0 && col < 4);
        return _values[col*4 + row];
    }
    Vec4 row(int row) const {
        return Vec4(at(row, 0), at(row, 1), at(row, 2), at(row, 3));
    }
    Vec4 column(int col) const {
        return Vec4(at(0, col), at(1, col), at(2, col), at(3, col));
    }
    inline Type &operator [] (int index) {
        DE_ASSERT(index >= 0 && index < 16);
        return _values[index];
    }
    inline Type operator [] (int index) const {
        DE_ASSERT(index >= 0 && index < 16);
        return _values[index];
    }
    ByteRefArray const data() const {
        return ByteRefArray(_values, sizeof(_values));
    }
    ByteRefArray data() {
        return ByteRefArray(_values, sizeof(_values));
    }
    const Type *values() const {
        return _values;
    }
    Type *values() {
        return _values;
    }

    // Math operations.
    Matrix4 operator * (const Matrix4 &right) const {
        Matrix4 result(Zero);
        for (int i = 0; i < 4; ++i)
            for (int j = 0; j < 4; ++j)
                for (int k = 0; k < 4; ++k)
                    result.at(i, j) += at(i, k) * right.at(k, j);
        return result;
    }
    Vec2 operator * (const Vec2 &vector) const {
        return (*this * Vec4::fromEuclidean(vector)).toEuclidean();
    }
    Vec3 operator * (const Vec3 &vector) const {
        return (*this * Vec4::fromEuclidean(vector)).toEuclidean();
    }
    Vec4 operator * (const Vec4 &vector) const {
        Vec4 result;
        for (int i = 0; i < 4; ++i)
            for (int j = 0; j < 4; ++j)
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
        for (int i = 0; i < 4; ++i)
            for (int j = 0; j < 4; ++j)
                m.at(i, j) = at(j, i);
        return m;
    }
    Matrix4 withoutTranslation() const {
        Matrix4 m(*this);
        m[12] = 0;
        m[13] = 0;
        m[14] = 0;
        return m;
    }
    Matrix3<Type> submatrix(int row, int col) const {
        Matrix3<Type> sub(Matrix3<Type>::Uninitialized);
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j)
                sub.at(i, j) = at(i + row, j + col);
        return sub;
    }

    String asText() const {
        std::ostringstream s;
        s << "Matrix4:\n"
          << "  " << row(0) << "\n"
          << "  " << row(1) << "\n"
          << "  " << row(2) << "\n"
          << "  " << row(3) << "\n";
        return s.str();
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
    static Matrix4 frustum(Type left, Type right, Type bottom, Type top, Type zNear, Type zFar) {
        // This is identical to glFrustum.
        const Type A = (right + left) / (right - left);
        const Type B = (top + bottom) / (top - bottom);
        const Type C = - (zFar + zNear) / (zFar - zNear);
        const Type D = - (Type(2) * zFar * zNear) / (zFar - zNear);
        Matrix4 m(Zero);
        m.at(0, 0) = (Type(2) * zNear) / (right - left);
        m.at(1, 1) = (Type(2) * zNear) / (top - bottom);
        m.at(0, 2) = A;
        m.at(1, 2) = B;
        m.at(2, 2) = C;
        m.at(3, 2) = -Type(1);
        m.at(2, 3) = D;
        return m;
    }
    static Matrix4 perspective(Type fov, Type aspectRatio, Type nearDistance = 1.f, Type farDistance = 1000.f) {
        const Type fovx = degreeToRadian(fov);
        const Type f = Type(1.0) / Type(std::tan(Type(0.5) * fovx));
        const Type A = farDistance + nearDistance;
        const Type B = nearDistance - farDistance;
        Matrix4 m(Zero);
        m.at(0, 0) = f;
        m.at(1, 1) = f * aspectRatio;
        m.at(2, 2) = A / B;
        m.at(3, 2) = Type(-1.0);
        m.at(2, 3) = (Type(2.0) * farDistance * nearDistance) / B;
        return m;
    }
    static Matrix4 perspectiveZoom(Type width, Type height, Type nearDist = 1.f, Type farDist = 1000.f, Type zoom = 1.f) {
        const Type zoomHalf =  zoom / 2;
        const Type aspect   =  width / height;
        const Type left     = -zoomHalf;
        const Type right    =  zoomHalf;
        const Type bottom   = -zoomHalf / aspect;
        const Type top      =  zoomHalf / aspect;
        Type m[16] = {
            2 * nearDist / (right - left), 0, 0, 0,
            0, 2 * nearDist / (top - bottom), 0, 0,
            (right + left) / (right - left), (top + bottom) / (top - bottom), -(farDist + nearDist) / (farDist - nearDist), -1,
            0, 0, -2 * (farDist * nearDist) / (farDist - nearDist), 0
        };
        return m;
    }
    static Matrix4 rotate(Type angleDegrees, const Vec3 &unitAxis = Vec3(0, 0, 1)) {
        const Type ang = degreeToRadian(angleDegrees);
        const Type c   = std::cos(ang);
        const Type s   = std::sin(ang);
        Type m[16] = {
            unitAxis.x*unitAxis.x*(1-c)+c,            unitAxis.x*unitAxis.y*(1-c)+unitAxis.z*s, unitAxis.x*unitAxis.z*(1-c)-unitAxis.y*s, 0,
            unitAxis.x*unitAxis.y*(1-c)-unitAxis.z*s, unitAxis.y*unitAxis.y*(1-c)+c,            unitAxis.y*unitAxis.z*(1-c)+unitAxis.x*s, 0,
            unitAxis.x*unitAxis.z*(1-c)+unitAxis.y*s, unitAxis.z*unitAxis.y*(1-c)-unitAxis.x*s, unitAxis.z*unitAxis.z*(1-c)+c,            0,
            0, 0, 0, 1
        };
        return m;
    }
    static Matrix4 rotateAround(const Vec3 &pivot, Type angleDegrees, const Vec3 &axis = Vec3(0, 0, 1)) {
        return translate(pivot) * rotate(angleDegrees, axis) * translate(-pivot);
    }
    static Matrix4 translate(const Vec3 &translation) {
        return scaleThenTranslate(Vec3(1, 1, 1), translation);
    }
    static Matrix4 scale(Type scalar) {
        return scale(Vec3(scalar, scalar, scalar));
    }
    static Matrix4 scale(const Vec2 &scalar) {
        return scale(Vec3(scalar, Type(1)));
    }
    static Matrix4 scale(const Vec3 &scalar) {
        return scaleThenTranslate(scalar, Vec3(0, 0, 0));
    }
    static Matrix4 scaleThenTranslate(Type scalar, const Vec3 &translation) {
        return scaleThenTranslate(Vec3(scalar, scalar, scalar), translation);
    }
    static Matrix4 scaleThenTranslate(const Vec2 &scalar, const Vec2 &translation) {
        return scaleThenTranslate(scalar, Vec3(translation, 0));
    }
    static Matrix4 scaleThenTranslate(const Vec2 &scalar, const Vec3 &translation) {
        return scaleThenTranslate(Vec3(scalar, Type(1)), translation);
    }
    static Matrix4 scaleThenTranslate(const Vec3 &scalar, const Vec3 &translation) {
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
    static Matrix4 lookAt(const Vec3 &target, const Vec3 &eyePos, const Vec3 &up) {
        return frame(target - eyePos, up, true /* right-handed */) * translate(-eyePos);
    }
    static Matrix4 frame(const Vec3 &front, const Vec3 &up, bool mirrored = false) {
        return unnormalizedFrame(front.normalize(), up.normalize(), mirrored);
    }
    static Matrix4 unnormalizedFrame(const Vec3 &front, const Vec3 &up, bool mirrored = false) {
        Matrix4 m(Zero);
        Vec3 f = front;
        Vec3 s = f.cross(up);
        Vec3 u = s.cross(f);
        if (mirrored) s = -s;
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
        return m;
    }

private:
    Type _values[16];
};

// Serialization of Matrix4.
template <typename Type>
inline Writer &operator << (Writer &to, Matrix4<Type> const &mat4) {
    for (int i = 0; i < 16; ++i) to << mat4[i];
    return to;
}

template <typename Type>
inline void operator << (const Writer &to, Matrix4<Type> const &mat4) {
    Writer w(to);
    for (int i = 0; i < 16; ++i) w << mat4[i];
}

template <typename Type>
inline Reader &operator >> (Reader &from, Matrix4<Type> &mat4) {
    for (int i = 0; i < 16; ++i) from >> mat4[i];
    return from;
}

template <typename Type>
inline void operator >> (const Reader &from, Matrix4<Type> &mat4) {
    Reader r(from);
    for (int i = 0; i < 16; ++i) r >> mat4[i];
}

template <typename Type>
inline std::ostream &operator << (std::ostream &os, Matrix4<Type> const &mat4) {
    os << mat4.asText();
    return os;
}

///@{
/// @ingroup types
typedef Matrix3<dfloat>  Mat3f;
typedef Matrix3<ddouble> Mat3d;
typedef Matrix4<dfloat>  Mat4f;
typedef Matrix4<ddouble> Mat4d;
///@}

} // namespace de

#endif // LIBCORE_MATRIX_H
