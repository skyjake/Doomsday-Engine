/** @file contactspreader.h  Map object => BSP leaf "contact" spreader.
 * @ingroup world
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2016 Daniel Swanson <danij@dengine.net>
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

#ifdef __CLIENT__
#ifndef DE_CLIENT_WORLD_CONTACTSPREADER_H
#define DE_CLIENT_WORLD_CONTACTSPREADER_H

#include <doomsday/world/blockmap.h>
#include <de/legacy/aabox.h>
#include <de/bitarray.h>

/**
 * Performs contact spreading for the specified @a blockmap.
 */
void spreadContacts(const world::Blockmap &blockmap, const AABoxd &region, de::BitArray *spreadBlocks = 0);

#endif  // DE_CLIENT_WORLD_CONTACTSPREADER_H
#endif  // __CLIENT__
