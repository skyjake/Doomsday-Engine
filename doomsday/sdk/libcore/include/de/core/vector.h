/** @file vector.h Vector templates.
 *
 * The Vector templates use the convention that in an expression, the type of
 * the left-hand operand defines which type is used for the operation. For
 * instance, when comparing a < b, where @a a is a 2D vector and @a b is a 3D
 * vector, the comparison only involves the first two components of @a b
 * (comparing as a 2D vector).
 *
 * @authors Copyright © 2009-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2013 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG2_VECTOR_H
#define LIBDENG2_VECTOR_H

#include "../math.h"
#include "../Error"
#include "../ISerializable"
#include "../Writer"
#include "../ByteRefArray"
#include "../Reader"
#include "../String"
#include "../NumberValue"

#include <QTextStream>
#include <cmath>

#ifdef WIN32
#  undef min
#  undef max
#endif

namespace de {

// Function for comparing numbers for equality in the Vector templates.
template <typename T>
inline bool numberEqual(T const &a, T const &b) {
    return a == b;
}
template <>
inline bool numberEqual(dfloat const &a, dfloat const &b) {
    return fequal(a, b);
}
template <>
inline bool numberEqual(ddouble const &a, ddouble const &b) {
    return fequal(a, b);
}

/**
 * Utility for converting a value with multiple elements into a vector. This
 * only works if the value contains a sufficient number of elements and they
 * can be converted to numbers.
 *
 * @param value  Value with multiple elements.
 *
 * @return Vector.
 */
template <typename VecType>
VecType vectorFromValue(Value const &value) {
    VecType converted;
    for (int i = 0; i < converted.size(); ++i) {
        converted[i] = typename VecType::ValueType(value.element(i).asNumber());
    }
    return converted;
}

enum SwizzleAxis
{
    AxisX = 0,
    AxisY = 1,
    AxisZ = 2,
    AxisW = 3,

    AxisNegX = -1,
    AxisNegY = -2,
    AxisNegZ = -3,
    AxisNegW = -4
};

/**
 * Template class for 2D vectors (points). The members are public for
 * convenient access. The used value type must be serializable.
 *
 * Does not directly implement ISerializable to keep the size of the class
 * at sizeof(Type) * 2 for array usage.
 *
 * @ingroup math
 */
template <typename Type>
class Vector2
{
public:
    typedef Type ValueType;

public:
    Vector2(Type a = Type(0), Type b = Type(0)) : x(a), y(b) {}
    Vector2(Type const *ab) : x(ab[0]), y(ab[1]) {}
    Vector2(Value const &value) { *this = vectorFromValue< Vector2<Type> >(value); }
    Vector2(Vector2 const &other) : x(other.x), y(other.y) {}

