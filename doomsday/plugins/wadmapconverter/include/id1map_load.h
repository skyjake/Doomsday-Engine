/**
 * @file id1map_load.h @ingroup wadmapconverter
 *
 * id tech 1 map data definition loaders.
 *
 * @authors Copyright &copy; 2007-2012 Daniel Swanson <danij@dengine.net>
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

#ifndef __WADMAPCONVERTER_ID1MAP_LOAD_H__
#define __WADMAPCONVERTER_ID1MAP_LOAD_H__

#include "doomsday.h"
#include "maplumpinfo.h"
#include "id1map_datatypes.h"

/**
 * Determine the size (in bytes) of an element of the specified map data
 * lump @a type for the current map format.
 *
 * @param type  Map lump data type.
 * @return Size of an element of the specified type.
 */
size_t ElementSizeForMapLumpType(MapLumpType type);

/**
 * Read a line definition from the archived map.
 *
 * @param line  Line definition to be populated.
 * @param reader  Reader instance.
 */
void MLine_Read(mline_t* line, Reader* reader);

/// Doom64 format variant of @ref MLine_Read()
void MLine64_Read(mline_t* line, Reader* reader);

/// Hexen format variant of @ref MLine_Read()
void MLineHx_Read(mline_t* line, Reader* reader);

/**
 * Read a side definition from the archived map.
 *
 * @param side  Side definition to be populated.
 * @param reader  Reader instance.
 */
void MSide_Read(mside_t* side, Reader* reader);

/// Doom64 format variant of @ref MSide_Read()
void MSide64_Read(mside_t* side, Reader* reader);

/**
 * Read a sector definition from the archived map.
 *
 * @param sector  Sector definition to be populated.
 * @param reader  Reader instance.
 */
void MSector_Read(msector_t* sector, Reader* reader);

/// Doom64 format variant of @ref MSector_Read()
void MSector64_Read(msector_t* sector, Reader* reader);

/**
 * Read a thing definition from the archived map.
 *
 * @param thing  Thing definition to be populated.
 * @param reader  Reader instance.
 */
void MThing_Read(mthing_t* thing, Reader* reader);

/// Doom64 format variant of @ref MThing_Read()
void MThing64_Read(mthing_t* thing, Reader* reader);

/// Hexen format variant of @ref MThing_Read()
void MThingHx_Read(mthing_t* thing, Reader* reader);

/**
 * Read a surface tint definition from the archived map.
 *
 * @param tint  Surface tint definition to be populated.
 * @param reader  Reader instance.
 */
void SurfaceTint_Read(surfacetint_t* tint, Reader* reader);

#endif /* __WADMAPCONVERTER_ID1MAP_LOAD_H__ */
