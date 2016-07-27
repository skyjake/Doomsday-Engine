/** @file m_misc.h
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
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

/**
 * Miscellanous Routines.
 */

#ifndef LIBDENG_M_MISC_H
#define LIBDENG_M_MISC_H

#include <stdio.h>

#include "dd_types.h"
#include <de/vector1.h>

#ifdef __cplusplus
extern "C" {
#endif

struct aaboxd_s;

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
int M_BoxOnLineSide(const struct aaboxd_s* box, double const linePoint[2], double const lineDirection[2]);

int M_BoxOnLineSide_FixedPrecision(const fixed_t box[], const fixed_t linePoint[], const fixed_t lineDirection[]);

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
int M_BoxOnLineSide2(const struct aaboxd_s* box, double const linePoint[2],
    double const lineDirection[2], double linePerp, double lineLength, double epsilon);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // LIBDENG_M_MISC_H