    /// Implicit conversion operator to a float vector.
    operator Vector2<dfloat> () const {
        return Vector2<dfloat>(dfloat(x), dfloat(y));
    }
    /// Implicit conversion operator to a double vector.
    operator Vector2<ddouble> () const {
        return Vector2<ddouble>(ddouble(x), ddouble(y));
    }
    Vector2<dint> toVector2i() const {
        return Vector2<dint>(dint(x), dint(y));
    }
    Vector2<duint> toVector2ui() const {
        return Vector2<duint>(duint(de::max(Type(0), x)), duint(de::max(Type(0), y)));
    }
    int size() const {
        return 2;
    }
    ByteRefArray const data() const {
        return ByteRefArray(&x, size() * sizeof(ValueType));
    }
    ByteRefArray data() {
        return ByteRefArray(&x, size() * sizeof(ValueType));
    }
    ValueType const *constPtr() const {
        return &x;
    }
    Type &operator [] (int index) {
        DENG2_ASSERT(index >= 0 && index <= 1);
        if (index >= 0 && index < 2)
        {
            return (&x)[index];
        }
        throw Error("Vector2::operator []", QString("Illegal index %1").arg(index));
    }
    Type const &operator [] (int index) const {
        return const_cast<Vector2<Type> &>(*this)[index];
    }
    Vector2 operator + (Vector2 const &other) const {
        return Vector2(x + other.x, y + other.y);
    }
    Vector2 operator - (Vector2 const &other) const {
        return Vector2(x - other.x, y - other.y);
    }
    Vector2 operator - () const {
        return Vector2(-x, -y);
    }
    Vector2 operator * (int scalar) const {
        return Vector2(Type(x * scalar), Type(y * scalar));
    }
    Vector2 operator * (float scalar) const {
        return Vector2(Type(x * scalar), Type(y * scalar));
    }
    Vector2 operator * (double scalar) const {
        return Vector2(Type(x * scalar), Type(y * scalar));
    }
    Vector2 operator * (Vector2 const &other) const {
        return Vector2(x * other.x, y * other.y);
    }
    Vector2 operator / (ddouble scalar) const {
        return *this * (1.0 / scalar);
    }
    Vector2 operator / (Vector2 const &other) const {
        return Vector2(x / other.x, y / other.y);
    }
    Vector2 &operator += (Vector2 const &other) {
        x += other.x;
        y += other.y;
        return *this;
    }
    Vector2 &operator -= (Vector2 const &other) {
        x -= other.x;
        y -= other.y;
        return *this;
    }
    inline Vector2 &operator *= (ddouble scalar) {
        x *= scalar;
        y *= scalar;
        return *this;
    }
    inline Vector2 &operator *= (Vector2 const &other) {
        x *= other.x;
        y *= other.y;
        return *this;
    }
    inline Vector2 &operator /= (ddouble scalar) {
        return (*this) *= 1.0 / scalar;
    }
    inline bool operator == (Vector2 const &other) const {
        return numberEqual(x, other.x) && numberEqual(y, other.y);
    }
    inline bool operator != (Vector2 const &other) const {
        return !(*this == other);
    }
    bool operator > (Vector2 const &other) const {
        if (x == other.x) return y > other.y;
        return x > other.x && y > other.y;
    }
    bool operator < (Vector2 const &other) const {
        if (x == other.x) return y < other.y;
        return x < other.x && y < other.y;
    }
    bool operator >= (Vector2 const &other) const {
        return *this == other || *this > other;
    }
    bool operator <= (Vector2 const &other) const {
        return *this == other || *this < other;
    }
    inline ddouble length() const {
        return std::sqrt(lengthSquared());
    }
    inline ddouble lengthSquared() const {
        return ddouble(x*x + y*y);
    }
    void setLength(ddouble len) {
        *this = *this / length() * len;
    }
    Vector2 normalize() const {
        ddouble const len = length();
        if (len != 0) {
            return *this / len;
        }
        return Vector2();
    }
    String asText() const {
        String str;
        QTextStream s(&str);
        s << *this;
        return str;
    }
    Vector2 abs() const {
        return Vector2(de::abs(x), de::abs(y));
    }
    double dot(const Vector2 &other) const {
        return x * other.x + y * other.y;
    }
    double cross(const Vector2 &other) const {
        return x * other.y - y * other.x;
    }
    Vector2 min(Vector2 const &other) const {
        return Vector2(de::min(x, other.x), de::min(y, other.y));
    }
    Vector2 max(Vector2 const &other) const {
        return Vector2(de::max(x, other.x), de::max(y, other.y));
    }
    Type min() const {
        return de::min(x, y);
    }
    Type max() const {
        return de::max(x, y);
    }
    int minAxis() const {
        Vector2 vecAbs = abs();
        return vecAbs.x < vecAbs.y? 0 : 1;
    }
    int maxAxis() const {
        Vector2 vecAbs = abs();
        return vecAbs.x > vecAbs.y? 1 : 0;
    }
    inline void decompose(Type *array) const {
        for (int i = 0; i < 2; ++i) array[i] = (*this)[i];
    }
    Vector2<Type> yx() const { return swizzle(*this, AxisY, AxisX); }

public:
    Type x;
    Type y;
};

// Serialization of Vector2.
template <typename Type>
inline Writer &operator << (Writer &to, Vector2<Type> const &vec2) {
    to << vec2.x << vec2.y;
    return to;
}

template <typename Type>
inline void operator << (Writer const &to, Vector2<Type> const &vec2) {
    Writer w(to);
    w << vec2.x << vec2.y;
}

template <typename Type>
inline Reader &operator >> (Reader &from, Vector2<Type> &vec2) {
    from >> vec2.x >> vec2.y;
    return from;
}

template <typename Type>
inline void operator >> (Reader const &from, Vector2<Type> &vec2) {
    Reader r(from);
    r >> vec2.x >> vec2.y;
}

