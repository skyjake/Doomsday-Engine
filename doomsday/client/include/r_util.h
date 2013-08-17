/** @file r_util.h Refresh Utility Routines.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#ifndef DENG_REFRESH_UTIL_H
#define DENG_REFRESH_UTIL_H

#include "api_gl.h"

#include <de/Matrix>
#include <de/Vector>

/**
 * Get a global angle from Cartesian coordinates relative to the viewer.
 *
 * @param x   X coordinate to test.
 * @param y   Y coordinate to test.
 *
 * @return  Angle between the test point and view x,y.
 */
angle_t R_ViewPointToAngle(coord_t x, coord_t y);

/**
 * Determine distance to the specified point relative to the viewer.
 *
 * @param x   X coordinate to test.
 * @param y   Y coordinate to test.
 *
 * @return  Distance from the viewer to the test point.
 */
coord_t R_ViewPointDistance(coord_t x, coord_t y);

de::Vector3d R_ClosestPointOnPlane(de::Vector3f const &planeNormal,
    de::Vector3d const &planePoint, de::Vector3d const &origin);

void R_ProjectViewRelativeLine2D(coord_t const center[2], boolean alignToViewPlane,
    coord_t width, coord_t offset, coord_t start[2], coord_t end[2]);

/**
 * Scale @a color uniformly so that the highest component becomes one.
 */
void R_AmplifyColor(de::Vector3f &color);

void R_ScaleAmbientRGB(float *out, float const *in, float mul);

/**
 * Generate texcoords on the surface centered on point.
 *
 * @param s              Texture s coords written back here.
 * @param t              Texture t coords written back here.
 * @param point          Point on surface around which texture is centered.
 * @param xScale         Scale multiplier on the horizontal axis.
 * @param yScale         Scale multiplier on the vertical axis.
 * @param v1             Top left vertex of the surface being projected on.
 * @param v2             Bottom right vertex of the surface being projected on.
 * @param tangentMatrix  Normalized tangent space matrix for the surface being projected to.
 *
 * @return  @c true if the generated coords are within bounds.
 */
bool R_GenerateTexCoords(de::Vector2f &s, de::Vector2f &t, de::Vector3d const &point,
    float xScale, float yScale, de::Vector3d const &v1, de::Vector3d const &v2,
    de::Matrix3f const &tangentMatrix);

char const *R_NameForBlendMode(blendmode_t mode);

#endif // DENG_REFRESH_UTIL_H
