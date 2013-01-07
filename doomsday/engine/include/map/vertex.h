/**
 * @file vertex.h
 * Logical map vertex. @ingroup map
 *
 * @authors Copyright &copy; 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright &copy; 2006-2013 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG_MAP_VERTEX
#define LIBDENG_MAP_VERTEX

#include "resource/r_data.h"
#include "map/p_dmu.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Count the total number of linedef "owners" of this vertex. An owner in
 * this context is any linedef whose start or end vertex is this.
 *
 * @pre Vertex linedef owner rings must have already been calculated.
 * @pre @a oneSided and/or @a twoSided must have already been initialized.
 *
 * @param vtx       Vertex instance.
 * @param oneSided  Total number of one-sided lines is written here. Can be @a NULL.
 * @param twoSided  Total number of two-sided lines is written here. Can be @a NULL.
 */
void Vertex_CountLineOwners(Vertex* vtx, uint* oneSided, uint* twoSided);

/**
 * Get a property value, selected by DMU_* name.
 *
 * @param vertex    Vertex instance.
 * @param args      Property arguments.
 * @return  Always @c 0 (can be used as an iterator).
 */
int Vertex_GetProperty(const Vertex* vertex, setargs_t* args);

/**
 * Update a property value, selected by DMU_* name.
 *
 * @param vertex    Vertex instance.
 * @param args      Property arguments.
 * @return  Always @c 0 (can be used as an iterator).
 */
int Vertex_SetProperty(Vertex* vertex, const setargs_t* args);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /// LIBDENG_MAP_VERTEX
