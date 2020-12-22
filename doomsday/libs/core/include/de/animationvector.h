/** @file animationvector.h Vector whose components are Animation instances.
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

#ifndef LIBCORE_ANIMATIONVECTOR_H
#define LIBCORE_ANIMATIONVECTOR_H

#include "de/animation.h"
#include "de/vector.h"

namespace de {

/**
 * Animated 2D vector.
 * @ingroup math
 */
class DE_PUBLIC AnimationVector2
{
public:
    AnimationVector2(Animation::Style style = Animation::EaseOut)
        : x(0, style), y(0, style) {}

    AnimationVector2 &operator = (const Vec2f &vector)
    {
        setValue(vector);
        return *this;
    }

    void setValue(const Vec2f &vector, TimeSpan transitionSpan = 0.0)
    {
        x.setValue(vector.x, transitionSpan);
        y.setValue(vector.y, transitionSpan);
    }

    void setValueIfDifferentTarget(const Vec2f &vector, TimeSpan transitionSpan = 0.0)
    {
        if (!fequal(x.target(), vector.x))
        {
            x.setValue(vector.x, transitionSpan);
        }
        if (!fequal(y.target(), vector.y))
        {
            y.setValue(vector.y, transitionSpan);
        }
    }

    void setStyle(Animation::Style s)
    {
        x.setStyle(s);
        y.setStyle(s);
    }

    Vec2f value() const
    {
        return Vec2f(x, y);
    }

    Vec2f target() const
    {
        return Vec2f(x.target(), y.target());
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
class DE_PUBLIC AnimationVector3
{
public:
    AnimationVector3(Animation::Style style = Animation::EaseOut)
        : x(0, style), y(0, style), z(0, style) {}

    AnimationVector3 &operator = (const Vec3f &vector)
    {
        setValue(vector);
        return *this;
    }

    void setValue(const Vec3f &vector, float transitionSpan = 0.f)
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

    Vec3f value() const
    {
        return Vec3f(x, y, z);
    }

    Vec3f target() const
    {
        return Vec3f(x.target(), y.target(), z.target());
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

#endif // LIBCORE_ANIMATIONVECTOR_H
