/**\file r_util.h
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2012 Daniel Swanson <danij@dengine.net>
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

/**
 * Refresh Utility Routines
 */

#ifndef LIBDENG_REFRESH_UTIL_H
#define LIBDENG_REFRESH_UTIL_H

#include <de/vector1.h>
#include "map/p_mapdata.h"

/**
 * Which side of the partition does the point lie?
 *
 * @param x  X coordinate to test.
 * @param y  Y coordinate to test.
 * @return int  @c 0 = front, else @c 1 = back.
 */
int Partition_PointOnSide(const partition_t* par, coord_t const point[2]);
int Partition_PointXYOnSide(const partition_t* par, coord_t x, coord_t y);

/**
 * Get a global angle from Cartesian coordinates relative to the viewer.
 *
 * @param x   X coordinate to test.
 * @param y   Y coordinate to test.
 *
 * @return  Angle between the test point and view x,y.
 */
angle_t R_ViewPointXYToAngle(coord_t x, coord_t y);

/**
 * Determine distance to the specified point relative to the viewer.
 *
 * @param x   X coordinate to test.
 * @param y   Y coordinate to test.
 *
 * @return  Distance from the viewer to the test point.
 */
coord_t R_ViewPointXYDistance(coord_t x, coord_t y);

void R_ProjectViewRelativeLine2D(coord_t const center[2], boolean alignToViewPlane,
    coord_t width, coord_t offset, coord_t start[2], coord_t end[2]);

/**
 * Scale @a color uniformly so that the highest component becomes one.
 */
void R_AmplifyColor(float color[3]);

void R_ScaleAmbientRGB(float* out, const float* in, float mul);

void R_HSVToRGB(float* rgb, float h, float s, float v);

/**
 * Generate texcoords on the surface centered on point.
 *
 * @param s  Texture s coords written back here.
 * @param t  Texture t coords written back here.
 * @param point  Point on surface around which texture is centered.
 * @param xScale  Scale multiplier on the horizontal axis.
 * @param yScale  Scale multiplier on the vertical axis.
 * @param v1  Top left vertex of the surface being projected on.
 * @param v2  Bottom right vertex of the surface being projected on.
 * @param tangent  Tangent vector of the surface being projected on.
 * @param bitangent  Bitangent vector of the surface being projected on.
 *
 * @return  @c true if the generated coords are within bounds.
 */
boolean R_GenerateTexCoords(pvec2f_t s, pvec2f_t t, const_pvec3d_t point, float xScale, float yScale,
    const_pvec3d_t v1, const_pvec3d_t v2, const_pvec3f_t tangent, const_pvec3f_t bitangent);

/**
 * Choose an alignment mode and/or calculate the appropriate scaling factor
 * for fitting an element within the bounds of the "available" region.
 * The aspect ratio of the element is respected.
 *
 * @param scale  If not @c NULL the calculated scale factor is written here.
 * @param width  Width of the element to fit into the available region.
 * @param height  Height of the element to fit into the available region.
 * @param availWidth  Width of the available region.
 * @param availHeight  Height of the available region.
 * @param scaleMode  @ref scaleModes
 *
 * @return  @c true if aligning to the horizontal axis else the vertical.
 */
boolean R_ChooseAlignModeAndScaleFactor(float* scale, int width, int height,
    int availWidth, int availHeight, scalemode_t scaleMode);

/**
 * Choose a scale mode by comparing the dimensions of the two, two-dimensional
 * regions. The aspect ratio is respected when fitting to the bounds of the
 * "available" region.
 *
 * @param width  Width of the element to fit into the available region.
 * @param height  Height of the element to fit into the available region.
 * @param availWidth  Width of the available region.
 * @param availHeight  Height of the available region.
 * @param overrideMode  Scale mode override, for caller-convenience. @ref scaleModes
 * @param stretchEpsilon  Range within which aspect ratios are considered
 *      identical for "smart stretching".
 *
 * @return  Chosen scale mode @ref scaleModes.
 */
scalemode_t R_ChooseScaleMode2(int width, int height, int availWidth, int availHeight,
    scalemode_t overrideMode, float stretchEpsilon);
scalemode_t R_ChooseScaleMode(int width, int height, int availWidth, int availHeight,
    scalemode_t overrideMode);

#endif /* LIBDENG_REFRESH_UTIL_H */