template <typename Type>
inline QTextStream &operator << (QTextStream &os, Vector2<Type> const &vec2) {
    os << "(" << vec2.x << ", " << vec2.y << ")";
    return os;
}

/**
 * Template class for 3D vectors (points).
 * The members are public for convenient access.
 *
 * @ingroup math
 */
template <typename Type>
class Vector3 : public Vector2<Type>
{
public:
    Vector3(Type a = 0, Type b = 0, Type c = 0) : Vector2<Type>(a, b), z(c) {}
    Vector3(Vector2<Type> const &v2, Type c = 0) : Vector2<Type>(v2), z(c) {}
    Vector3(Type const *abc) : Vector2<Type>(abc), z(abc[2]) {}
    Vector3(Value const &value) { *this = vectorFromValue< Vector3<Type> >(value); }

    /// Implicit conversion operator to a float vector.
    operator Vector3<dfloat> () const {
        return Vector3<dfloat>(Vector2<Type>::x, Vector2<Type>::y, z);
    }
    /// Implicit conversion operator to a double vector.
    operator Vector3<ddouble> () const {
        return Vector3<ddouble>(Vector2<Type>::x, Vector2<Type>::y, z);
    }
    Vector3<dbyte> toVector3ub() const {
        return Vector3<dbyte>(dbyte(Vector2<Type>::x), dbyte(Vector2<Type>::y), dbyte(z));
    }
    Vector3<dfloat> toVector3f() const {
        return Vector3<dfloat>(dfloat(Vector2<Type>::x), dfloat(Vector2<Type>::y), dfloat(z));
    }
    Vector3<dint> toVector3i() const {
        return Vector3<dint>(dint(Vector2<Type>::x), dint(Vector2<Type>::y), dint(z));
    }
    int size() const {
        return 3;
    }
    ByteRefArray const data() const {
        return ByteRefArray(&this->Vector2<Type>::x, size() * sizeof(Type));
    }
    ByteRefArray data() {
        return ByteRefArray(&this->Vector2<Type>::x, size() * sizeof(Type));
    }
    Type &operator [] (int index) {
        DENG2_ASSERT(index >= 0 && index <= 2);
        if (index >= 0 && index < 3)
        {
            return (&(this->Vector2<Type>::x))[index];
        }
        throw Error("Vector3::operator []", QString("Illegal index %1").arg(index));
    }
    Type const &operator [] (int index) const {
        return const_cast<Vector3<Type> &>(*this)[index];
    }
    Vector3 operator + (Vector3 const &other) const {
        return Vector3(Vector2<Type>::x + other.x, Vector2<Type>::y + other.y, z + other.z);
    }
    Vector3 operator - (Vector3 const &other) const {
        return Vector3(Vector2<Type>::x - other.x, Vector2<Type>::y - other.y, z - other.z);
    }
    Vector3 operator - () const {
        return Vector3(-Vector2<Type>::x, -Vector2<Type>::y, -z);
    }
    Vector3 operator * (ddouble scalar) const {
        return Vector3(Type(Vector2<Type>::x * scalar), Type(Vector2<Type>::y * scalar),
            Type(z * scalar));
    }
    Vector3 operator / (ddouble scalar) const {
        return *this * (1.0 / scalar);
    }
    Vector3 operator * (Vector3 const &other) const {
        return Vector3(Vector2<Type>::x * other.x, Vector2<Type>::y * other.y, z * other.z);
    }
    Vector3 &operator += (Vector3 const &other) {
        Vector2<Type>::x += other.x;
        Vector2<Type>::y += other.y;
        z += other.z;
        return *this;
    }
    Vector3 &operator -= (Vector3 const &other) {
        Vector2<Type>::x -= other.x;
        Vector2<Type>::y -= other.y;
        z -= other.z;
        return *this;
    }
    inline Vector3 &operator *= (ddouble scalar) {
        Vector2<Type>::operator *= (scalar);
        z *= scalar;
        return *this;
    }
    inline Vector3 &operator *= (Vector3 const &other) {
        Vector2<Type>::operator *= (other);
        z *= other.z;
        return *this;
    }
    inline Vector3 &operator /= (ddouble scalar) {
        return (*this) *= 1.0 / scalar;
    }
    inline bool operator == (Vector3 const &other) const {
        return Vector2<Type>::operator == (other) && numberEqual(z, other.z);
    }
    inline bool operator != (Vector3 const &other) const {
        return !(*this == other);
    }
    bool operator > (Vector3 const &other) const {
        if (Vector2<Type>::operator == (other)) return z > other.z;
        return Vector2<Type>::operator > (other) && z > other.z;
    }
    bool operator < (Vector3 const &other) const {
        if (Vector2<Type>::operator == (other)) return z < other.z;
        return Vector2<Type>::operator < (other) && z < other.z;
    }
    bool operator >= (Vector3 const &other) const {
        return *this == other || *this > other;
    }
    bool operator <= (Vector3 const &other) const {
        return *this == other || *this < other;
    }
    inline ddouble length() const {
        return std::sqrt(lengthSquared());
    }
    void setLength(ddouble len) {
        *this = *this / length() * len;
    }
    ddouble lengthSquared() const {
        return Vector2<Type>::x*Vector2<Type>::x + Vector2<Type>::y*Vector2<Type>::y + z*z;
    }
    Vector3 normalize() const {
        ddouble const len = length();
        if (len != 0) {
            return *this / len;
        }
        return Vector3();
    }
    String asText() const {
        String str;
        QTextStream os(&str);
        os << *this;
        return str;
    }
    Vector3 abs() const {
        return Vector3(de::abs(Vector2<Type>::x), de::abs(Vector2<Type>::y), de::abs(z));
    }
    ddouble dot(Vector3 const &other) const {
        return Vector2<Type>::x * other.x + Vector2<Type>::y * other.y + z * other.z;
    }
    inline Vector3 cross(Vector3 const &other) const {
        return Vector3(Vector2<Type>::y * other.z - z * other.y,
                       z * other.x - Vector2<Type>::x * other.z,
                       Vector2<Type>::x * other.y - Vector2<Type>::y * other.x);
    }
    Vector3 min(Vector3 const &other) const {
        return Vector3(de::min(Vector2<Type>::x, other.x), de::min(Vector2<Type>::y, other.y),
            de::min(z, other.z));
    }
    Vector3 max(Vector3 const &other) const {
        return Vector3(de::max(Vector2<Type>::x, other.x), de::max(Vector2<Type>::y, other.y),
            de::max(z, other.z));
    }
    Type min() const {
        return de::min(z, Vector2<Type>::min());
    }
    Type max() const {
        return de::max(z, Vector2<Type>::max());
    }
    int minAxis() const {
        Vector3 vecAbs = abs();
        int axis = 2;
        if (vecAbs.y < vecAbs[axis]) axis = 1;
        if (vecAbs.x < vecAbs[axis]) axis = 0;
        return axis;
    }
    int maxAxis() const {
        Vector3 vecAbs = abs();
        int axis = 0;
        if (vecAbs.y > vecAbs[axis]) axis = 1;
        if (vecAbs.z > vecAbs[axis]) axis = 2;
        return axis;
    }
    inline void decompose(Type *array) const {
        for (int i = 0; i < 3; ++i) array[i] = (*this)[i];
    }
    Vector2<Type> xy() const { return *this; }
    Vector2<Type> xz() const { return swizzle(*this, AxisX, AxisZ); }
    Vector3<Type> xzy() const { return swizzle(*this, AxisX, AxisZ, AxisY); }
    Vector3<Type> zyx() const { return swizzle(*this, AxisZ, AxisY, AxisX); }

public:
    Type z;
};

