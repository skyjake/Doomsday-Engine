/**
 * @file smoother.h
 * Interpolator for smoothing out a movement curve.
 *
 * The original movement path is composed out of discrete 3D points. Smoother
 * calculates the points in between.
 *
 * @authors Copyright © 2011-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2013 Daniel Swanson <danij@dengine.net>
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

#ifndef DE_SMOOTHER_H
#define DE_SMOOTHER_H

#include <de/liblegacy.h>

#ifdef __cplusplus
extern "C" {
#endif

/// @addtogroup legacyMath
/// @{

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
DE_PUBLIC Smoother *Smoother_New();

/**
 * Destructs a smoother. This must be called when the smoother is no longer
 * needed.
 *
 * @param sm  Smoother instance.
 */
DE_PUBLIC void Smoother_Delete(Smoother *sm);

/**
 * If the difference between the smoother's past and now times is larger than
 * @a delta, the smoother gets automatically advanced to the present. This may
 * occur if the smoother keeps being advanced but no new values are inserted.
 * The assumption is that new values are being inserted at a semi-regular rate.
 */
DE_PUBLIC void Smoother_SetMaximumPastNowDelta(Smoother *sm, float delta);

/**
 * Resets the smoother instance. More than one discrete input point is needed before
 * output values can be calculated again. After calling Smoother_Clear() the state
 * of the Smoother is the same as right after construction.
 *
 * @param sm  Smoother instance.
 */
DE_PUBLIC void Smoother_Clear(Smoother *sm);

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
DE_PUBLIC void Smoother_AddPos(Smoother *sm, float time, coord_t x, coord_t y, coord_t z, dd_bool onFloor);

/**
 * Defines a new XY input point in the future of the smoother.
 *
 * @param sm    Smoother instance.
 * @param time  Point in time (game tick).
 * @param x     Cooordinate.
 * @param y     Cooordinate.
 */
DE_PUBLIC void Smoother_AddPosXY(Smoother *sm, float time, coord_t x, coord_t y);

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
DE_PUBLIC dd_bool Smoother_Evaluate(const Smoother *sm, coord_t *xyz);

/**
 * Calculates a coordinate for the current point in time.
 *
 * @param sm         Smoother instance.
 * @param component  The component to evaluate (0..2).
 * @param v          The evaluated coordinate value is written here. Must have room for 1 value.
 *
 * @return  @c true if the evaluation was successful. When @c false is returned,
 *          the value in @a v is not valid.
 *
 * @see Smoother_Advance()
 */
DE_PUBLIC dd_bool Smoother_EvaluateComponent(const Smoother *sm, int component, coord_t *v);

/**
 * Determines whether the smoother's Z coordinate is currently on the floor plane.
 * @param sm    Smoother instance.
 */
DE_PUBLIC dd_bool Smoother_IsOnFloor(const Smoother *sm);

/**
 * Determines whether the smoother is currently undergoing movement.
 * @param sm    Smoother instance.
 */
DE_PUBLIC dd_bool Smoother_IsMoving(const Smoother *sm);

/**
 * Advances the smoother @a sm by @a period amount of time.
 * @param sm    Smoother instance.
 * @param period  Amount of time to advance the Smoother's current time.
 */
DE_PUBLIC void Smoother_Advance(Smoother *sm, float period);

void Smoother_Debug(const Smoother *sm);

/// @}

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* DE_SMOOTHER_H */
