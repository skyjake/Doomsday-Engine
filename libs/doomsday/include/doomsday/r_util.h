/** @file r_util.h  Refresh Utility Routines.
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

#pragma once

#include "libdoomsday.h"
#include <de/vector.h>

LIBDOOMSDAY_PUBLIC float R_MovementYaw(float const mom[2]);
LIBDOOMSDAY_PUBLIC float R_MovementXYYaw(float momx, float momy);

LIBDOOMSDAY_PUBLIC float R_MovementPitch(float const mom[3]);
LIBDOOMSDAY_PUBLIC float R_MovementXYZPitch(float momx, float momy, float momz);

LIBDOOMSDAY_PUBLIC de::Vec3d R_ClosestPointOnPlane(const de::Vec3f &planeNormal,
                                                   const de::Vec3d &planePoint,
                                                   const de::Vec3d &origin);

/**
 * Scale @a color uniformly so that the highest component becomes one.
 */
LIBDOOMSDAY_PUBLIC void R_AmplifyColor(de::Vec3f &color);

LIBDOOMSDAY_PUBLIC void R_ScaleAmbientRGB(float *out, const float *in, float mul);