// Serialization of Vector3.
template <typename Type>
inline Writer &operator << (Writer &to, Vector3<Type> const &vec3) {
    to << vec3.x << vec3.y << vec3.z;
    return to;
}

template <typename Type>
inline void operator << (Writer const &to, Vector3<Type> const &vec3) {
    Writer w(to);
    w << vec3.x << vec3.y << vec3.z;
}

template <typename Type>
inline Reader &operator >> (Reader &from, Vector3<Type> &vec3) {
    from >> vec3.x >> vec3.y >> vec3.z;
    return from;
}

template <typename Type>
inline void operator >> (Reader const &from, Vector3<Type> &vec3) {
    Reader r(from);
    r >> vec3.x >> vec3.y >> vec3.z;
}

template <typename Type>
QTextStream &operator << (QTextStream &os, Vector3<Type> const &vec3)
{
    os << "(" << vec3.x << ", " << vec3.y << ", " << vec3.z << ")";
    return os;
}

/**
 * Template class for 4D vectors.
 * The members are public for convenient access.
 *
 * Note that when mixing 3D and 4D vectors, by default the automatic conversion
 * between these simply disregards the @em w component. If the intention is to
 * treat 4D vectors as homogeneous, one must explicitly convert to/from 3D
 * vectors using Vector4<Type>::fromEuclidean() and
 * Vector4<Type>::toEuclidean().
 *
 * @ingroup math
 */
