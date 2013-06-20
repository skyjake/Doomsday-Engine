/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2009-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef LIBDENG2_RECTANGLE_H
#define LIBDENG2_RECTANGLE_H

#include "../Vector"
#include "math.h"
#include <QRect>

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
    Rectangle(Corner const &tl, Corner const &br) : topLeft(tl), bottomRight(br) {}
    static RectangleType fromSize(Size const &size) {
        return RectangleType(0, 0, size.x, size.y);
    }
    static RectangleType fromSize(Corner const &tl, Size const &size) {
        return RectangleType(tl.x, tl.y, size.x, size.y);
    }
    static RectangleType fromQRect(QRect const &qr) {
        return RectangleType(qr.left(), qr.top(), qr.width(), qr.height());
    }    

    // Automatic conversion to a Rectanglef.
    operator Rectangle<Vector2f, Vector2f> () const {
        return Rectangle<Vector2f, Vector2f>(topLeft.x, topLeft.y, width(), height());
    }

    SizeType width() const { return abs(bottomRight.x - topLeft.x); }
    SizeType height() const { return abs(bottomRight.y - topLeft.y); }
    SizeType area() const { return width() * height(); }
    bool isNull() const { return area() == 0; }
    Size size() const { return Size(width(), height()); }
    void moveLeft(Type const &value) {
        bottomRight.x = value + width();
        topLeft.x = value;
    }
    void moveTop(Type const &value) {
        bottomRight.y = value + height();
        topLeft.y = value;
    }
    void moveTopLeft(CornerVectorType const &point) {
        Size s = size();
        topLeft = point;
        bottomRight.x = point.x + s.x;
        bottomRight.y = point.y + s.y;
    }
    RectangleType &move(CornerVectorType const &delta) {
        topLeft     += delta;
        bottomRight += delta;
        return *this;
    }
    RectangleType moved(CornerVectorType const &delta) const {
        return RectangleType(topLeft + delta, bottomRight + delta);
    }
    void setWidth(SizeType w) { bottomRight.x = topLeft.x + w; }
    void setHeight(SizeType h) { bottomRight.y = topLeft.y + h; }
    void setSize(SizeVectorType const &s) { setWidth(s.x); setHeight(s.y); }
    void include(Corner const &point) {
        topLeft = topLeft.min(point);
        bottomRight = bottomRight.max(point);
    }
    RectangleType expanded(Type n) const {
        return RectangleType(topLeft - Corner(n, n), bottomRight + Corner(n, n));
    }
    RectangleType shrunk(Type n) const {
        return RectangleType(topLeft + Corner(n, n), bottomRight - Corner(n, n));
    }
    RectangleType adjusted(CornerVectorType const &tl, CornerVectorType const &br) const {
        return RectangleType(topLeft + tl, bottomRight + br);
    }
    Rectangle<Vector2ui, Vector2ui> toRectangleui() const {
        Vector2ui tl(duint(de::max(0, topLeft.x)),     duint(de::max(0, topLeft.y)));
        Vector2ui br(duint(de::max(0, bottomRight.x)), duint(de::max(0, bottomRight.y)));
        return Rectangle<Vector2ui, Vector2ui>(tl, br);
    }
    bool contains(Corner const &point) const {
        return point >= topLeft && point <= bottomRight;
    }
    bool contains(RectangleType const &other) const {
        return contains(other.topLeft) && contains(other.bottomRight);
    }
    bool operator == (RectangleType const &other) const {
        return topLeft == other.topLeft && bottomRight == other.bottomRight;
    }
    bool operator != (RectangleType const &other) const {
        return !(*this == other);
    }
    RectangleType operator | (RectangleType const &other) const {
        return RectangleType(topLeft.min(other.topLeft),
                             bottomRight.max(other.bottomRight));
    }
    RectangleType &operator |= (RectangleType const &other) {
        topLeft     = topLeft.min(other.topLeft);
        bottomRight = bottomRight.max(other.bottomRight);
        return *this;
    }
    RectangleType operator & (RectangleType const &other) const {
        if(other.topLeft.x >= bottomRight.x ||
           other.topLeft.y >= bottomRight.y ||
           other.bottomRight.x <= topLeft.x ||
           other.bottomRight.y <= topLeft.y) return RectangleType(); // disconnected

        return RectangleType(topLeft.max(other.topLeft),
                             bottomRight.min(other.bottomRight));
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

public:
    Corner topLeft;
    Corner bottomRight;
};

// Common types.
typedef Rectangle<Vector2i,  Vector2ui> Rectanglei;
typedef Rectangle<Vector2ui, Vector2ui> Rectangleui;
typedef Rectangle<Vector2f,  Vector2f>  Rectanglef;
typedef Rectangle<Vector2d,  Vector2d>  Rectangled;

} // namespace de

#endif /* LIBDENG2_RECTANGLE_H */
