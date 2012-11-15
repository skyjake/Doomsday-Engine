/**
 * @file vanillablockmap.h
 * Blockmap functionality guaranteed to work exactly like in the original DOOM.
 * @ingroup vanilla
 *
 * @authors Copyright (c) 2012 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBCOMMON_VANILLABLOCKMAP_H
#define LIBCOMMON_VANILLABLOCKMAP_H

#include "common.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Loads the original blockmap data for a map.
 *
 * @param mapId  Id of a map (e.g., "E1M2").
 *
 * @return @c true, if the blockmap data was successfully found and loaded.
 * Otherwise, @c false.
 */
boolean Vanilla_LoadBlockmap(const char* mapId);

boolean Vanilla_IsBlockmapAvailable();

/**
 * Iterates through all the lines inside the bounding box using the original
 * vanilla data. Validcount should be incremented prior to calling this.
 *
 * @param box   Bounding box in which to check lines.
 * @param func  Callback to call for each line.
 *
 * @return @c false, if the iteration was cancelled before all lines were
 * checked. @c true, if iteration completed through all lines in the box.
 */
boolean Vanilla_BlockLinesIterator(const AABoxd* box, int (*func)(LineDef*, void*));

#ifdef __cplusplus
} // extern "C"
#endif

#endif //LIBCOMMON_VANILLABLOCKMAP_H