template <typename Type>
class Vector4 : public Vector3<Type>
{
public:
    Vector4(Type a = 0, Type b = 0, Type c = 0, Type d = 0) : Vector3<Type>(a, b, c), w(d) {}
    Vector4(Vector3<Type> const &v3, Type d = 0) : Vector3<Type>(v3), w(d) {}
    Vector4(Vector2<Type> const &a, Vector2<Type> const &b) : Vector3<Type>(a, b.x), w(b.y) {}
    Vector4(Type const *abcd) : Vector3<Type>(abcd), w(abcd[3]) {}
    Vector4(Value const &value) { *this = vectorFromValue< Vector4<Type> >(value); }

    /// Implicit conversion operator to a float vector.
    operator Vector4<dfloat> () const {
        return Vector4<dfloat>(Vector3<Type>::x, Vector3<Type>::y, Vector3<Type>::z, w);
    }
    /// Implicit conversion operator to a double vector.
    operator Vector4<ddouble> () const {
        return Vector4<ddouble>(Vector3<Type>::x, Vector3<Type>::y, Vector3<Type>::z, w);
    }
    Vector4<dint> toVector4i() const {
        return Vector4<dint>(dint(Vector3<Type>::x), dint(Vector3<Type>::y), dint(Vector3<Type>::z), dint(w));
    }
    Vector4<dbyte> toVector4ub() const {
        return Vector4<dbyte>(dbyte(Vector3<Type>::x), dbyte(Vector3<Type>::y), dbyte(Vector3<Type>::z), dbyte(w));
    }
    Vector4<dfloat> toVector4f() const {
        return Vector4<dfloat>(dfloat(Vector3<Type>::x), dfloat(Vector3<Type>::y), dfloat(Vector3<Type>::z), dfloat(w));
    }
    int size() const {
        return 4;
    }
    ByteRefArray const data() const {
        return ByteRefArray(&this->Vector2<Type>::x, size() * sizeof(Type));
    }
    ByteRefArray data() {
        return ByteRefArray(&this->Vector2<Type>::x, size() * sizeof(Type));
    }
    Type &operator [] (int index) {
        DENG2_ASSERT(index >= 0 && index <= 3);
        if (index >= 0 && index < 4)
        {
            return (&(this->Vector2<Type>::x))[index];
        }
        throw Error("Vector4::operator []", QString("Illegal index %1").arg(index));
    }
    Type const &operator [] (int index) const {
        return const_cast<Vector4<Type> &>(*this)[index];
    }
    Vector4 operator + (Vector4 const &other) const {
        return Vector4(Vector3<Type>::x + other.x, Vector3<Type>::y + other.y,
            Vector3<Type>::z + other.z, w + other.w);
    }
    Vector4 operator - (Vector4 const &other) const {
        return Vector4(Vector3<Type>::x - other.x, Vector3<Type>::y - other.y,
            Vector3<Type>::z - other.z, w - other.w);
    }
    Vector4 operator - () const {
        return Vector4(-Vector3<Type>::x, -Vector3<Type>::y, -Vector3<Type>::z, -w);
    }
    Vector4 operator * (ddouble scalar) const {
        return Vector4(Type(Vector3<Type>::x * scalar), Type(Vector3<Type>::y * scalar),
            Type(Vector3<Type>::z * scalar), Type(w * scalar));
    }
    Vector4 operator * (Vector4 const &other) const {
        return Vector4(Vector3<Type>::x * other.x, Vector3<Type>::y * other.y,
            Vector3<Type>::z * other.z, w * other.w);
    }
    Vector4 &operator += (Vector4 const &other) {
        Vector3<Type>::x += other.x;
        Vector3<Type>::y += other.y;
        Vector3<Type>::z += other.z;
        w += other.w;
        return *this;
    }
    Vector4 &operator -= (Vector4 const &other) {
        Vector3<Type>::x -= other.x;
        Vector3<Type>::y -= other.y;
        Vector3<Type>::z -= other.z;
        w -= other.w;
        return *this;
    }
    inline Vector4 &operator *= (ddouble scalar) {
        Vector3<Type>::operator *= (scalar);
        w *= scalar;
        return *this;
    }
    inline Vector4 &operator *= (Vector4 const &other) {
        Vector3<Type>::operator *= (other);
        w *= other.w;
        return *this;
    }
    Vector4 operator / (ddouble scalar) const {
        return *this * (1.0 / scalar);
    }
    inline Vector4 &operator /= (ddouble scalar) {
        return (*this) *= 1.0 / scalar;
    }
    inline bool operator == (Vector4 const &other) const {
        return Vector3<Type>::operator == (other) && numberEqual(w, other.w);
    }
    inline bool operator != (Vector4 const &other) const {
        return !(*this == other);
    }
    bool operator > (Vector4 const &other) const {
        if (Vector3<Type>::operator == (other)) return w > other.w;
        return Vector3<Type>::operator > (other) && w > other.w;
    }
    bool operator < (Vector4 const &other) const {
        if (Vector3<Type>::operator == (other)) return w < other.w;
        return Vector3<Type>::operator < (other) && w < other.w;
    }
    bool operator >= (Vector4 const &other) const {
        return *this == other || *this > other;
    }
    bool operator <= (Vector4 const &other) const {
        return *this == other || *this < other;
    }
    String asText() const {
        String str;
        QTextStream os(&str);
        os << *this;
        return str;
    }
    Vector4 abs() const {
        return Vector4(de::abs(Vector3<Type>::x), de::abs(Vector3<Type>::y), de::abs(Vector3<Type>::z), de::abs(w));
    }
    ddouble dot(Vector4 const &other) const {
        return Vector3<Type>::x * other.x + Vector3<Type>::y * other.y
             + Vector3<Type>::z * other.z + w * other.w;
    }
    Vector4 min(Vector4 const &other) const {
        return Vector4(de::min(Vector3<Type>::x, other.x), de::min(Vector3<Type>::y, other.y),
            de::min(Vector3<Type>::z, other.z), de::min(w, other.w));
    }
    Vector4 max(Vector4 const &other) const {
        return Vector4(de::max(Vector3<Type>::x, other.x), de::max(Vector3<Type>::y, other.y),
            de::max(Vector3<Type>::z, other.z), de::max(w, other.w));
    }
    Type min() const {
        return de::min(w, Vector3<Type>::min());
    }
    Type max() const {
        return de::max(w, Vector3<Type>::max());
    }
    int minAxis() const {
        Vector4 vecAbs = abs();
        int axis = 3;
        if (vecAbs.z < vecAbs[axis]) axis = 2;
        if (vecAbs.y < vecAbs[axis]) axis = 1;
        if (vecAbs.x < vecAbs[axis]) axis = 0;
        return axis;
    }
    int maxAxis() const {
        Vector4 vecAbs = abs();
        int axis = 0;
        if (vecAbs.y > vecAbs[axis]) axis = 1;
        if (vecAbs.z > vecAbs[axis]) axis = 2;
        if (vecAbs.w > vecAbs[axis]) axis = 3;
        return axis;
    }
    inline void decompose(Type *array) const {
        for (int i = 0; i < 4; ++i) array[i] = (*this)[i];
    }

