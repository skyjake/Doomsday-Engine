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

#ifndef LIBCORE_VECTOR_H
#define LIBCORE_VECTOR_H

#include "de/math.h"
#include "de/error.h"
#include "de/iserializable.h"
#include "de/writer.h"
#include "de/byterefarray.h"
#include "de/reader.h"
#include "de/string.h"
#include "de/numbervalue.h"

#include <sstream>
#include <cmath>

#ifdef WIN32
#  undef min
#  undef max
#endif

namespace de {

// Function for comparing numbers for equality in the Vector templates.
template <typename T>
inline bool numberEqual(const T &a, const T &b) {
    return a == b;
}
template <>
inline bool numberEqual(const dfloat &a, const dfloat &b) {
    return fequal(a, b);
}
template <>
inline bool numberEqual(const ddouble &a, const ddouble &b) {
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
VecType vectorFromValue(const Value &value) {
    VecType converted;
    for (int i = 0; i < converted.size(); ++i) {
        converted[i] = typename VecType::value_type(value.element(i).asNumber());
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
    typedef Type value_type;

public:
    constexpr Vector2(Type xy = Type(0)) : x(xy), y(xy) {}
    constexpr Vector2(Type a, Type b) : x(a), y(b) {}
    explicit Vector2(const Type *ab) : x(ab[0]), y(ab[1]) {}
    Vector2(const Value &value) { *this = vectorFromValue<Vector2<Type>>(value); }
    Vector2(const Vector2 &other) = default;

    /// Implicit conversion operator to a float vector.
    operator Vector2<dfloat> () const {
        return Vector2<dfloat>(dfloat(x), dfloat(y));
    }
    /// Implicit conversion operator to a double vector.
    operator Vector2<ddouble> () const {
        return Vector2<ddouble>(ddouble(x), ddouble(y));
    }
    Vector2<dint> toVec2i() const {
        return Vector2<dint>(dint(x), dint(y));
    }
    Vector2<duint> toVec2ui() const {
        return Vector2<duint>(duint(de::max(Type(0), x)), duint(de::max(Type(0), y)));
    }
    int size() const {
        return 2;
    }
    ByteRefArray const data() const {
        return ByteRefArray(&x, size() * sizeof(value_type));
    }
    ByteRefArray data() {
        return ByteRefArray(&x, size() * sizeof(value_type));
    }
    const value_type *constPtr() const {
        return &x;
    }
    Type &operator [] (int index) {
        DE_ASSERT(index >= 0 && index <= 1);
        if (index >= 0 && index < 2)
        {
            return (&x)[index];
        }
        throw Error("Vector2::operator []", "Illegal index " + String::asText(index));
    }
    const Type &operator [] (int index) const {
        return const_cast<Vector2<Type> &>(*this)[index];
    }
    Vector2 operator + (const Vector2 &other) const {
        return Vector2(x + other.x, y + other.y);
    }
    Vector2 operator - (const Vector2 &other) const {
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
    Vector2 operator * (const Vector2 &other) const {
        return Vector2(x * other.x, y * other.y);
    }
    Vector2 operator / (ddouble scalar) const {
        return *this * (1.0 / scalar);
    }
    Vector2 operator / (const Vector2 &other) const {
        return Vector2(x / other.x, y / other.y);
    }
    Vector2 &operator += (const Vector2 &other) {
        x += other.x;
        y += other.y;
        return *this;
    }
    Vector2 &operator -= (const Vector2 &other) {
        x -= other.x;
        y -= other.y;
        return *this;
    }
    inline Vector2 &operator *= (ddouble scalar) {
        x *= scalar;
        y *= scalar;
        return *this;
    }
    inline Vector2 &operator *= (const Vector2 &other) {
        x *= other.x;
        y *= other.y;
        return *this;
    }
    inline Vector2 &operator /= (ddouble scalar) {
        return (*this) *= 1.0 / scalar;
    }
    inline bool operator == (const Vector2 &other) const {
        return numberEqual(x, other.x) && numberEqual(y, other.y);
    }
    inline bool operator != (const Vector2 &other) const {
        return !(*this == other);
    }
    bool operator > (const Vector2 &other) const {
        if (x == other.x) return y > other.y;
        return x > other.x && y > other.y;
    }
    bool operator < (const Vector2 &other) const {
        if (x == other.x) return y < other.y;
        return x < other.x && y < other.y;
    }
    bool operator >= (const Vector2 &other) const {
        return *this == other || *this > other;
    }
    bool operator <= (const Vector2 &other) const {
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
        const ddouble len = length();
        if (!fequal(len, 0.0)) {
            return *this / len;
        }
        return Vector2();
    }
    String asText() const {
        std::ostringstream s;
        s << *this;
        return s.str();
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
    Vector2 min(const Vector2 &other) const {
        return Vector2(de::min(x, other.x), de::min(y, other.y));
    }
    Vector2 max(const Vector2 &other) const {
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
    Type area() const {
        return x * y;
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
inline void operator << (const Writer &to, Vector2<Type> const &vec2) {
    Writer w(to);
    w << vec2.x << vec2.y;
}

template <typename Type>
inline Reader &operator >> (Reader &from, Vector2<Type> &vec2) {
    from >> vec2.x >> vec2.y;
    return from;
}

template <typename Type>
inline void operator >> (const Reader &from, Vector2<Type> &vec2) {
    Reader r(from);
    r >> vec2.x >> vec2.y;
}

template <typename Type>
inline std::ostream &operator << (std::ostream &os, const Vector2<Type> &vec2) {
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
    constexpr Vector3(Type xyz = Type(0)) : Vector2<Type>(xyz), z(xyz) {}
    constexpr Vector3(Type a, Type b, Type c) : Vector2<Type>(a, b), z(c) {}
    constexpr Vector3(const Vector2<Type> &v2, Type c = Type(0)) : Vector2<Type>(v2), z(c) {}
    explicit Vector3(const Type *abc) : Vector2<Type>(abc), z(abc[2]) {}
    Vector3(const Value &value) { *this = vectorFromValue<Vector3<Type>>(value); }
    Vector3(const Vector3 &other) = default;

    /// Implicit conversion operator to a float vector.
    operator Vector3<dfloat> () const {
        return Vector3<dfloat>(Vector2<Type>::x, Vector2<Type>::y, z);
    }
    /// Implicit conversion operator to a double vector.
    operator Vector3<ddouble> () const {
        return Vector3<ddouble>(Vector2<Type>::x, Vector2<Type>::y, z);
    }
    Vector3<dbyte> toVec3ub() const {
        return Vector3<dbyte>(dbyte(Vector2<Type>::x), dbyte(Vector2<Type>::y), dbyte(z));
    }
    Vector3<dfloat> toVec3f() const {
        return Vector3<dfloat>(dfloat(Vector2<Type>::x), dfloat(Vector2<Type>::y), dfloat(z));
    }
    Vector3<dint> toVec3i() const {
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
        DE_ASSERT(index >= 0 && index <= 2);
        if (index >= 0 && index < 3)
        {
            return (&(this->Vector2<Type>::x))[index];
        }
        throw Error("Vector3::operator []", "Illegal index " + String::asText(index));
    }
    const Type &operator [] (int index) const {
        return const_cast<Vector3<Type> &>(*this)[index];
    }
    Vector3 operator + (const Vector3 &other) const {
        return Vector3(Vector2<Type>::x + other.x, Vector2<Type>::y + other.y, z + other.z);
    }
    Vector3 operator - (const Vector3 &other) const {
        return Vector3(Vector2<Type>::x - other.x, Vector2<Type>::y - other.y, z - other.z);
    }
    Vector3 operator - () const {
        return Vector3(-Vector2<Type>::x, -Vector2<Type>::y, -z);
    }
    Vector3 operator * (ddouble scalar) const {
        return Vector3(Type(Vector2<Type>::x * scalar), Type(Vector2<Type>::y * scalar),
            Type(z * scalar));
    }
    Vector3 operator * (const Vector3 &other) const {
        return Vector3(Vector2<Type>::x * other.x, Vector2<Type>::y * other.y, z * other.z);
    }
    Vector3 operator / (ddouble scalar) const {
        return *this * (1.0 / scalar);
    }
    Vector3 operator / (const Vector3 &other) const {
        return Vector3(Vector2<Type>::x / other.x, Vector2<Type>::y / other.y, z / other.z);
    }
    Vector3 &operator += (const Vector3 &other) {
        Vector2<Type>::x += other.x;
        Vector2<Type>::y += other.y;
        z += other.z;
        return *this;
    }
    Vector3 &operator -= (const Vector3 &other) {
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
    inline Vector3 &operator *= (const Vector3 &other) {
        Vector2<Type>::operator *= (other);
        z *= other.z;
        return *this;
    }
    inline Vector3 &operator /= (ddouble scalar) {
        return (*this) *= 1.0 / scalar;
    }
    inline bool operator == (const Vector3 &other) const {
        return Vector2<Type>::operator == (other) && numberEqual(z, other.z);
    }
    inline bool operator != (const Vector3 &other) const {
        return !(*this == other);
    }
    bool operator > (const Vector3 &other) const {
        if (Vector2<Type>::operator == (other)) return z > other.z;
        return Vector2<Type>::operator > (other) && z > other.z;
    }
    bool operator < (const Vector3 &other) const {
        if (Vector2<Type>::operator == (other)) return z < other.z;
        return Vector2<Type>::operator < (other) && z < other.z;
    }
    bool operator >= (const Vector3 &other) const {
        return *this == other || *this > other;
    }
    bool operator <= (const Vector3 &other) const {
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
        const ddouble len = length();
        if (!fequal(len, 0.0)) {
            return *this / len;
        }
        return Vector3();
    }
    String asText() const {
        std::ostringstream os;
        os << *this;
        return os.str();
    }
    Vector3 abs() const {
        return Vector3(de::abs(Vector2<Type>::x), de::abs(Vector2<Type>::y), de::abs(z));
    }
    ddouble dot(const Vector3 &other) const {
        return Vector2<Type>::x * other.x + Vector2<Type>::y * other.y + z * other.z;
    }
    inline Vector3 cross(const Vector3 &other) const {
        return Vector3(Vector2<Type>::y * other.z - z * other.y,
                       z * other.x - Vector2<Type>::x * other.z,
                       Vector2<Type>::x * other.y - Vector2<Type>::y * other.x);
    }
    Vector3 min(const Vector3 &other) const {
        return Vector3(de::min(Vector2<Type>::x, other.x), de::min(Vector2<Type>::y, other.y),
            de::min(z, other.z));
    }
    Vector3 max(const Vector3 &other) const {
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
inline void operator << (const Writer &to, Vector3<Type> const &vec3) {
    Writer w(to);
    w << vec3.x << vec3.y << vec3.z;
}

template <typename Type>
inline Reader &operator >> (Reader &from, Vector3<Type> &vec3) {
    from >> vec3.x >> vec3.y >> vec3.z;
    return from;
}

template <typename Type>
inline void operator >> (const Reader &from, Vector3<Type> &vec3) {
    Reader r(from);
    r >> vec3.x >> vec3.y >> vec3.z;
}

template <typename Type>
std::ostream &operator << (std::ostream &os, Vector3<Type> const &vec3)
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
    constexpr Vector4(Type xyzw = Type(0)) : Vector3<Type>(xyzw, xyzw, xyzw), w(xyzw) {}
    constexpr Vector4(Type a, Type b, Type c, Type d) : Vector3<Type>(a, b, c), w(d) {}
    constexpr Vector4(Vector3<Type> const &v3, Type d = Type(0)) : Vector3<Type>(v3), w(d) {}
    constexpr Vector4(Vector2<Type> const &a, Type c = Type(0), Type d = Type(0)) : Vector3<Type>(a, c), w(d) {}
    constexpr Vector4(Vector2<Type> const &a, Vector2<Type> const &b) : Vector3<Type>(a, b.x), w(b.y) {}
    explicit Vector4(const Type *abcd) : Vector3<Type>(abcd), w(abcd[3]) {}
    Vector4(const Value &value4) { *this = vectorFromValue<Vector4<Type>>(value4); }
    Vector4(const Value &value3, Type d) { *this = Vector4<Type>(vectorFromValue<Vector3<Type>>(value3), d); }
    Vector4(const Vector4 &other) = default;

    /// Implicit conversion operator to a float vector.
    operator Vector4<dfloat> () const {
        return Vector4<dfloat>(Vector3<Type>::x, Vector3<Type>::y, Vector3<Type>::z, w);
    }
    /// Implicit conversion operator to a double vector.
    operator Vector4<ddouble> () const {
        return Vector4<ddouble>(Vector3<Type>::x, Vector3<Type>::y, Vector3<Type>::z, w);
    }
    Vector4<dint> toVec4i() const {
        return Vector4<dint>(dint(Vector3<Type>::x), dint(Vector3<Type>::y), dint(Vector3<Type>::z), dint(w));
    }
    Vector4<dbyte> toVec4ub() const {
        return Vector4<dbyte>(dbyte(Vector3<Type>::x), dbyte(Vector3<Type>::y), dbyte(Vector3<Type>::z), dbyte(w));
    }
    Vector4<dfloat> toVec4f() const {
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
        DE_ASSERT(index >= 0 && index <= 3);
        if (index >= 0 && index < 4)
        {
            return (&(this->Vector2<Type>::x))[index];
        }
        throw Error("Vector4::operator []", "Illegal index " + String::asText(index));
    }
    const Type &operator [] (int index) const {
        return const_cast<Vector4<Type> &>(*this)[index];
    }
    Vector4 operator + (const Vector4 &other) const {
        return Vector4(Vector3<Type>::x + other.x, Vector3<Type>::y + other.y,
            Vector3<Type>::z + other.z, w + other.w);
    }
    Vector4 operator - (const Vector4 &other) const {
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
    Vector4 operator * (const Vector4 &other) const {
        return Vector4(Vector3<Type>::x * other.x, Vector3<Type>::y * other.y,
            Vector3<Type>::z * other.z, w * other.w);
    }
    Vector4 &operator += (const Vector4 &other) {
        Vector3<Type>::x += other.x;
        Vector3<Type>::y += other.y;
        Vector3<Type>::z += other.z;
        w += other.w;
        return *this;
    }
    Vector4 &operator -= (const Vector4 &other) {
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
    inline Vector4 &operator *= (const Vector4 &other) {
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
    inline bool operator == (const Vector4 &other) const {
        return Vector3<Type>::operator == (other) && numberEqual(w, other.w);
    }
    inline bool operator != (const Vector4 &other) const {
        return !(*this == other);
    }
    bool operator > (const Vector4 &other) const {
        if (Vector3<Type>::operator == (other)) return w > other.w;
        return Vector3<Type>::operator > (other) && w > other.w;
    }
    bool operator < (const Vector4 &other) const {
        if (Vector3<Type>::operator == (other)) return w < other.w;
        return Vector3<Type>::operator < (other) && w < other.w;
    }
    bool operator >= (const Vector4 &other) const {
        return *this == other || *this > other;
    }
    bool operator <= (const Vector4 &other) const {
        return *this == other || *this < other;
    }
    String asText() const {
        std::ostringstream os;
        os << *this;
        return os.str();
    }
    Vector4 abs() const {
        return Vector4(de::abs(Vector3<Type>::x), de::abs(Vector3<Type>::y), de::abs(Vector3<Type>::z), de::abs(w));
    }
    ddouble dot(const Vector4 &other) const {
        return Vector3<Type>::x * other.x + Vector3<Type>::y * other.y
             + Vector3<Type>::z * other.z + w * other.w;
    }
    Vector4 min(const Vector4 &other) const {
        return Vector4(de::min(Vector3<Type>::x, other.x), de::min(Vector3<Type>::y, other.y),
            de::min(Vector3<Type>::z, other.z), de::min(w, other.w));
    }
    Vector4 max(const Vector4 &other) const {
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

    static Vector4 fromEuclidean(Vector2<Type> const &vec2) {
        return Vector4(vec2, Type(0), Type(1));
    }
    static Vector4 fromEuclidean(Vector3<Type> const &vec3) {
        return Vector4(vec3, Type(1));
    }
    Vector3<Type> toEuclidean() const {
        if (w != 0) {
            return Vector3<Type>(Vector2<Type>::x/w, Vector2<Type>::y/w, Vector3<Type>::z/w);
        }
        return {};
    }
    Vector2<Type> xy() const   { return *this; }
    Vector2<Type> zw() const   { return swizzle(*this, AxisZ, AxisW); }
    Vector3<Type> xyz() const  { return *this; }
    Vector4<Type> zyxw() const { return swizzle(*this, AxisZ, AxisY, AxisX, AxisW); }
    Vector4<Type> replaced(int index, const Type &value) const {
        Vector4 v = *this;
        v[index] = value;
        return v;
    }

public:
    Type w;
};

// Math.
template <typename Type>
Vector2<Type> floor(const Vector2<Type> &vec2) {
    return {Type(std::floor(vec2.x)), Type(std::floor(vec2.y))};
}

template <typename Type>
Vector3<Type> floor(const Vector3<Type> &vec3) {
    return {Type(std::floor(vec3.x)), Type(std::floor(vec3.y)), Type(std::floor(vec3.z))};
}

template <typename Type>
Vector4<Type> floor(const Vector4<Type> &vec4) {
    return {Type(std::floor(vec4.x)), Type(std::floor(vec4.y)), Type(std::floor(vec4.z)), Type(std::floor(vec4.w))};
}

template <typename Type>
Vector2<Type> ceil(const Vector2<Type> &vec2) {
    return {Type(std::ceil(vec2.x)), Type(std::ceil(vec2.y))};
}

template <typename Type>
Vector3<Type> ceil(const Vector3<Type> &vec3) {
    return {Type(std::ceil(vec3.x)), Type(std::ceil(vec3.y)), Type(std::ceil(vec3.z))};
}

template <typename Type>
Vector4<Type> ceil(const Vector4<Type> &vec4) {
    return {Type(std::ceil(vec4.x)), Type(std::ceil(vec4.y)), Type(std::ceil(vec4.z)), Type(std::ceil(vec4.w))};
}

// Swizzling.
template <typename Type>
typename Type::value_type swizzledComponent(const Type &vec, SwizzleAxis axis) {
    if (axis >= 0) return vec[axis];
    return -vec[-axis - 1];
}

template <typename Type>
Vector2<typename Type::value_type> swizzle(const Type &vec, SwizzleAxis a, SwizzleAxis b) {
    return Vector2<typename Type::value_type>(swizzledComponent(vec, a), swizzledComponent(vec, b));
}

template <typename Type>
Vector3<typename Type::value_type> swizzle(const Type &vec, SwizzleAxis a, SwizzleAxis b, SwizzleAxis c) {
    return Vector3<typename Type::value_type>(
        swizzledComponent(vec, a), swizzledComponent(vec, b), swizzledComponent(vec, c));
}

template <typename Type>
Vector4<typename Type::value_type> swizzle(const Type &vec, SwizzleAxis a, SwizzleAxis b, SwizzleAxis c, SwizzleAxis d) {
    return Vector4<typename Type::value_type>(swizzledComponent(vec, a),
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
inline void operator << (const Writer &to, Vector4<Type> const &vec4) {
    Writer w(to);
    w << vec4.x << vec4.y << vec4.z << vec4.w;
}

template <typename Type>
inline Reader &operator >> (Reader &from, Vector4<Type> &vec4) {
    from >> vec4.x >> vec4.y >> vec4.z >> vec4.w;
    return from;
}

template <typename Type>
inline void operator >> (const Reader &from, Vector4<Type> &vec4) {
    Reader r(from);
    r >> vec4.x >> vec4.y >> vec4.z >> vec4.w;
}

template <typename Type>
std::ostream &operator << (std::ostream &os, Vector4<Type> const &vec4)
{
    os << "(" << vec4.x << ", " << vec4.y << ", " << vec4.z << ", " << vec4.w << ")";
    return os;
}

///@{
/// @ingroup types
typedef Vector2<dint>    Vec2i;  ///< 2-component vector of integer values.
typedef Vector2<duint>   Vec2ui; ///< 2-component vector of unsigned integer values.
typedef Vector2<dfloat>  Vec2f;  ///< 2-component vector of floating point values.
typedef Vector2<ddouble> Vec2d;  ///< 2-component vector of high-precision floating point values.
typedef Vector3<dbyte>   Vec3ub; ///< 3-component vector of unsigned byte values.
typedef Vector3<dint>    Vec3i;  ///< 3-component vector of integer values.
typedef Vector3<duint>   Vec3ui; ///< 3-component vector of unsigned integer values.
typedef Vector3<dfloat>  Vec3f;  ///< 3-component vector of floating point values.
typedef Vector3<ddouble> Vec3d;  ///< 3-component vector of high-precision floating point values.
typedef Vector4<dbyte>   Vec4ub; ///< 4-component vector of unsigned byte values.
typedef Vector4<dint>    Vec4i;  ///< 4-component vector of integer values.
typedef Vector4<duint>   Vec4ui; ///< 4-component vector of unsigned integer values.
typedef Vector4<dfloat>  Vec4f;  ///< 4-component vector of floating point values.
typedef Vector4<ddouble> Vec4d;  ///< 4-component vector of high-precision floating point values.
///@}

DE_PUBLIC Vec2i ratio(const Vec2i &);

} // namespace de

#endif /* LIBCORE_VECTOR_H */
