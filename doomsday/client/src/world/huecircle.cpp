/** @file huecircle.cpp HueCircle manipulator, for runtime map editing.
 *
 * @authors Copyright © 2006-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
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

#include <cmath>

#include "de_base.h"
#include "m_misc.h" // M_HSVToRGB(), remove me (use QColor)

#include "world/huecircle.h"

using namespace de;

DENG2_PIMPL_NOREF(HueCircle)
{
    Vector3f frontVec, sideVec, upVec;
    Instance() {}
};

HueCircle::HueCircle() : d(new Instance())
{}

Vector3d HueCircle::origin(Vector3d const &viewOrigin, double distance) const
{
    return viewOrigin + d->frontVec * distance;
}

void HueCircle::setOrientation(Vector3f const &frontVec, Vector3f const &sideVec,
                               Vector3f const &upVec)
{
    d->frontVec = frontVec;
    d->sideVec  = sideVec;
    d->upVec    = upVec;
}

Vector3f HueCircle::colorAt(Vector3f const &viewFrontVec, float *angle, float *sat) const
{
    float const minAngle = 0.1f;
    float const range    = 0.19f;

    float viewDot = viewFrontVec.dot(d->frontVec);

    float saturation = (acos(viewDot) - minAngle) / range;
    saturation = de::clamp(0.f, saturation, 1.f);

    if(sat) *sat = saturation;

    if(saturation == 0 || viewDot > .999f)
    {
        if(angle) *angle = 0;
        if(sat) *sat = 0;

        float colorV1[3]; M_HSVToRGB(colorV1, 0, 0, 1);
        return Vector3f(colorV1);
    }

    // Calculate the angle to the viewer by projecting the current viewfront to the
    // hue circle plane.  Project onto the normal, subtract and then normalize.
    float scale = viewFrontVec.dot(d->frontVec) / d->frontVec.dot(d->frontVec);

    Vector3f proj = (viewFrontVec - (d->frontVec * scale)).normalize();

    float hue = acos(proj.dot(d->upVec));

    if(proj.dot(d->sideVec) > 0)
        hue = float(2 * de::PI) - hue;

    hue /= float(2 * de::PI);
    hue += 0.25;

    if(angle) *angle = hue;

    float colorV1[3]; M_HSVToRGB(colorV1, hue, saturation, 1);
    return Vector3f(colorV1);
}

Vector3f HueCircle::offset(double angle) const
{
    return Vector3f(cos(angle) * d->sideVec.x + sin(angle) * d->upVec.x,
                    sin(angle) * d->upVec.y,
                    cos(angle) * d->sideVec.z + sin(angle) * d->upVec.z);
}
