/*
 * The Doomsday Engine Project -- libcore
 *
 * Copyright © 2009-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBCORE_RECTANGLE_H
#define LIBCORE_RECTANGLE_H

#include "de/vector.h"
#include "de/range.h"
#include "math.h"

#ifdef WIN32
#  undef min
#  undef max
#endif

namespace de {

/**
 * Template for 2D rectangles. The members are public for convenient access.
 *
 * @ingroup math
 */
template <typename CornerVectorType, typename SizeVectorType>
class Rectangle
{
public:
    typedef Rectangle<CornerVectorType, SizeVectorType> RectangleType;
    typedef CornerVectorType Corner;
    typedef SizeVectorType Size;
    typedef typename Corner::ValueType Type;
    typedef typename Size::ValueType SizeType;

public:
    Rectangle() {}
    Rectangle(Type left, Type top, SizeType width, SizeType height)
        : topLeft(left, top), bottomRight(left + width, top + height) {}
    Rectangle(const Corner &tl, const Corner &br) : topLeft(tl), bottomRight(br) {}
    static RectangleType fromSize(const Size &size) {
        return RectangleType(0, 0, size.x, size.y);
    }
    static RectangleType fromSize(const Corner &tl, const Size &size) {
        return RectangleType(tl.x, tl.y, size.x, size.y);
    }
//    static RectangleType fromQRect(const QRect &qr) {
//        return RectangleType(qr.left(), qr.top(), qr.width(), qr.height());
//    }

    // Automatic conversion to a Rectanglef.
    operator Rectangle<Vec2f, Vec2f> () const {
        return Rectangle<Vec2f, Vec2f>(topLeft.x, topLeft.y, width(), height());
    }

