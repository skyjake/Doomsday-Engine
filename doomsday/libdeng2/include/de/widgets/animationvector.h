/** @file animationvector.h Vector whose components are Animation instances.
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
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#ifndef LIBDENG2_ANIMATIONVECTOR_H
#define LIBDENG2_ANIMATIONVECTOR_H

#include "../Animation"
#include "../Vector"

namespace de {

/**
 * Animated 2D vector.
 * @ingroup math
 */
class DENG2_PUBLIC AnimationVector2
{
public:
    AnimationVector2(Animation::Style style = Animation::EaseOut)
        : x(0, style), y(0, style) {}

    AnimationVector2 &operator = (Vector2f const &vector)
    {
        setValue(vector);
        return *this;
    }

    void setValue(Vector2f const &vector, float transitionSpan = 0.f)
    {
        x.setValue(vector.x, transitionSpan);
        y.setValue(vector.y, transitionSpan);
    }

    void setStyle(Animation::Style s)
    {
        x.setStyle(s);
        y.setStyle(s);
    }

    Vector2f value() const
    {
        return Vector2f(x, y);
    }

    Vector2f target() const
    {
        return Vector2f(x.target(), y.target());
    }

    bool done() const
    {
        return x.done() && y.done();
    }

public:
    Animation x;
    Animation y;
};

/**
 * Animated 3D vector.
 * @ingroup math
 */
class DENG2_PUBLIC AnimationVector3
{
public:
    AnimationVector3(Animation::Style style = Animation::EaseOut)
        : x(0, style), y(0, style), z(0, style) {}

    AnimationVector3 &operator = (Vector3f const &vector)
    {
        setValue(vector);
        return *this;
    }

    void setValue(Vector3f const &vector, float transitionSpan = 0.f)
    {
        x.setValue(vector.x, transitionSpan);
        y.setValue(vector.y, transitionSpan);
        z.setValue(vector.z, transitionSpan);
    }

    void setStyle(Animation::Style s)
    {
        x.setStyle(s);
        y.setStyle(s);
        z.setStyle(s);
    }

    Vector3f value() const
    {
        return Vector3f(x, y, z);
    }

    Vector3f target() const
    {
        return Vector3f(x.target(), y.target(), z.target());
    }

    bool done() const
    {
        return x.done() && y.done() && z.done();
    }

public:
    Animation x;
    Animation y;
    Animation z;
};

} // namespace de

#endif // LIBDENG2_ANIMATIONVECTOR_H
