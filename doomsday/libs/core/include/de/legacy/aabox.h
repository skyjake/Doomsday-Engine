/** @file aabox.h Axis-aligned bounding box.
 *
 * @authors Copyright &copy; 2006-2013 Daniel Swanson <danij@dengine.net>
 * @authors Copyright &copy; 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef DE_AABOX_H
#define DE_AABOX_H

#include "types.h"

/**
 * Axis-aligned bounding box with integer precision. Handy POD structure for
 * manipulation of bounding boxes.
 *
 * @ingroup legacyMath
 */
typedef struct aabox_s {
    union {
        struct {
            int vec4[4];
        };
        struct {
            int arvec2[2][2];
        };
        struct {
            int min[2];
            int max[2];
        };
        struct {
            int minX;
            int minY;
            int maxX;
            int maxY;
        };
    };
#ifdef __cplusplus
    aabox_s() : minX(DDMAXINT), minY(DDMAXINT), maxX(DDMININT), maxY(DDMININT) {}
    aabox_s(int _minX, int _minY, int _maxX, int _maxY) : minX(_minX), minY(_minY), maxX(_maxX), maxY(_maxY) {}

    aabox_s(const aabox_s &) = default;
    aabox_s &operator=(const aabox_s &) = default;

    aabox_s& clear()
    {
        minX = minY = DDMAXINT;
        maxX = maxY = DDMININT;
        return *this;
    }
#endif
} AABox;

/**
 * Axis-aligned bounding box with floating-point precision.
 * Handy POD structure for manipulation of bounding boxes.
 *
 * @ingroup legacyMath
 */
typedef struct aaboxf_s {
    union {
        struct {
            float vec4[4];
        };
        struct {
            float arvec2[2][2];
        };
        struct {
            float min[2];
            float max[2];
        };
        struct {
            float minX;
            float minY;
            float maxX;
            float maxY;
        };
    };
#ifdef __cplusplus
    aaboxf_s() : minX(DDMAXFLOAT), minY(DDMAXFLOAT), maxX(DDMINFLOAT), maxY(DDMINFLOAT) {}
    aaboxf_s(float _minX, float _minY, float _maxX, float _maxY) : minX(_minX), minY(_minY), maxX(_maxX), maxY(_maxY) {}

    aaboxf_s(const aaboxf_s &) = default;
    aaboxf_s &operator=(const aaboxf_s &) = default;

    aaboxf_s& clear()
    {
        minX = minY = DDMAXFLOAT;
        maxX = maxY = DDMINFLOAT;
        return *this;
    }
#endif
} AABoxf;

/**
 * Axis-aligned bounding box with double floating-point precision.
 * Handy POD structure for manipulation of bounding boxes.
 *
 * @ingroup legacyMath
 */
typedef struct aaboxd_s {
    union {
        struct {
            double vec4[4];
        };
        struct {
            double arvec2[2][2];
        };
        struct {
            double min[2];
            double max[2];
        };
        struct {
            double minX;
            double minY;
            double maxX;
            double maxY;
        };
    };
#ifdef __cplusplus
    aaboxd_s() : minX(DDMAXFLOAT), minY(DDMAXFLOAT), maxX(DDMINFLOAT), maxY(DDMINFLOAT) {}
    aaboxd_s(double _minX, double _minY, double _maxX, double _maxY) : minX(_minX), minY(_minY), maxX(_maxX), maxY(_maxY) {}

    aaboxd_s(const aaboxd_s &) = default;
    aaboxd_s &operator=(const aaboxd_s &) = default;
    
    aaboxd_s& clear()
    {
        minX = minY = DDMAXFLOAT;
        maxX = maxY = DDMINFLOAT;
        return *this;
    }

#endif
} AABoxd;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Check the spatial relationship between the given box and a partitioning line.
 *
 * @param box            Box being tested.
 * @param linePoint      Point on the line.
 * @param lineDirection  Direction of the line (slope).
 *
 * @return  @c <0= bbox is wholly on the left side.
 *          @c  0= line intersects bbox.
 *          @c >0= bbox wholly on the right side.
 */
DE_PUBLIC int M_BoxOnLineSide(const struct aaboxd_s* box, double const linePoint[2], double const lineDirection[2]);

DE_PUBLIC int M_BoxOnLineSide_FixedPrecision(const fixed_t box[], const fixed_t linePoint[], const fixed_t lineDirection[]);

/**
 * Check the spatial relationship between the given box and a partitioning line.
 *
 * An alternative version of M_BoxOnLineSide() which allows specifying many of the
 * intermediate values used in the calculation a priori for performance reasons.
 *
 * @param box            Box being tested.
 * @param linePoint      Point on the line.
 * @param lineDirection  Direction of the line (slope).
 * @param linePerp       Perpendicular distance of the line.
 * @param lineLength     Length of the line.
 * @param epsilon        Points within this distance will be considered equal.
 *
 * @return  @c <0= bbox is wholly on the left side.
 *          @c  0= line intersects bbox.
 *          @c >0= bbox wholly on the right side.
 */
DE_PUBLIC int M_BoxOnLineSide2(const struct aaboxd_s* box, double const linePoint[2],
    double const lineDirection[2], double linePerp, double lineLength, double epsilon);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // DE_AABOX_H
