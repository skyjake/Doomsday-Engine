/**
 * @file plane.h
 * Map Plane. @ingroup map
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

#ifndef LIBDENG_MAP_PLANE
#define LIBDENG_MAP_PLANE

#include "resource/r_data.h"
#include "map/p_dmu.h"

#ifdef __cplusplus
extern "C" {
#endif

// Return the index of plane within a sector's planes array.
#define GET_PLANE_IDX(pln)      ( (int) ((pln) - (pln)->sector->planes[0]) )

/**
 * Get a property value, selected by DMU_* name.
 *
 * @param plane  Plane instance.
 * @param args  Property arguments.
 * @return  Always @c 0 (can be used as an iterator).
 */
int Plane_GetProperty(const Plane* plane, setargs_t* args);

/**
 * Update a property value, selected by DMU_* name.
 *
 * @param plane  Plane instance.
 * @param args  Property arguments.
 * @return  Always @c 0 (can be used as an iterator).
 */
int Plane_SetProperty(Plane* plane, const setargs_t* args);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /// LIBDENG_MAP_PLANE
