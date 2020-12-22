/** @file edit_map.h Internal runtime map editing interface.
 *
 * @authors Copyright © 2007-2013 Daniel Swanson <danij@dengine.net>
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

#ifndef DE_WORLD_EDITMAP_H
#define DE_WORLD_EDITMAP_H

#include <doomsday/world/map.h>

/**
 * Provides access to the current map being built with the runtime map editing
 * interface. If no map is currently being built then @c 0 is returned. Ownership
 * of the map is @em NOT given to the caller.
 *
 * @see MPE_TakeMap()
 */
world::Map *MPE_Map();

/**
 * Take ownership of the last map built with the runtime map editing interface.
 * May return @c 0 if no such map exists.
 */
world::Map *MPE_TakeMap();

#endif  // DE_WORLD_EDITMAP_H