    // Implements ISerializable.
    void operator >> (Writer &to) const {
        Vector3<Type>::operator >> (to);
        to << w;
    }
    void operator << (Reader &from) {
        Vector3<Type>::operator << (from);
        from >> w;
    }

    static Vector4 fromEuclidean(Vector3<Type> const &vec3) {
        return Vector4(vec3, Type(1));
    }
    Vector3<Type> toEuclidean() const {
        if (w != 0)
        {
            return Vector3<Type>(Vector2<Type>::x/w, Vector2<Type>::y/w, Vector3<Type>::z/w);
        }
        return Vector3<Type>();
    }
    Vector2<Type> xy() const   { return *this; }
    Vector2<Type> zw() const   { return swizzle(*this, AxisZ, AxisW); }
    Vector3<Type> xyz() const  { return *this; }
    Vector4<Type> zyxw() const { return swizzle(*this, AxisZ, AxisY, AxisX, AxisW); }
    Vector4<Type> replaced(int index, Type const &value) const {
        Vector4 v = *this;
        v[index] = value;
        return v;
    }

public:
    Type w;
};

// Swizzling.
template <typename Type>
typename Type::ValueType swizzledComponent(Type const &vec, SwizzleAxis axis) {
    if (axis >= 0) return vec[axis];
    return -vec[-axis - 1];
}

