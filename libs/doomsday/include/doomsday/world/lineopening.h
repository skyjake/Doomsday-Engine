/** @file
 *
 * @authors Copyright (c) 2020 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "../libdoomsday.h"
#include "../api_map.h"

/**
 * Describes the @em sharp coordinates of the opening between sectors which
 * interface at a given map line. The open range is defined as the gap between
 * foor and ceiling on the front side clipped by the floor and ceiling planes on
 * the back side (if present).
 */
struct LIBDOOMSDAY_PUBLIC lineopening_s
{
    /// Top and bottom z of the opening.
    float top, bottom;

    /// Distance from top to bottom.
    float range;

    /// Z height of the lowest Plane at the opening on the X|Y axis.
    /// @todo Does not belong here?
    float lowFloor;

#ifdef __cplusplus
    lineopening_s() : top(0), bottom(0), range(0), lowFloor(0) {}
    lineopening_s(const world_Line &line);
    lineopening_s &operator=(const lineopening_s &other);
#endif
};

typedef struct lineopening_s LineOpening;
