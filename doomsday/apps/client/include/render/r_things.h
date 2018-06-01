/** @file r_things.h  Map Object => Vissprite Projection.
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
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#ifdef __CLIENT__
#ifndef DE_CLIENT_RENDER_THINGS_H
#define DE_CLIENT_RENDER_THINGS_H

#include "world/p_object.h"

/**
 * Generates vissprite(s) for given map-object if they might be visible.
 */
void R_ProjectSprite(mobj_t &mob);

#endif  // DE_CLIENT_RENDER_THINGS_H
#endif  // __CLIENT__
