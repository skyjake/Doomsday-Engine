/** @file vector.h Vector templates.
 *
 * @authors Copyright © 2009-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2013 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG2_VECTOR_H
#define LIBDENG2_VECTOR_H

#include "math.h"
#include "Error"
#include "ISerializable"
#include "Writer"
#include "Reader"
#include "String"

#include <QTextStream>
#include <cmath>

#ifdef WIN32
#  undef min
#  undef max
#endif

namespace de {

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
    Vector2(Type a = 0, Type b = 0) : x(a), y(b) {}
    Vector2(Type const *ab) : x(ab[0]), y(ab[1]) {}

    /// Conversion operator to a float vector.
    operator Vector2<dfloat> () const {
        return Vector2<dfloat>(dfloat(x), dfloat(y));
    }
    /// Conversion operator to a double vector.
    operator Vector2<ddouble> () const {
        return Vector2<ddouble>(ddouble(x), ddouble(y));
    }
    Type &operator [] (int index) {
        DENG2_ASSERT(index >= 0 && index <= 1);
        Type *ptrs[] = { &x, &y };
        if(index >= 0 && index < 2)
        {
            return *ptrs[index];
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
    Vector2 operator * (ddouble scalar) const {
        return Vector2(Type(x * scalar), Type(y * scalar));
    }
    Vector2 operator * (Vector2 const &other) const {
        return Vector2(x * other.x, y * other.y);
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
    inline Vector2 &operator /= (ddouble scalar) {
        return (*this) *= 1.0 / scalar;
    }
    bool operator > (Vector2 const &other) const {
        return x > other.x && y > other.y;
    }
    bool operator < (Vector2 const &other) const {
        return x < other.x && y < other.y;
    }
    bool operator >= (Vector2 const &other) const {
        return x >= other.x && y >= other.y;
    }
    bool operator <= (Vector2 const &other) const {
        return x <= other.x && y <= other.y;
    }
    ddouble length() const {
        return std::sqrt(ddouble(x*x + y*y));
    }
    Vector2 normalize() const {
        ddouble const len = length();
        if(len != 0) {
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
    ddouble dot(Vector2 const &other) const {
        return x * other.x + y * other.y;
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

    /// Conversion operator to a float vector.
    operator Vector3<dfloat> () const {
        return Vector3<dfloat>(Vector2<Type>::x, Vector2<Type>::y, z);
    }
    /// Conversion operator to a double vector.
    operator Vector3<ddouble> () const {
        return Vector3<ddouble>(Vector2<Type>::x, Vector2<Type>::y, z);
    }
    Type &operator [] (int index) {
        DENG2_ASSERT(index >= 0 && index <= 2);
        Type *ptrs[] = { &(this->Vector2<Type>::x),
                         &(this->Vector2<Type>::y),
                         &z };
        if(index >= 0 && index < 3)
        {
            return *ptrs[index];
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
    inline Vector3 &operator /= (ddouble scalar) {
        return (*this) *= 1.0 / scalar;
    }
    bool operator > (Vector3 const &other) const {
        return Vector2<Type>::operator > (other) && z > other.z;
    }
    bool operator < (Vector3 const &other) const {
        return Vector2<Type>::operator < (other) && z < other.z;
    }
    bool operator >= (Vector3 const &other) const {
        return Vector2<Type>::operator >= (other) && z >= other.z;
    }
    bool operator <= (Vector3 const &other) const {
        return Vector2<Type>::operator <= (other) && z <= other.z;
    }
    ddouble length() const {
        return std::sqrt(Vector2<Type>::x*Vector2<Type>::x + Vector2<Type>::y*Vector2<Type>::y + z*z);
    }
    Vector3 normalize() const {
        ddouble const len = length();
        if(len != 0) {
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
        if(vecAbs.y < vecAbs[axis]) axis = 1;
        if(vecAbs.x < vecAbs[axis]) axis = 0;
        return axis;
    }
    int maxAxis() const {
        Vector3 vecAbs = abs();
        int axis = 0;
        if(vecAbs.y > vecAbs[axis]) axis = 1;
        if(vecAbs.z > vecAbs[axis]) axis = 2;
        return axis;
    }

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
    Vector4(Type const *abcd) : Vector3<Type>(abcd), w(abcd[3]) {}

    /// Conversion operator to a float vector.
    operator Vector4<dfloat> () const {
        return Vector4<dfloat>(Vector3<Type>::x, Vector3<Type>::y, Vector3<Type>::z, w);
    }
    /// Conversion operator to a double vector.
    operator Vector4<ddouble> () const {
        return Vector4<ddouble>(Vector3<Type>::x, Vector3<Type>::y, Vector3<Type>::z, w);
    }
    Type &operator [] (int index) {
        DENG2_ASSERT(index >= 0 && index <= 3);
        Type *ptrs[] = { &(this->Vector2<Type>::x),
                         &(this->Vector2<Type>::y),
                         &(this->Vector3<Type>::z),
                         &w };
        if(index >= 0 && index < 4)
        {
            return *ptrs[index];
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
    inline Vector4 &operator /= (ddouble scalar) {
        return (*this) *= 1.0 / scalar;
    }
    bool operator > (Vector4 const &other) const {
        return Vector3<Type>::operator > (other) && w > other.w;
    }
    bool operator < (Vector4 const &other) const {
        return Vector3<Type>::operator < (other) && w < other.w;
    }
    bool operator >= (Vector4 const &other) const {
        return Vector3<Type>::operator >= (other) && w >= other.w;
    }
    bool operator <= (Vector4 const &other) const {
        return Vector3<Type>::operator <= (other) && w <= other.w;
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
        if(vecAbs.z < vecAbs[axis]) axis = 2;
        if(vecAbs.y < vecAbs[axis]) axis = 1;
        if(vecAbs.x < vecAbs[axis]) axis = 0;
        return axis;
    }
    int maxAxis() const {
        Vector4 vecAbs = abs();
        int axis = 0;
        if(vecAbs.y > vecAbs[axis]) axis = 1;
        if(vecAbs.z > vecAbs[axis]) axis = 2;
        if(vecAbs.w > vecAbs[axis]) axis = 3;
        return axis;
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
        if(w != 0)
        {
            return Vector3<Type>(Vector2<Type>::x/w, Vector2<Type>::y/w, Vector3<Type>::z/w);
        }
        return Vector3<Type>();
    }

public:
    Type w;
};

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

// Equality operators for integer types.
inline bool operator == (Vector2<dint> const &a, Vector2<dint> const &b)
{
    return a.x == b.x && a.y == b.y;
}

inline bool operator == (Vector3<dint> const &a, Vector3<dint> const &b)
{
    return a.x == b.x && a.y == b.y && a.z == b.z;
}

inline bool operator == (Vector4<dint> const &a, Vector4<dint> const &b)
{
    return a.x == b.x && a.y == b.y && a.z == b.z && a.w == b.w;
}

inline bool operator == (Vector2<duint> const &a, Vector2<duint> const &b)
{
    return a.x == b.x && a.y == b.y;
}

inline bool operator == (Vector3<duint> const &a, Vector3<duint> const &b)
{
    return a.x == b.x && a.y == b.y && a.z == b.z;
}

inline bool operator == (Vector4<duint> const &a, Vector4<duint> const &b)
{
    return a.x == b.x && a.y == b.y && a.z == b.z && a.w == b.w;
}

// Inequality operators for integer types.
inline bool operator != (Vector2<dint> const &a, Vector2<dint> const &b)
{
    return !(a == b);
}

inline bool operator != (Vector3<dint> const &a, Vector3<dint> const &b)
{
    return !(a == b);
}

inline bool operator != (Vector4<dint> const &a, Vector4<dint> const &b)
{
    return !(a == b);
}

inline bool operator != (Vector2<duint> const &a, Vector2<duint> const &b)
{
    return !(a == b);
}

inline bool operator != (Vector3<duint> const &a, Vector3<duint> const &b)
{
    return !(a == b);
}

inline bool operator != (Vector4<duint> const &a, Vector4<duint> const &b)
{
    return !(a == b);
}

// Equality operators for single-precision floating-point types.
inline bool operator == (Vector2<dfloat> const &a, Vector2<dfloat> const &b)
{
    return de::fequal(a.x, b.x) && de::fequal(a.y, b.y);
}

inline bool operator == (Vector3<dfloat> const &a, Vector3<dfloat> const &b)
{
    return de::fequal(a.x, b.x) && de::fequal(a.y, b.y) && de::fequal(a.z, b.z);
}

inline bool operator == (Vector4<dfloat> const &a, Vector4<dfloat> const &b)
{
    return de::fequal(a.x, b.x) && de::fequal(a.y, b.y) && de::fequal(a.z, b.z) && de::fequal(a.w, b.w);
}

// Equality operators for double-precision floating-point types.
inline bool operator == (Vector2<ddouble> const &a, Vector2<ddouble> const &b)
{
    return de::fequal(a.x, b.x) && de::fequal(a.y, b.y);
}

inline bool operator == (Vector3<ddouble> const &a, Vector3<ddouble> const &b)
{
    return de::fequal(a.x, b.x) && de::fequal(a.y, b.y) && de::fequal(a.z, b.z);
}

inline bool operator == (Vector4<ddouble> const &a, Vector4<ddouble> const &b)
{
    return de::fequal(a.x, b.x) && de::fequal(a.y, b.y) && de::fequal(a.z, b.z) && de::fequal(a.w, b.w);
}

// Inequality operators for single-precision floating-point types.
inline bool operator != (Vector2<dfloat> const &a, Vector2<dfloat> const &b)
{
    return !(a == b);
}

inline bool operator != (Vector3<dfloat> const &a, Vector3<dfloat> const &b)
{
    return !(a == b);
}

inline bool operator != (Vector4<dfloat> const &a, Vector4<dfloat> const &b)
{
    return !(a == b);
}

// Inequality operators for double-precision floating-point types.
inline bool operator != (Vector2<ddouble> const &a, Vector2<ddouble> const &b)
{
    return !(a == b);
}

inline bool operator != (Vector3<ddouble> const &a, Vector3<ddouble> const &b)
{
    return !(a == b);
}

inline bool operator != (Vector4<ddouble> const &a, Vector4<ddouble> const &b)
{
    return !(a == b);
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
typedef Vector4<dint>    Vector4i;  ///< 4-component vector of integer values.
typedef Vector4<duint>   Vector4ui; ///< 4-component vector of unsigned integer values.
typedef Vector4<dfloat>  Vector4f;  ///< 4-component vector of floating point values.
typedef Vector4<ddouble> Vector4d;  ///< 4-component vector of high-precision floating point values.
///@}

} // namespace de

#endif /* LIBDENG2_VECTOR_H */
