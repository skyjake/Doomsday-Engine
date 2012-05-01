/**
 * @file smoother.h
 * Interpolator for smoothing out a movement curve. @ingroup math
 *
 * The original movement path is composed out of discrete 3D points. Smoother
 * calculates the points in between.
 *
 * @authors Copyright © 2011-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2012 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG_SMOOTHER_H
#define LIBDENG_SMOOTHER_H

#include "dd_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SMOOTHER_MOVE_EPSILON           .001

struct smoother_s; // The smoother instance (opaque).

/**
 * Smoother instance. Use Smoother_New() to construct.
 */
typedef struct smoother_s Smoother;

/**
 * Construct a new smoother instance.
 *
 * @return  Smoother instance.
 */
Smoother* Smoother_New();

/**
 * Destructs a smoother. This must be called when the smoother is no longer
 * needed.
 *
 * @param sm  Smoother instance.
 */
void Smoother_Delete(Smoother* sm);

/**
 * Resets the smoother instance. More than one discrete input point is needed before
 * output values can be calculated again. After calling Smoother_Clear() the state
 * of the Smoother is the same as right after construction.
 *
 * @param sm  Smoother instance.
 */
void Smoother_Clear(Smoother* sm);

/**
 * Defines a new input point in the future of the smoother.
 *
 * @param sm    Smoother instance.
 * @param time  Point in time (game tick).
 * @param x     Cooordinate.
 * @param y     Cooordinate.
 * @param z     Cooordinate.
 * @param onFloor  @c true if the z coordinate should be on the floor plane.
 */
void Smoother_AddPos(Smoother* sm, float time, coord_t x, coord_t y, coord_t z, boolean onFloor);

/**
 * Calculates the coordinates for the current point in time.
 *
 * @param sm   Smoother instance.
 * @param xyz  The coordinates are written here. Must have room for 3 values.
 *
 * @return  @c true if the evaluation was successful. When @c false is returned,
 *          the values in @a xyz are not valid.
 *
 * @see Smoother_Advance()
 */
boolean Smoother_Evaluate(const Smoother* sm, coord_t* xyz);

/**
 * Determines whether the smoother's Z coordinate is currently on the floor plane.
 * @param sm    Smoother instance.
 */
boolean Smoother_IsOnFloor(const Smoother* sm);

/**
 * Determines whether the smoother is currently undergoing movement.
 * @param sm    Smoother instance.
 */
boolean Smoother_IsMoving(const Smoother* sm);

/**
 * Advances the smoother @a sm by @a period amount of time.
 * @param sm    Smoother instance.
 * @param period  Amount of time to advance the Smoother's current time.
 */
void Smoother_Advance(Smoother* sm, float period);

void Smoother_Debug(const Smoother* sm);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* LIBDENG_SMOOTHER_H */
