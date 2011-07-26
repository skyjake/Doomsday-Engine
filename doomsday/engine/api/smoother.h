/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2011 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef __DOOMSDAY_SMOOTHER_H__
#define __DOOMSDAY_SMOOTHER_H__

#ifdef __cplusplus
extern "C" {
#endif

struct smoother_s; // The smoother instance (opaque).
typedef struct smoother_s Smoother;

/**
 * Construct a new smoother instance.
 *
 * @return  The smoother.
 */
Smoother* Smoother_New();

/**
 * Destructs a smoother. This must be called when the smoother is no
 * longer needed.
 */
void Smoother_Destruct(Smoother* sm);

/**
 * Resets the smoother instance.
 */
void Smoother_Clear(Smoother* sm);

/**
 * Defines a new point in the future of the smoother.
 *
 * @param sm    Smoother instance.
 * @param time  Point in time (game tick).
 * @param x     Cooordinate.
 * @param y     Cooordinate.
 * @param z     Cooordinate.
 * @param onFloor  @c true if the z coordinate should be on the floor plane.
 */
void Smoother_AddPos(Smoother* sm, float time, float x, float y, float z, boolean onFloor);

/**
 * Calculates the coordinates for the current point in time.
 *
 * @param sm   Smoother instance.
 * @param xyz  The coordinates are written here. Must have room for 3 values.
 *
 * @return  @c true if the evaluation was successful. When @c false is returned,
 *          the values in @a xyz are not valid.
 */
boolean Smoother_Evaluate(const Smoother* sm, float* xyz);

/**
 * Determines whether the smoother's Z coordinate is currently on the floor plane.
 */
boolean Smoother_IsOnFloor(const Smoother* sm);

/**
 * Determines whether the smoother is currently undergoing movement.
 */
boolean Smoother_IsMoving(const Smoother* sm);

/**
 * Advances the smoother @a sm by @a period amount of time.
 */
void Smoother_Advance(Smoother* sm, float period);

//void Smoother_Debug(const Smoother* sm);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // __DOOMSDAY_SMOOTHER_H__
