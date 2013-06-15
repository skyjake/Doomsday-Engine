/** @file p_intercept.h World map line / object interception.
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

#ifndef DENG_WORLD_P_INTERCEPT_H
#define DENG_WORLD_P_INTERCEPT_H

class Line;

struct InterceptNode; // The interceptnode instance (opaque).
typedef struct InterceptNode InterceptNode;

/**
 * Empties the intercepts array and makes sure it has been allocated.
 */
void P_ClearIntercepts();

/**
 * You must clear intercepts before the first time this is called.
 * The intercepts array grows if necessary.
 *
 * @param type      Type of interception.
 * @param distance  Distance along the trace vector that the interception occured [0...1].
 * @param object    Object being intercepted.
 *
 * @return  Newly added intercept or @c NULL if outside the trace range.
 */
InterceptNode *P_AddIntercept(intercepttype_t type, float distance, void *object);

/**
 * @return  Zero if the traverser callback returns zero for all processed intercepts.
 */
int P_TraverseIntercepts(traverser_t callback, void *parameters);

/// @todo Find a better home for the following functions ----------------------

int PIT_AddLineIntercepts(Line *ld, void *parameters);

int PIT_AddMobjIntercepts(struct mobj_s *mobj, void *parameters);

#endif // DENG_WORLD_P_INTERCEPT_H
