/** @file r_util.cpp  Refresh Utility Routines.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2015 Daniel Swanson <danij@dengine.net>
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

#include "doomsday/r_util.h"
#include "dd_share.h"

#include <de/legacy/binangle.h>
#include <de/legacy/vector1.h>
#include <cmath>

using namespace de;

float R_MovementYaw(float const mom[])
{
    return radianToDegree(atan2f(-mom[MY], mom[MX]));
}

float R_MovementXYYaw(float momx, float momy)
{
    float mom[2] = { momx, momy };
    return R_MovementYaw(mom);
}

float R_MovementPitch(float const mom[])
{
    return radianToDegree(atan2f(1.2f * mom[MZ], V2f_Length(mom)));
}

float R_MovementXYZPitch(float momx, float momy, float momz)
{
    float mom[3] = { momx, momy, momz };
    return R_MovementPitch(mom);
}

Vec3d R_ClosestPointOnPlane(const Vec3f &planeNormal_,
                            const Vec3d &planePoint_, const Vec3d &origin_)
{
    vec3f_t planeNormal; V3f_Set(planeNormal, planeNormal_.x, planeNormal_.y, planeNormal_.z);
    vec3d_t planePoint; V3d_Set(planePoint, planePoint_.x, planePoint_.y, planePoint_.z);
    vec3d_t origin; V3d_Set(origin, origin_.x, origin_.y, origin_.z);
    vec3d_t point; V3d_ClosestPointOnPlanef(point, planeNormal, planePoint, origin);
    return Vec3d(point);
}

void R_AmplifyColor(de::Vec3f &rgb)
{
    float max = 0;

    for(int i = 0; i < 3; ++i)
    {
        if(rgb[i] > max)
            max = rgb[i];
    }
    if(!max || max == 1) return;

    for(int i = 0; i < 3; ++i)
    {
        rgb[i] = rgb[i] / max;
    }
}

void R_ScaleAmbientRGB(float *out, const float *in, float mul)
{
    mul = de::clamp(0.f, mul, 1.f);
    for(int i = 0; i < 3; ++i)
    {
        float val = in[i] * mul;
        if(out[i] < val)
            out[i] = val;
    }
}