    SizeType width() const { return abs(bottomRight.x - topLeft.x); }
    SizeType height() const { return abs(bottomRight.y - topLeft.y); }
    SizeType area() const { return width() * height(); }
    bool isNull() const { return area() == 0; }
    Size size() const { return Size(width(), height()); }
    void moveLeft(const Type &value) {
        bottomRight.x = value + width();
        topLeft.x = value;
    }
    void moveTop(const Type &value) {
        bottomRight.y = value + height();
        topLeft.y = value;
    }
    void moveTopLeft(const CornerVectorType &point) {
        Size s = size();
        topLeft = point;
        bottomRight.x = point.x + s.x;
        bottomRight.y = point.y + s.y;
    }
    RectangleType &move(const CornerVectorType &delta) {
        topLeft     += delta;
        bottomRight += delta;
        return *this;
    }
    RectangleType moved(const CornerVectorType &delta) const {
        return RectangleType(topLeft + delta, bottomRight + delta);
    }
    void setWidth(SizeType w) { bottomRight.x = topLeft.x + w; }
    void setHeight(SizeType h) { bottomRight.y = topLeft.y + h; }
    void setSize(const SizeVectorType &s) { setWidth(s.x); setHeight(s.y); }
    void include(const Corner &point) {
        topLeft = topLeft.min(point);
        bottomRight = bottomRight.max(point);
    }
    RectangleType expanded(Type n) const {
        return RectangleType(topLeft - Corner(n, n), bottomRight + Corner(n, n));
    }
    RectangleType expanded(Corner delta) const {
        return RectangleType(topLeft - delta, bottomRight + delta);
    }
    RectangleType shrunk(Type n) const {
        return RectangleType(topLeft + Corner(n, n), bottomRight - Corner(n, n));
    }
    RectangleType shrunk(Corner delta) const {
        return RectangleType(topLeft + delta, bottomRight - delta);
    }
    RectangleType adjusted(const CornerVectorType &tl, const CornerVectorType &br) const {
        return RectangleType(topLeft + tl, bottomRight + br);
    }
    Rectangle<Vec2i, Vec2ui> toRectanglei() const {
        return Rectangle<Vec2i, Vec2ui>(topLeft.toVec2i(), bottomRight.toVec2i());
    }
    Rectangle<Vec2ui, Vec2ui> toRectangleui() const {
        Vec2ui tl(duint(de::max(Type(0), topLeft.x)),     duint(de::max(Type(0), topLeft.y)));
        Vec2ui br(duint(de::max(Type(0), bottomRight.x)), duint(de::max(Type(0), bottomRight.y)));
        return Rectangle<Vec2ui, Vec2ui>(tl, br);
    }
    bool contains(const Corner &point) const {
        return point >= topLeft && point < bottomRight;
    }
    bool contains(const RectangleType &other) const {
        return contains(other.topLeft) && contains(other.bottomRight);
    }
    bool operator == (const RectangleType &other) const {
        return topLeft == other.topLeft && bottomRight == other.bottomRight;
    }
    bool operator != (const RectangleType &other) const {
        return !(*this == other);
    }
    RectangleType operator | (const RectangleType &other) const {
        if (isNull()) return other;
        if (other.isNull()) return *this;
        return {topLeft.min(other.topLeft), bottomRight.max(other.bottomRight)};
    }
    RectangleType &operator |= (const RectangleType &other) {
        return *this = *this | other;
    }
    inline bool overlaps(const RectangleType &other) const {
        return !(other.topLeft.x >= bottomRight.x ||
                 other.topLeft.y >= bottomRight.y ||
                 other.bottomRight.x <= topLeft.x ||
                 other.bottomRight.y <= topLeft.y);
    }
    RectangleType operator & (const RectangleType &other) const {
        if (!overlaps(other)) return RectangleType(); // disconnected
        return RectangleType(topLeft.max(other.topLeft),
                             bottomRight.min(other.bottomRight));
    }
    RectangleType &operator &= (const RectangleType &other) {
        return (*this = *this & other);
    }
    String asText() const {
        return "[" + topLeft.asText() + "->" + bottomRight.asText() +
                " size:" + size().asText() + "]";
    }
    Type left() const { return topLeft.x; }
    Type right() const { return bottomRight.x; }
    Type top() const { return topLeft.y; }
    Type bottom() const { return bottomRight.y; }
    Corner topRight() const {
        return Corner(bottomRight.x, topLeft.y);
    }
    Corner bottomLeft() const {
        return Corner(topLeft.x, bottomRight.y);
    }
    Corner midLeft() const {
        return Corner(topLeft.x, (topLeft.y + bottomRight.y)/2.0);
    }
    Corner midRight() const {
        return Corner(bottomRight.x, (topLeft.y + bottomRight.y)/2.0);
    }
    Corner midTop() const {
        return Corner((topLeft.x + bottomRight.x)/2.0, topLeft.y);
    }
    Corner midBottom() const {
        return Corner((topLeft.x + bottomRight.x)/2.0, bottomRight.y);
    }
    Corner middle() const {
        return Corner((topLeft.x + bottomRight.x)/2.0, (topLeft.y + bottomRight.y)/2.0);
    }
    Vector4<Type> xywh() const {
        return Vector4<Type>(topLeft.x, topLeft.y, Type(width()), Type(height()));
    }
    Range<Type> horizontal() const {
        return Range<Type>(left(), right());
    }
    Range<Type> vertical() const {
        return Range<Type>(top(), bottom());
    }
    RectangleType normalized() const {
        Corner a = topLeft;
        Corner b = bottomRight;
        if (a.x > b.x) {
            std::swap(a.x, b.x);
        }
        if (a.y > b.y) {
            std::swap(a.y, b.y);
        }
        return {a, b};
    }

public:
    Corner topLeft;
    Corner bottomRight;
};

// Common types.
typedef Rectangle<Vec2i,  Vec2ui> Rectanglei;
typedef Rectangle<Vec2ui, Vec2ui> Rectangleui;
typedef Rectangle<Vec2f,  Vec2f>  Rectanglef;
typedef Rectangle<Vec2d,  Vec2d>  Rectangled;

} // namespace de

#endif /* LIBCORE_RECTANGLE_H */
