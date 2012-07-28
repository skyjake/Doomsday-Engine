/**
 * @file id1map_load.h @ingroup wadmapconverter
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
#include "id1map_datatypes.h"

void MLine_Read(mline_t* l, Reader* reader);
void MLine64_Read(mline_t* l, Reader* reader);
void MLineHx_Read(mline_t* l, Reader* reader);
void MSide_Read(mside_t* s, Reader* reader);
void MSide64_Read(mside_t* s, Reader* reader);
void MSector_Read(msector_t* s, Reader* reader);
void MSector64_Read(msector_t* s, Reader* reader);
void MThing_Read(mthing_t* t, Reader* reader);
void MThing64_Read(mthing_t* t, Reader* reader);
void MThingHx_Read(mthing_t* t, Reader* reader);
void SurfaceTint_Read(surfacetint_t* t, Reader* reader);

#endif /* __WADMAPCONVERTER_ID1MAP_LOAD_H__ */