template <typename Type>
Vector2<typename Type::ValueType> swizzle(Type const &vec, SwizzleAxis a, SwizzleAxis b) {
    return Vector2<typename Type::ValueType>(swizzledComponent(vec, a),
                                             swizzledComponent(vec, b));
}

template <typename Type>
Vector3<typename Type::ValueType> swizzle(Type const &vec, SwizzleAxis a, SwizzleAxis b, SwizzleAxis c) {
    return Vector3<typename Type::ValueType>(swizzledComponent(vec, a),
                                             swizzledComponent(vec, b),
                                             swizzledComponent(vec, c));
}

template <typename Type>
Vector4<typename Type::ValueType> swizzle(Type const &vec, SwizzleAxis a, SwizzleAxis b, SwizzleAxis c, SwizzleAxis d) {
    return Vector4<typename Type::ValueType>(swizzledComponent(vec, a),
                                             swizzledComponent(vec, b),
                                             swizzledComponent(vec, c),
                                             swizzledComponent(vec, d));
}

// Serialization of Vector4.
template <typename Type>
inline Writer &operator << (Writer &to, Vector4<Type> const &vec4) {
    to << vec4.x << vec4.y << vec4.z << vec4.w;
    return to;
}

template <typename Type>
inline void operator << (Writer const &to, Vector4<Type> const &vec4) {
    Writer w(to);
    w << vec4.x << vec4.y << vec4.z << vec4.w;
}

template <typename Type>
inline Reader &operator >> (Reader &from, Vector4<Type> &vec4) {
    from >> vec4.x >> vec4.y >> vec4.z >> vec4.w;
    return from;
}

template <typename Type>
inline void operator >> (Reader const &from, Vector4<Type> &vec4) {
    Reader r(from);
    r >> vec4.x >> vec4.y >> vec4.z >> vec4.w;
}

template <typename Type>
QTextStream &operator << (QTextStream &os, Vector4<Type> const &vec4)
{
    os << "(" << vec4.x << ", " << vec4.y << ", " << vec4.z << ", " << vec4.w << ")";
    return os;
}

///@{
/// @ingroup types
typedef Vector2<dint>    Vector2i;  ///< 2-component vector of integer values.
typedef Vector2<duint>   Vector2ui; ///< 2-component vector of unsigned integer values.
typedef Vector2<dfloat>  Vector2f;  ///< 2-component vector of floating point values.
typedef Vector2<ddouble> Vector2d;  ///< 2-component vector of high-precision floating point values.
typedef Vector3<dbyte>   Vector3ub; ///< 3-component vector of unsigned byte values.
typedef Vector3<dint>    Vector3i;  ///< 3-component vector of integer values.
typedef Vector3<duint>   Vector3ui; ///< 3-component vector of unsigned integer values.
typedef Vector3<dfloat>  Vector3f;  ///< 3-component vector of floating point values.
typedef Vector3<ddouble> Vector3d;  ///< 3-component vector of high-precision floating point values.
typedef Vector4<dbyte>   Vector4ub; ///< 4-component vector of unsigned byte values.
typedef Vector4<dint>    Vector4i;  ///< 4-component vector of integer values.
typedef Vector4<duint>   Vector4ui; ///< 4-component vector of unsigned integer values.
typedef Vector4<dfloat>  Vector4f;  ///< 4-component vector of floating point values.
typedef Vector4<ddouble> Vector4d;  ///< 4-component vector of high-precision floating point values.
///@}

using Vec2i = Vector2i;
using Vec2ui = Vector2ui;
using Vec3i = Vector3i;
using Vec3ui = Vector3ui;
using Vec4i = Vector4i;
using Vec4ub = Vector4ub;
using Vec2f = Vector2f;
using Vec3f = Vector3f;
using Vec4f = Vector4f;
using Vec2d = Vector2d;
using Vec3d = Vector3d;
using Vec4d = Vector4d;

// Qt hash functions:
inline quint32 qHash(Vector2i const &vec) { return vec.x * vec.y + vec.x - vec.y; }

} // namespace de

#endif /* LIBDENG2_VECTOR_H */
